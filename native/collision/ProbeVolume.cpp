#include "CollisionCatalog.h"
#include "VolumeQuery.h"
#include "Placement.h"
#include "ImgDirectory.h"

#include <array>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

int main(int argc,const char* argv[]) {
    if (argc!=9) {
        std::fprintf(stderr,"Usage: gta_sa_volume_probe GAME_DIR MODEL FROM_X FROM_Y FROM_Z TO_X TO_Y TO_Z\n");
        return 2;
    }
    try {
        const auto world=gtasa::world::LoadWorldCatalog(argv[1]);
        const auto archive=gtasa::assets::ImgDirectory::Open(
            std::filesystem::path(argv[1]) / "models/gta3.img");
        const auto catalog=gtasa::collision::CollisionCatalog::Load(archive);
        const std::array<float,3> from{std::stof(argv[3]),std::stof(argv[4]),
                                       std::stof(argv[5])};
        const std::array<float,3> to{std::stof(argv[6]),std::stof(argv[7]),
                                     std::stof(argv[8])};
        std::size_t matches=0;
        for (const auto& instance : world.instances) {
            if (instance.interior!=0) continue;
            const auto type=world.types.find(instance.modelId);
            if (type==world.types.end() || type->second.model!=std::string_view(argv[2]))
                continue;
            const float dx=instance.position[0]-from[0];
            const float dy=instance.position[1]-from[1];
            if (dx*dx+dy*dy>25.0f*25.0f) continue;
            const auto* mesh=catalog.Find(type->second.model);
            if (!mesh) continue;
            const bool blocked=gtasa::collision::BlocksVolumes(*mesh,
                instance.position,instance.rotation,from,to);
            std::printf("instance=%d pos=(%.2f,%.2f,%.2f) quat=(%.3f,%.3f,%.3f,%.3f) boxes=%zu spheres=%zu triangles=%zu blocks=%s\n",
                instance.modelId,instance.position[0],instance.position[1],
                instance.position[2],instance.rotation[0],instance.rotation[1],
                instance.rotation[2],instance.rotation[3],mesh->boxes.size(),
                mesh->spheres.size(),mesh->triangles.size(),blocked ? "yes" : "no");
            ++matches;
        }
        std::printf("matches=%zu\n",matches);
        return matches ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"Volume probe failed: %s\n",error.what());
        return 1;
    }
}
