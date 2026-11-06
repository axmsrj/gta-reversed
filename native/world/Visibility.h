#pragma once

#include <array>
#include <cmath>

namespace gtasa::world {

// CRenderer::SetupMapEntityVisibility keeps a loaded map object in its
// distance-fade band while distance - MAX_FADING_DISTANCE < draw distance.
inline bool WithinLoadedDrawDistance(float drawDistance,
                                     std::array<float,3> camera,
                                     std::array<float,3> origin) {
    constexpr float fadingDistance=20.0f;
    if (!std::isfinite(drawDistance) || drawDistance<0.0f) return false;
    const double dx=double(camera[0])-origin[0];
    const double dy=double(camera[1])-origin[1];
    const double dz=double(camera[2])-origin[2];
    return std::sqrt(dx*dx+dy*dy+dz*dz)-fadingDistance<drawDistance;
}

} // namespace gtasa::world
