#include "GroundQuery.h"

#include <algorithm>
#include <cmath>

namespace gtasa::collision {
namespace {
std::array<float,3> Transform(const std::array<float,3>& vertex,
                              const std::array<float,3>& position,
                              const std::array<float,4>& rotation) {
    const float x=-rotation[0],y=-rotation[1],z=-rotation[2],w=rotation[3];
    const float cx=y*vertex[2]-z*vertex[1];
    const float cy=z*vertex[0]-x*vertex[2];
    const float cz=x*vertex[1]-y*vertex[0];
    const float dx=y*cz-z*cy,dy=z*cx-x*cz,dz=x*cy-y*cx;
    return {vertex[0]+2.0f*(w*cx+dx)+position[0],
            vertex[1]+2.0f*(w*cy+dy)+position[1],
            vertex[2]+2.0f*(w*cz+dz)+position[2]};
}
}

std::optional<float> GroundHeight(const Mesh& mesh,
                                  const std::array<float,3>& position,
                                  const std::array<float,4>& rotation,
                                  float x,float y,float maxZ) {
    std::optional<float> best;
    for (const auto& triangle : mesh.triangles) {
        const auto a=Transform(mesh.vertices[triangle.indices[0]],position,rotation);
        const auto b=Transform(mesh.vertices[triangle.indices[1]],position,rotation);
        const auto c=Transform(mesh.vertices[triangle.indices[2]],position,rotation);
        const float denominator=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
        if (std::abs(denominator)<1.0e-6f) continue;
        const float u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/denominator;
        const float v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/denominator;
        const float w=1.0f-u-v;
        if (u< -1.0e-5f || v< -1.0e-5f || w< -1.0e-5f) continue;
        const float height=u*a[2]+v*b[2]+w*c[2];
        if (height<=maxZ+0.1f && (!best || height>*best)) best=height;
    }
    return best;
}

} // namespace gtasa::collision
