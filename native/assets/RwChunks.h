#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gtasa::assets {
struct RwChunk {
    std::uint32_t type;
    std::uint32_t length;
    std::uint32_t libraryId;
    std::size_t dataOffset;
    std::size_t endOffset;
};

RwChunk ReadRwChunk(std::span<const std::uint8_t> bytes, std::size_t offset,
                    std::size_t bound);
std::vector<RwChunk> ReadRwChildren(std::span<const std::uint8_t> bytes,
                                    const RwChunk& parent);
}
