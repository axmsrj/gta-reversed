#pragma once

#include "ColArchive.h"

#include <array>
#include <optional>

namespace gtasa::collision {

// Returns the highest collision triangle below the query point. The quaternion
// convention matches the IPL transform used for rendering world instances.
std::optional<float> GroundHeight(const Mesh& mesh,
                                  const std::array<float,3>& position,
                                  const std::array<float,4>& rotation,
                                  float x,float y,float maxZ);

} // namespace gtasa::collision
