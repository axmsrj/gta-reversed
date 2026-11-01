#include "ColArchive.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <cmath>
#include <stdexcept>

namespace gtasa::collision {
namespace {
std::uint16_t U16(std::span<const std::uint8_t> bytes,std::size_t at) {
    if (at>bytes.size() || bytes.size()-at<2) throw std::runtime_error("Truncated COL field");
    return std::uint16_t(bytes[at]) | (std::uint16_t(bytes[at+1])<<8);
}

std::uint32_t U32(std::span<const std::uint8_t> bytes,std::size_t at) {
    if (at>bytes.size() || bytes.size()-at<4) throw std::runtime_error("Truncated COL field");
    return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at+1])<<8) |
           (std::uint32_t(bytes[at+2])<<16) | (std::uint32_t(bytes[at+3])<<24);
}

float F32(std::span<const std::uint8_t> bytes,std::size_t at) {
    const float value=std::bit_cast<float>(U32(bytes,at));
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite COL volume");
    return value;
}

bool Range(std::size_t offset,std::size_t count,std::size_t stride,std::size_t size) {
    return offset<=size && count<=(size-offset)/stride;
}
}

std::vector<Mesh> ParseColArchive(std::span<const std::uint8_t> bytes) {
    if (bytes.size()<8) throw std::runtime_error("Truncated COL archive");
    std::vector<Mesh> meshes;
    std::size_t start=0;
    while (start+8<=bytes.size()) {
        const auto signature=bytes.subspan(start,4);
        const bool col2=std::memcmp(signature.data(),"COL2",4)==0;
        const bool col3=std::memcmp(signature.data(),"COL3",4)==0;
        if (!col2 && !col3) {
            if (meshes.empty()) throw std::runtime_error("No COL2/COL3 records");
            if (std::any_of(bytes.begin()+start,bytes.end(),
                            [](std::uint8_t value) { return value!=0; }))
                throw std::runtime_error("Nonzero data after COL records");
            break; // IMG sector padding.
        }
        const auto payload=U32(bytes,start+4);
        const std::size_t total=std::size_t(payload)+8;
        const std::size_t headerSize=col3 ? 120 : 108;
        if (total<headerSize || total>bytes.size()-start)
            throw std::runtime_error("COL record exceeds entry bounds");
        const auto record=bytes.subspan(start,total);
        Mesh mesh;
        const auto name=record.subspan(8,22);
        const auto end=std::find(name.begin(),name.end(),0);
        mesh.name.assign(reinterpret_cast<const char*>(name.data()),
                         static_cast<std::size_t>(end-name.begin()));
        mesh.fileModelId=U16(record,30);
        const std::size_t sphereCount=U16(record,72);
        const std::size_t boxCount=U16(record,74);
        const std::size_t faceCount=U16(record,76);
        const auto sphereOffset=U32(record,84);
        const auto boxOffset=U32(record,88);
        const auto vertexOffset=U32(record,96);
        const auto faceOffset=U32(record,100);
        if (sphereCount) {
            if (!sphereOffset || sphereCount>10000 ||
                !Range(std::size_t(sphereOffset)+4,sphereCount,20,total))
                throw std::runtime_error("Invalid COL sphere bounds");
            mesh.spheres.reserve(sphereCount);
            for (std::size_t i=0;i<sphereCount;++i) {
                const auto at=std::size_t(sphereOffset)+4+i*20;
                Sphere sphere{{F32(record,at),F32(record,at+4),F32(record,at+8)},
                              F32(record,at+12),record[at+16]};
                if (sphere.radius<0.0f)
                    throw std::runtime_error("Invalid COL sphere radius");
                mesh.spheres.push_back(sphere);
            }
        }
        if (boxCount) {
            if (!boxOffset || boxCount>10000 ||
                !Range(std::size_t(boxOffset)+4,boxCount,28,total))
                throw std::runtime_error("Invalid COL box bounds");
            mesh.boxes.reserve(boxCount);
            for (std::size_t i=0;i<boxCount;++i) {
                const auto at=std::size_t(boxOffset)+4+i*28;
                Box box{{F32(record,at),F32(record,at+4),F32(record,at+8)},
                        {F32(record,at+12),F32(record,at+16),F32(record,at+20)},
                        record[at+24]};
                for (int axis=0;axis<3;++axis)
                    if (box.minimum[axis]>box.maximum[axis])
                        throw std::runtime_error("Invalid COL box extent");
                mesh.boxes.push_back(box);
            }
        }
        if (faceCount) {
            if (!vertexOffset || !faceOffset || vertexOffset>=faceOffset ||
                !Range(std::size_t(faceOffset)+4,faceCount,8,total))
                throw std::runtime_error("Invalid COL triangle offsets");
            mesh.triangles.reserve(faceCount);
            std::uint16_t highestVertex=0;
            for (std::size_t i=0;i<faceCount;++i) {
                const auto at=std::size_t(faceOffset)+4+i*8;
                Triangle triangle{{U16(record,at),U16(record,at+2),U16(record,at+4)},
                                  record[at+6],record[at+7]};
                highestVertex=std::max({highestVertex,triangle.indices[0],
                                        triangle.indices[1],triangle.indices[2]});
                mesh.triangles.push_back(triangle);
            }
            const std::size_t vertexCount=std::size_t(highestVertex)+1;
            const std::size_t vertexAt=std::size_t(vertexOffset)+4;
            if (!Range(vertexAt,vertexCount,6,total) ||
                vertexCount>(std::size_t(faceOffset)-vertexOffset)/6)
                throw std::runtime_error("COL triangle index exceeds vertex data");
            mesh.vertices.reserve(vertexCount);
            for (std::size_t i=0;i<vertexCount;++i) {
                const auto at=vertexAt+i*6;
                mesh.vertices.push_back({std::bit_cast<std::int16_t>(U16(record,at))/128.0f,
                                         std::bit_cast<std::int16_t>(U16(record,at+2))/128.0f,
                                         std::bit_cast<std::int16_t>(U16(record,at+4))/128.0f});
            }
        }
        meshes.push_back(std::move(mesh));
        start+=total;
    }
    if (start<bytes.size() &&
        std::any_of(bytes.begin()+start,bytes.end(),
                    [](std::uint8_t value) { return value!=0; }))
        throw std::runtime_error("Nonzero data after COL records");
    return meshes;
}

} // namespace gtasa::collision
