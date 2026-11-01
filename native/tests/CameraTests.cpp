#include "Camera.h"

#include <array>
#include <cstdio>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr,"Camera test failed: %s\n",message);
    return condition;
}

ModelBounds Box(float x0,float y0,float z0,float x1,float y1,float z1) {
    return {{x0,y0,z0},{x1,y1,z1}};
}
}

int main() {
    bool ok=true;
    ok&=Expect(std::abs(FollowPedDistance(0.22f)-3.6518f)<0.001f,
               "original follow ped radius at startup pitch");
    const auto follow=ProjectionParameters(Box(-1,-1,-1,1,1,1),
        ViewControls{},std::array<float,4>{0,0,0,80},1.6f,true);
    ok&=Expect(std::abs(follow[13]-1.428148f)<0.00001f &&
               std::abs(follow[14]-1.428148f*1.6f)<0.00001f,
               "follow ped uses horizontal 70 degree field of view");
    std::array<float,17> perspective{};
    perspective[0]=1.0f;
    perspective[2]=1.0f;
    perspective[11]=1.0f;
    perspective[12]=5.0f;
    perspective[13]=perspective[14]=1.0f;
    perspective[15]=0.1f;
    perspective[16]=10.0f;
    ok&=Expect(VisibleBounds(Box(-1,-1,-1,1,1,1),perspective),"perspective center");
    ok&=Expect(!VisibleBounds(Box(7,0,0,8,1,1),perspective),"perspective right");
    ok&=Expect(!VisibleBounds(Box(0,0,7,1,1,8),perspective),"perspective top");
    ok&=Expect(!VisibleBounds(Box(0,-7,0,1,-6,1),perspective),"perspective near");
    ok&=Expect(!VisibleBounds(Box(0,6,0,1,7,1),perspective),"perspective far");
    ok&=Expect(VisibleBounds(Box(-1,-6,-1,1,0,1),perspective),"perspective near crossing");

    std::array<float,17> orthographic{};
    orthographic[0]=orthographic[2]=orthographic[6]=1.0f;
    orthographic[9]=1.0f;
    orthographic[10]=0.5f;
    ok&=Expect(VisibleBounds(Box(-0.5f,-0.5f,-0.5f,0.5f,0.5f,0.5f),orthographic),"orthographic center");
    ok&=Expect(!VisibleBounds(Box(-3,0,0,-2,1,1),orthographic),"orthographic left");
    ok&=Expect(!VisibleBounds(Box(0,0,2,1,1,3),orthographic),"orthographic top");
    return ok ? 0 : 1;
}
