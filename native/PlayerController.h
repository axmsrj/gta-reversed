#pragma once

#include <array>
#include <functional>
#include <optional>

namespace gtasa::native {

class PlayerController {
public:
    using GroundProbe=std::function<std::optional<float>(float,float,float)>;
    using ObstacleProbe=std::function<bool(std::array<float,3>,std::array<float,3>)>;

    PlayerController(std::array<float,3> rootPosition,float footOffset,
                     float walkingSpeed=1.55f)
        : position_(rootPosition),footOffset_(footOffset),walkingSpeed_(walkingSpeed) {}

    const std::array<float,3>& Position() const { return position_; }
    float Heading() const { return heading_; }

    // Ground-constrained walking with axis-wise wall sliding. Falling and full
    // slope/material responses remain separate work.
    bool Step(float seconds,float worldX,float worldY,const GroundProbe& ground,
              const ObstacleProbe& blocked={},std::optional<float> travelDistance={});

private:
    std::array<float,3> position_{};
    float footOffset_{};
    float walkingSpeed_{};
    float heading_{};
};

} // namespace gtasa::native
