#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace gtasa::animation {

struct SequenceInfo {
    std::string name;
    std::uint32_t boneTag{};
    std::uint32_t frameType{};
    std::uint32_t frameCount{};
    std::size_t framesOffset{};
};

struct ClipInfo {
    std::string name;
    std::uint32_t flags{};
    std::vector<SequenceInfo> sequences;
};

struct IfpCatalog {
    std::string blockName;
    std::vector<ClipInfo> clips;
};

struct KeyFrame {
    std::array<float,4> rotation{};
    std::array<float,3> translation{};
    float deltaTime{};
};

// ANP3 metadata and exact frame ranges. Frame values are decoded separately.
IfpCatalog ReadAnp3(std::span<const std::uint8_t> bytes);
std::vector<KeyFrame> DecodeFrames(std::span<const std::uint8_t> bytes,
                                   const SequenceInfo& sequence);

} // namespace gtasa::animation
