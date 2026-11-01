#include "WorldGround.h"
#include "GroundQuery.h"
#include "VolumeQuery.h"

namespace gtasa::collision {

std::optional<GroundHit> GroundAt(const world::Region& world,
                                  const CollisionCatalog& collision,
                                  std::array<float,3> point,
                                  float searchRadius) {
    std::optional<GroundHit> best;
    for (const auto* instance : world::InstancesNear(world,point,searchRadius)) {
        const auto& type=world.types.at(instance->modelId);
        const auto* mesh=collision.Find(type.model);
        if (!mesh) continue;
        const auto height=GroundHeight(*mesh,instance->position,instance->rotation,
                                       point[0],point[1],point[2]);
        if (height && (!best || *height>best->height))
            best=GroundHit{*height,instance->modelId,type.model};
    }
    return best;
}

bool BlockedByWorldGeometry(const world::Region& world,
                            const CollisionCatalog& collision,
                            std::array<float,3> fromFeet,
                            std::array<float,3> toFeet,float searchRadius) {
    for (const auto* instance : world::InstancesNear(world,fromFeet,searchRadius)) {
        const auto& type=world.types.at(instance->modelId);
        const auto* mesh=collision.Find(type.model);
        if (mesh && BlocksVolumes(*mesh,instance->position,instance->rotation,
                                  fromFeet,toFeet)) return true;
    }
    return false;
}

} // namespace gtasa::collision
