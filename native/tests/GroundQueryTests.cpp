#include "GroundQuery.h"

#include <cassert>

int main() {
    gtasa::collision::Mesh mesh;
    mesh.vertices={{{0,0,0}},{{10,0,0}},{{0,10,0}},
                   {{0,0,5}},{{10,0,5}},{{0,10,5}}};
    mesh.triangles={{{0,1,2},0,0},{{3,4,5},0,0}};
    constexpr std::array<float,4> identity{0,0,0,1};
    const std::array<float,3> position{100,200,3};
    assert(gtasa::collision::GroundHeight(mesh,position,identity,102,202,20)==8.0f);
    assert(gtasa::collision::GroundHeight(mesh,position,identity,102,202,6)==3.0f);
    assert(!gtasa::collision::GroundHeight(mesh,position,identity,120,220,20));
    constexpr std::array<float,4> halfTurn{0,0,1,0};
    assert(gtasa::collision::GroundHeight(mesh,position,halfTurn,98,198,20)==8.0f);
    return 0;
}
