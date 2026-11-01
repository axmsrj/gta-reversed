#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gtasa::assets {
// FourCC values from D3D native TXD rasters. Returns an empty vector for
// unsupported/truncated input or invalid dimensions.
std::vector<std::uint8_t> DecodeDxt(const std::uint8_t* blocks, std::size_t size,
                                    int width, int height, std::uint32_t fourcc);
}
