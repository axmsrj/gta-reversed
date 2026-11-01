#pragma once

#include "ColArchive.h"

#include <array>

namespace gtasa::collision {

// Sweeps two body-height spheres along a short walking step against COL boxes,
// spheres and triangles. Points are feet positions in world space.
bool BlocksVolumes(const Mesh& mesh,const std::array<float,3>& instancePosition,
                   const std::array<float,4>& instanceRotation,
                   std::array<float,3> fromFeet,std::array<float,3> toFeet,
                   float radius=0.32f);

} // namespace gtasa::collision
