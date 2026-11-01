#include "PedIfp.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gtasa::animation {
namespace {

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::size_t Offset() const { return offset_; }
    std::size_t Remaining() const { return bytes_.size()-offset_; }
    std::span<const std::uint8_t> Take(std::size_t count) {
        if (count>Remaining()) throw std::runtime_error("Truncated ANP3 data");
        auto result=bytes_.subspan(offset_,count);
        offset_+=count;
        return result;
    }
    std::uint32_t U32() {
        const auto value=Take(4);
        return std::uint32_t(value[0]) | std::uint32_t(value[1])<<8 |
            std::uint32_t(value[2])<<16 | std::uint32_t(value[3])<<24;
    }
    float F32() { return std::bit_cast<float>(U32()); }
    std::int16_t I16() {
        const auto value=Take(2);
        return static_cast<std::int16_t>(std::uint16_t(value[0]) |
                                         std::uint16_t(value[1])<<8);
    }
    std::string Name() {
        const auto value=Take(24);
        const auto end=std::find(value.begin(),value.end(),0);
        if (end==value.end()) throw std::runtime_error("Unterminated ANP3 name");
        return {reinterpret_cast<const char*>(value.data()),
                static_cast<std::size_t>(end-value.begin())};
    }
private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

std::size_t FrameSize(std::uint32_t type) {
    switch (type) {
    case 1: return 20;
    case 2: return 32;
    case 3: return 10;
    case 4: return 16;
    default: throw std::runtime_error("Invalid ANP3 frame type");
    }
}

} // namespace

IfpCatalog ReadAnp3(std::span<const std::uint8_t> bytes) {
    Reader reader(bytes);
    const auto signature=reader.Take(4);
    if (signature[0]!='A' || signature[1]!='N' || signature[2]!='P' || signature[3]!='3')
        throw std::runtime_error("Unsupported IFP signature");
    const auto payloadSize=reader.U32();
    if (payloadSize!=reader.Remaining())
        throw std::runtime_error("ANP3 payload size mismatch");
    IfpCatalog catalog;
    catalog.blockName=reader.Name();
    const auto clipCount=reader.U32();
    if (clipCount>10000 || clipCount>reader.Remaining()/36)
        throw std::runtime_error("Invalid ANP3 clip count");
    catalog.clips.reserve(clipCount);
    for (std::uint32_t i=0;i<clipCount;++i) {
        ClipInfo clip;
        clip.name=reader.Name();
        const auto sequenceCount=reader.U32();
        const auto frameBytes=reader.U32();
        clip.flags=reader.U32();
        if (sequenceCount>512 || sequenceCount>reader.Remaining()/36 ||
            frameBytes>reader.Remaining())
            throw std::runtime_error("Invalid ANP3 clip bounds");
        clip.sequences.reserve(sequenceCount);
        std::size_t actualBytes=0;
        for (std::uint32_t j=0;j<sequenceCount;++j) {
            SequenceInfo sequence;
            sequence.name=reader.Name();
            sequence.frameType=reader.U32();
            sequence.frameCount=reader.U32();
            sequence.boneTag=reader.U32();
            const auto stride=FrameSize(sequence.frameType);
            if (sequence.frameCount>reader.Remaining()/stride ||
                sequence.frameCount>(std::numeric_limits<std::size_t>::max()-actualBytes)/stride)
                throw std::runtime_error("Invalid ANP3 frame count");
            const auto size=std::size_t(sequence.frameCount)*stride;
            sequence.framesOffset=reader.Offset();
            reader.Take(size);
            actualBytes+=size;
            clip.sequences.push_back(std::move(sequence));
        }
        if (actualBytes!=frameBytes)
            throw std::runtime_error("ANP3 frame allocation size mismatch");
        catalog.clips.push_back(std::move(clip));
    }
    if (reader.Remaining()!=0) throw std::runtime_error("Trailing ANP3 data");
    return catalog;
}

std::vector<KeyFrame> DecodeFrames(std::span<const std::uint8_t> bytes,
                                   const SequenceInfo& sequence) {
    const auto stride=FrameSize(sequence.frameType);
    if (sequence.framesOffset>bytes.size() ||
        sequence.frameCount>(bytes.size()-sequence.framesOffset)/stride)
        throw std::runtime_error("ANP3 frame range outside file");
    Reader reader(bytes.subspan(sequence.framesOffset,std::size_t(sequence.frameCount)*stride));
    std::vector<KeyFrame> frames;
    frames.reserve(sequence.frameCount);
    for (std::uint32_t i=0;i<sequence.frameCount;++i) {
        KeyFrame frame;
        if (sequence.frameType<=2) {
            for (auto& component : frame.rotation) component=reader.F32();
            frame.deltaTime=reader.F32();
            if (sequence.frameType==2)
                for (auto& component : frame.translation) component=reader.F32();
        } else {
            for (auto& component : frame.rotation) component=reader.I16()/4096.0f;
            frame.deltaTime=reader.I16()/60.0f;
            if (sequence.frameType==4)
                for (auto& component : frame.translation) component=reader.I16()/1024.0f;
        }
        for (const auto component : frame.rotation)
            if (!std::isfinite(component)) throw std::runtime_error("Nonfinite ANP3 rotation");
        for (const auto component : frame.translation)
            if (!std::isfinite(component)) throw std::runtime_error("Nonfinite ANP3 translation");
        if (!std::isfinite(frame.deltaTime) || frame.deltaTime<0.0f)
            throw std::runtime_error("Invalid ANP3 frame time");
        frames.push_back(frame);
    }
    return frames;
}

} // namespace gtasa::animation
