#include "RwChunks.h"

#include <stdexcept>

namespace gtasa::assets {
namespace {
std::uint32_t Read32(std::span<const std::uint8_t> bytes, std::size_t at) {
    return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
           (std::uint32_t(bytes[at + 2]) << 16) | (std::uint32_t(bytes[at + 3]) << 24);
}
}

RwChunk ReadRwChunk(std::span<const std::uint8_t> bytes, std::size_t offset,
                    std::size_t bound) {
    if (bound > bytes.size() || offset > bound || bound - offset < 12) {
        throw std::runtime_error("Truncated RenderWare chunk header");
    }
    const auto length = Read32(bytes, offset + 4);
    const auto dataOffset = offset + 12;
    if (length > bound - dataOffset) {
        throw std::runtime_error("RenderWare chunk exceeds containing entry");
    }
    return {Read32(bytes, offset), length, Read32(bytes, offset + 8),
            dataOffset, dataOffset + length};
}

std::vector<RwChunk> ReadRwChildren(std::span<const std::uint8_t> bytes,
                                    const RwChunk& parent) {
    if (parent.dataOffset > parent.endOffset || parent.endOffset > bytes.size()) {
        throw std::runtime_error("Invalid RenderWare parent bounds");
    }
    std::vector<RwChunk> children;
    auto offset = parent.dataOffset;
    while (offset < parent.endOffset) {
        const auto child = ReadRwChunk(bytes, offset, parent.endOffset);
        children.push_back(child);
        offset = child.endOffset;
    }
    return children;
}
}
