#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

struct ViewControls {
    float yaw{0.59f}, elevation{0.46f}, zoom{1.0f};
    float panX{}, panY{};
};

struct ModelBounds {
    std::array<float,3> minimum{INFINITY,INFINITY,INFINITY};
    std::array<float,3> maximum{-INFINITY,-INFINITY,-INFINITY};
};

inline std::array<float,17> ProjectionParameters(const ModelBounds& bounds,
    const ViewControls& view, const std::optional<std::array<float,4>>& worldCenter,
    float aspect, bool closeCamera=false) {
    const float cy=std::cos(view.yaw), sy=std::sin(view.yaw);
    const float ce=std::cos(view.elevation), se=std::sin(view.elevation);
    const auto project=[&](float x,float y,float z) {
        return std::array<float,3>{cy*x+sy*y, -sy*se*x+cy*se*y+ce*z,
                                   sy*ce*x-cy*ce*y+se*z};
    };
    float minX=INFINITY,minY=INFINITY,minD=INFINITY;
    float maxX=-INFINITY,maxY=-INFINITY,maxD=-INFINITY;
    for (int corner=0;corner<8;++corner) {
        const auto p=project(
            (corner&1) ? bounds.maximum[0] : bounds.minimum[0],
            (corner&2) ? bounds.maximum[1] : bounds.minimum[1],
            (corner&4) ? bounds.maximum[2] : bounds.minimum[2]);
        minX=std::min(minX,p[0]); maxX=std::max(maxX,p[0]);
        minY=std::min(minY,p[1]); maxY=std::max(maxY,p[1]);
        minD=std::min(minD,p[2]); maxD=std::max(maxD,p[2]);
    }
    float centerX=(minX+maxX)*0.5f,centerY=(minY+maxY)*0.5f;
    float scale=view.zoom*1.55f/std::max(0.001f,std::max(maxX-minX,(maxY-minY)*1.6f));
    if (worldCenter) {
        const auto screen=project((*worldCenter)[0],(*worldCenter)[1],(*worldCenter)[2]);
        centerX=screen[0]; centerY=screen[1];
        scale=view.zoom*1.55f/((*worldCenter)[3]*3.0f);
        const float distance=(closeCamera ? 5.0f : (*worldCenter)[3]*3.5f)/view.zoom;
        const float focal=closeCamera ? 1.55f : 1.55f*3.5f/3.0f;
        const float nearPlane=0.1f;
        const float farPlane=std::max(1000.0f,screen[2]+distance-minD+100.0f);
        return {cy,sy,ce,se,centerX,centerY,scale,view.panX,view.panY,
                maxD,1.0f/std::max(0.001f,maxD-minD),1.0f,
                screen[2]+distance,focal/std::max(0.1f,aspect),focal,nearPlane,farPlane};
    }
    return {cy,sy,ce,se,centerX,centerY,scale,view.panX,view.panY,
            maxD,1.0f/std::max(0.001f,maxD-minD),0.0f,0.0f,0.0f,0.0f,0.1f,1000.0f};
}

inline bool VisibleBounds(const ModelBounds& bounds, const std::array<float,17>& p) {
    unsigned outsideEveryCorner=0x3f;
    for (int corner=0;corner<8;++corner) {
        const float x=(corner&1) ? bounds.maximum[0] : bounds.minimum[0];
        const float y=(corner&2) ? bounds.maximum[1] : bounds.minimum[1];
        const float z=(corner&4) ? bounds.maximum[2] : bounds.minimum[2];
        const float sx=p[0]*x+p[1]*y;
        const float sy=-p[1]*p[3]*x+p[0]*p[3]*y+p[2]*z;
        const float d=p[1]*p[2]*x-p[0]*p[2]*y+p[3]*z;
        float clipX,clipY,clipZ,w;
        if (p[11]>0.5f) {
            w=p[12]-d;
            clipX=(sx-p[4])*p[13]+p[7]*w;
            clipY=(sy-p[5])*p[14]+p[8]*w;
            clipZ=(w-p[15])*p[16]/(p[16]-p[15]);
        } else {
            w=1.0f;
            clipX=(sx-p[4])*p[6]+p[7];
            clipY=(sy-p[5])*p[6]+p[8];
            clipZ=0.05f+0.90f*(p[9]-d)*p[10];
        }
        unsigned outside=0;
        if (clipX < -w) outside|=1;
        if (clipX > w) outside|=2;
        if (clipY < -w) outside|=4;
        if (clipY > w) outside|=8;
        if (clipZ < 0.0f) outside|=16;
        if (clipZ > w) outside|=32;
        outsideEveryCorner&=outside;
        if (!outsideEveryCorner) return true;
    }
    return outsideEveryCorner==0;
}
