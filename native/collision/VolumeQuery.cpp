#include "VolumeQuery.h"

#include <algorithm>
#include <cmath>

namespace gtasa::collision {
namespace {

std::array<float,3> ToLocal(std::array<float,3> point,
                            const std::array<float,3>& position,
                            const std::array<float,4>& rotation) {
    for (int axis=0;axis<3;++axis) point[axis]-=position[axis];
    const float x=rotation[0],y=rotation[1],z=rotation[2],w=rotation[3];
    const float cx=y*point[2]-z*point[1];
    const float cy=z*point[0]-x*point[2];
    const float cz=x*point[1]-y*point[0];
    const float dx=y*cz-z*cy,dy=z*cx-x*cz,dz=x*cy-y*cx;
    return {point[0]+2.0f*(w*cx+dx),point[1]+2.0f*(w*cy+dy),
            point[2]+2.0f*(w*cz+dz)};
}

bool SegmentBox(const std::array<float,3>& start,const std::array<float,3>& end,
                const Box& box,float radius) {
    float enter=0.0f,leave=1.0f;
    for (int axis=0;axis<3;++axis) {
        const float direction=end[axis]-start[axis];
        const float low=box.minimum[axis]-radius;
        const float high=box.maximum[axis]+radius;
        if (std::abs(direction)<1.0e-7f) {
            if (start[axis]<low || start[axis]>high) return false;
            continue;
        }
        const float a=(low-start[axis])/direction,b=(high-start[axis])/direction;
        enter=std::max(enter,std::min(a,b));
        leave=std::min(leave,std::max(a,b));
        if (enter>leave) return false;
    }
    // The expanded box only rejects distant paths. Its corners are larger than
    // a sphere swept along the segment, so measure the actual closest distance.
    const auto distanceSquared=[&](float t) {
        float squared=0.0f;
        for (int axis=0;axis<3;++axis) {
            const float point=start[axis]+(end[axis]-start[axis])*t;
            const float delta=point<box.minimum[axis] ? box.minimum[axis]-point :
                              point>box.maximum[axis] ? point-box.maximum[axis] : 0.0f;
            squared+=delta*delta;
        }
        return squared;
    };
    float low=enter,high=leave;
    for (int iteration=0;iteration<32;++iteration) {
        const float first=low+(high-low)/3.0f;
        const float second=high-(high-low)/3.0f;
        if (distanceSquared(first)<distanceSquared(second)) high=second;
        else low=first;
    }
    return distanceSquared((low+high)*0.5f)<=radius*radius;
}

bool SegmentSphere(const std::array<float,3>& start,const std::array<float,3>& end,
                   const Sphere& sphere,float radius) {
    const std::array<float,3> direction{end[0]-start[0],end[1]-start[1],end[2]-start[2]};
    const std::array<float,3> offset{sphere.center[0]-start[0],
                                     sphere.center[1]-start[1],
                                     sphere.center[2]-start[2]};
    const float length=direction[0]*direction[0]+direction[1]*direction[1]+
                       direction[2]*direction[2];
    const float projection=length>0.0f ? std::clamp((offset[0]*direction[0]+
        offset[1]*direction[1]+offset[2]*direction[2])/length,0.0f,1.0f) : 0.0f;
    float distance=0.0f;
    for (int axis=0;axis<3;++axis) {
        const float delta=offset[axis]-direction[axis]*projection;
        distance+=delta*delta;
    }
    const float combined=sphere.radius+radius;
    return distance<=combined*combined;
}

using Point=std::array<float,3>;

Point Subtract(const Point& a,const Point& b) {
    return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};
}

Point AddScaled(const Point& a,const Point& direction,float scale) {
    return {a[0]+direction[0]*scale,a[1]+direction[1]*scale,
            a[2]+direction[2]*scale};
}

float Dot(const Point& a,const Point& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

Point ClosestSegment(const Point& point,const Point& a,const Point& b) {
    const auto delta=Subtract(b,a);
    const float length=Dot(delta,delta);
    return AddScaled(a,delta,length>0.0f ?
        std::clamp(Dot(Subtract(point,a),delta)/length,0.0f,1.0f) : 0.0f);
}

Point ClosestTriangle(const Point& point,const Point& a,const Point& b,const Point& c) {
    const auto ab=Subtract(b,a),ac=Subtract(c,a),ap=Subtract(point,a);
    const float d1=Dot(ab,ap),d2=Dot(ac,ap);
    if (d1<=0.0f && d2<=0.0f) return a;
    const auto bp=Subtract(point,b);
    const float d3=Dot(ab,bp),d4=Dot(ac,bp);
    if (d3>=0.0f && d4<=d3) return b;
    const float vc=d1*d4-d3*d2;
    if (vc<=0.0f && d1>=0.0f && d3<=0.0f)
        return AddScaled(a,ab,d1/(d1-d3));
    const auto cp=Subtract(point,c);
    const float d5=Dot(ab,cp),d6=Dot(ac,cp);
    if (d6>=0.0f && d5<=d6) return c;
    const float vb=d5*d2-d1*d6;
    if (vb<=0.0f && d2>=0.0f && d6<=0.0f)
        return AddScaled(a,ac,d2/(d2-d6));
    const float va=d3*d6-d5*d4;
    if (va<=0.0f && d4-d3>=0.0f && d5-d6>=0.0f)
        return AddScaled(b,Subtract(c,b),(d4-d3)/((d4-d3)+(d5-d6)));
    const float denominator=va+vb+vc;
    if (std::abs(denominator)<1.0e-10f) {
        const auto onAB=ClosestSegment(point,a,b);
        const auto onAC=ClosestSegment(point,a,c);
        const auto onBC=ClosestSegment(point,b,c);
        const float dab=Dot(Subtract(point,onAB),Subtract(point,onAB));
        const float dac=Dot(Subtract(point,onAC),Subtract(point,onAC));
        const float dbc=Dot(Subtract(point,onBC),Subtract(point,onBC));
        return dab<=dac && dab<=dbc ? onAB : dac<=dbc ? onAC : onBC;
    }
    const float inverse=1.0f/denominator;
    return AddScaled(AddScaled(a,ab,vb*inverse),ac,vc*inverse);
}

bool SegmentTriangle(const Point& start,const Point& end,
                     const Point& a,const Point& b,const Point& c,float radius) {
    for (int axis=0;axis<3;++axis) {
        const float low=std::min(start[axis],end[axis])-radius;
        const float high=std::max(start[axis],end[axis])+radius;
        if (std::max({a[axis],b[axis],c[axis]})<low ||
            std::min({a[axis],b[axis],c[axis]})>high) return false;
    }
    const auto delta=Subtract(end,start);
    const float length=std::sqrt(Dot(delta,delta));
    const int samples=std::max(1,static_cast<int>(std::min(64.0f,
        std::ceil(length/(radius*0.5f)))));
    for (int sample=0;sample<=samples;++sample) {
        const auto point=AddScaled(start,delta,float(sample)/samples);
        const auto closest=ClosestTriangle(point,a,b,c);
        const auto separation=Subtract(point,closest);
        if (Dot(separation,separation)<=radius*radius) return true;
    }
    return false;
}

} // namespace

bool BlocksVolumes(const Mesh& mesh,const std::array<float,3>& instancePosition,
                   const std::array<float,4>& instanceRotation,
                   std::array<float,3> fromFeet,std::array<float,3> toFeet,
                   float radius) {
    if (!std::isfinite(radius) || radius<=0.0f) return false;
    for (float height : {0.65f,1.35f}) {
        const auto start=ToLocal({fromFeet[0],fromFeet[1],fromFeet[2]+height},
                                 instancePosition,instanceRotation);
        const auto end=ToLocal({toFeet[0],toFeet[1],toFeet[2]+height},
                               instancePosition,instanceRotation);
        for (const auto& box : mesh.boxes)
            if (SegmentBox(start,end,box,radius)) return true;
        for (const auto& sphere : mesh.spheres)
            if (SegmentSphere(start,end,sphere,radius)) return true;
        for (const auto& triangle : mesh.triangles) {
            const auto& a=mesh.vertices[triangle.indices[0]];
            const auto& b=mesh.vertices[triangle.indices[1]];
            const auto& c=mesh.vertices[triangle.indices[2]];
            if (SegmentTriangle(start,end,a,b,c,radius)) return true;
        }
    }
    return false;
}

} // namespace gtasa::collision
