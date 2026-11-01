#pragma once

#include "PedIfp.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace gtasa::animation {

struct BonePose {
    std::uint32_t boneTag{};
    std::array<float,4> rotation{};
    std::array<float,3> translation{};
};

std::vector<BonePose> BlendPoses(const std::vector<BonePose>& first,
                                const std::vector<BonePose>& second,float weight);

class ClipPlayback {
public:
    ClipPlayback(std::span<const std::uint8_t> bytes,const ClipInfo& clip);
    float Duration() const { return duration_; }
    std::optional<float> RootTravelSpeed() const;
    std::optional<float> RootTravelDistance(float fromSeconds,float toSeconds) const;
    std::vector<BonePose> Sample(float seconds) const;

private:
    struct Track {
        std::uint32_t boneTag{};
        std::vector<KeyFrame> frames;
        std::vector<float> times;
    };
    std::vector<Track> tracks_;
    float duration_{};
};

} // namespace gtasa::animation
