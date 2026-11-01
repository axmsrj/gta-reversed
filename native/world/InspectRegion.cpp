#include "Placement.h"

#include <cstdio>
#include <exception>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>

int main(int argc, const char* argv[]) {
    if (argc != 4 && argc != 6) {
        std::fprintf(stderr,"Usage: gta_sa_world_probe GAME_DIR REGION_IDE REGION_IPL\n"
                            "   or: gta_sa_world_probe GAME_DIR X Y Z RADIUS\n");
        return 2;
    }
    try {
        if (argc == 6) {
            const auto world=gtasa::world::LoadWorldCatalog(argv[1]);
            char* end=nullptr;
            std::array<float,3> center{};
            for (int i=0;i<3;++i) {
                center[i]=std::strtof(argv[i+2],&end);
                if (!end || *end) throw std::runtime_error("Invalid center coordinate");
            }
            const float radius=std::strtof(argv[5],&end);
            if (!end || *end) throw std::runtime_error("Invalid radius");
            const auto nearby=gtasa::world::InstancesNear(world,center,radius);
            std::printf("catalog types=%zu instances=%zu nearby=%zu\n",
                world.types.size(),world.instances.size(),nearby.size());
            std::size_t lodReferences=0,resolvedLods=0;
            for (const auto& instance : world.instances) {
                lodReferences+=instance.lodIndex>=0;
                resolvedLods+=instance.lodTarget>=0;
            }
            std::printf("lod references=%zu resolved=%zu unresolved=%zu\n",
                lodReferences,resolvedLods,lodReferences-resolvedLods);
            std::size_t shown=0;
            for (const auto* instance : nearby) {
                const auto& type=world.types.at(instance->modelId);
                if (type.model.size() >= 3 &&
                    (type.model[0]=='L' || type.model[0]=='l') &&
                    (type.model[1]=='O' || type.model[1]=='o') &&
                    (type.model[2]=='D' || type.model[2]=='d')) continue;
                std::printf("  id=%d model=%s txd=%s pos=(%.2f,%.2f,%.2f)\n",
                    instance->modelId,type.model.c_str(),type.textureDictionary.c_str(),
                    instance->position[0],instance->position[1],instance->position[2]);
                if (++shown==10) break;
            }
            return 0;
        }
        auto ide=gtasa::world::ResolveGamePath(argv[1],argv[2]);
        auto ipl=gtasa::world::ResolveGamePath(argv[1],argv[3]);
        const auto region=gtasa::world::LoadRegion(ide,ipl);
        std::size_t resolved=0;
        for (const auto& instance : region.instances)
            resolved+=region.types.contains(instance.modelId);
        std::printf("types=%zu instances=%zu locally-resolved=%zu\n",
            region.types.size(),region.instances.size(),resolved);
        for (const auto& instance : region.instances) {
            if (region.types.contains(instance.modelId) && instance.lodIndex < 0) {
                const auto& type=region.types.at(instance.modelId);
                std::printf("example: id=%d model=%s txd=%s pos=(%.2f,%.2f,%.2f)\n",
                    instance.modelId,type.model.c_str(),type.textureDictionary.c_str(),
                    instance.position[0],instance.position[1],instance.position[2]);
                break;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"World parse failed: %s\n",error.what());
        return 1;
    }
}
