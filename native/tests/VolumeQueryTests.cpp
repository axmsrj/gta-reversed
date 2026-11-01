#include "VolumeQuery.h"

#include <cassert>

int main() {
    gtasa::collision::Mesh mesh;
    constexpr std::array<float,3> position{0,0,0};
    constexpr std::array<float,4> identity{0,0,0,1};
    mesh.boxes.push_back({{1.0f,-0.2f,0.4f},{1.2f,0.2f,2.0f},7});
    assert(gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{2,0,0}));
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,2,0},{2,2,0}));
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{0.4f,0,0}));
    mesh.boxes.clear();
    mesh.boxes.push_back({{1.0f,1.0f,0.65f},{1.2f,1.2f,1.35f},7});
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,
                                            {0.7f,0.7f,0},{0.75f,0.75f,0}));
    assert(gtasa::collision::BlocksVolumes(mesh,position,identity,
                                           {0.7f,0.7f,0},{1.0f,0.9f,0}));
    mesh.boxes.clear();
    mesh.boxes.push_back({{1.0f,-0.2f,-0.2f},{1.2f,0.2f,0.2f},7});
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{2,0,0}));
    mesh.boxes.clear();
    mesh.spheres.push_back({{1.0f,0.0f,1.0f},0.25f,9});
    assert(gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{2,0,0}));
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,2,0},{2,2,0}));
    constexpr std::array<float,4> quarterTurn{0,0,0.70710678f,0.70710678f};
    assert(gtasa::collision::BlocksVolumes(mesh,{10,20,0},quarterTurn,
                                           {10,20,0},{10,18,0}));
    mesh.spheres.clear();
    mesh.vertices={{{1,-1,0}},{{1,1,0}},{{1,0,2}}};
    mesh.triangles={{{0,1,2},0,0}};
    assert(gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{2,0,0}));
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,2,0},{2,2,0}));
    mesh.vertices={{{0,-2,0}},{{2,-2,0}},{{1,2,0}}};
    assert(!gtasa::collision::BlocksVolumes(mesh,position,identity,{0,0,0},{2,0,0}));
    return 0;
}
