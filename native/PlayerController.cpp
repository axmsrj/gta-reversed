#include "PlayerController.h"

#include <algorithm>
#include <cmath>

namespace gtasa::native {

bool PlayerController::Step(float seconds,float worldX,float worldY,
                            const GroundProbe& ground,const ObstacleProbe& blocked,
                            std::optional<float> travelDistance) {
    if (seconds<=0.0f || !std::isfinite(seconds)) return false;
    const float length=std::hypot(worldX,worldY);
    if (length<=0.0f || !std::isfinite(length)) return false;
    if (!std::isfinite(walkingSpeed_) || walkingSpeed_<=0.0f) return false;
    constexpr float maxStepUp=0.6f;
    constexpr float maxDrop=1.5f;
    if (travelDistance && (!std::isfinite(*travelDistance) || *travelDistance<0.0f))
        return false;
    const float distance=travelDistance.value_or(walkingSpeed_*std::min(seconds,0.05f)) /
                         std::max(1.0f,length);
    const float deltaX=worldX*distance,deltaY=worldY*distance;
    const auto original=position_;
    const auto tryMove=[&](float dx,float dy) {
        if (dx==0.0f && dy==0.0f) return false;
        const float nextX=position_[0]+dx,nextY=position_[1]+dy;
        const float currentGround=position_[2]+footOffset_;
        const auto nextGround=ground(nextX,nextY,currentGround+maxStepUp);
        if (!nextGround || !std::isfinite(*nextGround) ||
            *nextGround>currentGround+maxStepUp ||
            *nextGround<currentGround-maxDrop) return false;
        if (blocked && blocked({position_[0],position_[1],currentGround},
                               {nextX,nextY,*nextGround})) return false;
        position_={nextX,nextY,*nextGround-footOffset_};
        return true;
    };
    if (!tryMove(deltaX,deltaY)) {
        // Keep the free component when a diagonal move hits a wall or corner.
        if (std::abs(deltaX)>=std::abs(deltaY)) {
            tryMove(deltaX,0.0f);
            tryMove(0.0f,deltaY);
        } else {
            tryMove(0.0f,deltaY);
            tryMove(deltaX,0.0f);
        }
    }
    const float traveledX=position_[0]-original[0];
    const float traveledY=position_[1]-original[1];
    if (traveledX==0.0f && traveledY==0.0f) return false;
    heading_=std::atan2(traveledY,traveledX);
    return true;
}

} // namespace gtasa::native
