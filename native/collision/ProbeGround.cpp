#include "CollisionCatalog.h"
#include "WorldGround.h"
#include "ImgDirectory.h"
#include "Placement.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace {
float Coordinate(const char* text) {
    char* end=nullptr;
    const float value=std::strtof(text,&end);
    if (end==text || *end || !std::isfinite(value)) throw std::runtime_error("Invalid coordinate");
    return value;
}
}

int main(int argc,const char* argv[]) {
    if (argc!=5 && argc!=6) {
        std::fprintf(stderr,"Usage: gta_sa_ground_probe GAME_ROOT X Y MAX_Z [SEARCH_RADIUS]\n");
        return 2;
    }
    try {
        const std::array<float,3> point{Coordinate(argv[2]),Coordinate(argv[3]),Coordinate(argv[4])};
        const float radius=argc==6 ? Coordinate(argv[5]) : 250.0f;
        const auto world=gtasa::world::LoadWorldCatalog(argv[1]);
        const auto archive=gtasa::assets::ImgDirectory::Open(
            gtasa::world::ResolveGamePath(argv[1],"MODELS\\GTA3.IMG"));
        const auto collision=gtasa::collision::CollisionCatalog::Load(archive);
        const auto ground=gtasa::collision::GroundAt(world,collision,point,radius);
        std::printf("collision-models=%zu triangles=%zu\n",
            collision.Size(),collision.TriangleCount());
        if (!ground) { std::puts("No ground triangle found"); return 1; }
        std::printf("ground=(%.3f,%.3f,%.3f) model=%s id=%d\n",
            point[0],point[1],ground->height,ground->modelName.c_str(),ground->modelId);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"Ground probe failed: %s\n",error.what());
        return 1;
    }
}
