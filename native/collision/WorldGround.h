#pragma once

#include "CollisionCatalog.h"
#include "Placement.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace gtasa::collision {

struct GroundHit {
    float height{};
    std::int32_t modelId{};
    std::string modelName;
};

std::optional<GroundHit> GroundAt(const world::Region& world,
                                  const CollisionCatalog& collision,
                                  std::array<float,3> point,
                                  float searchRadius=250.0f);

bool BlockedByWorldGeometry(const world::Region& world,
                            const CollisionCatalog& collision,
                            std::array<float,3> fromFeet,
                            std::array<float,3> toFeet,
                            float searchRadius=80.0f);

} // namespace gtasa::collision
