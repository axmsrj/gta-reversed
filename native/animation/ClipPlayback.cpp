#include "ClipPlayback.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gtasa::animation {
namespace {

std::array<float,4> Normalize(std::array<float,4> q) {
    float length=0.0f;
    for (float component : q) length+=component*component;
    if (!std::isfinite(length) || length<1.0e-8f)
        throw std::runtime_error("Invalid animation quaternion");
    const float inverse=1.0f/std::sqrt(length);
    for (auto& component : q) component*=inverse;
    return q;
}

std::array<float,4> Slerp(const std::array<float,4>& first,
                          const std::array<float,4>& second,float blend) {
    float dot=0.0f;
    for (int axis=0;axis<4;++axis) dot+=first[axis]*second[axis];
    const float sign=dot<0.0f ? -1.0f : 1.0f;
    dot=std::clamp(std::abs(dot),0.0f,1.0f);
    float a=1.0f-blend,b=blend;
    if (dot<0.9995f) {
        const float theta=std::acos(dot);
        const float inverse=1.0f/std::sin(theta);
        a=std::sin((1.0f-blend)*theta)*inverse;
        b=std::sin(blend*theta)*inverse;
    }
    std::array<float,4> result{};
    for (int axis=0;axis<4;++axis)
        result[axis]=a*first[axis]+b*sign*second[axis];
    return Normalize(result);
}

} // namespace

ClipPlayback::ClipPlayback(std::span<const std::uint8_t> bytes,const ClipInfo& clip) {
    tracks_.reserve(clip.sequences.size());
    for (const auto& sequence : clip.sequences) {
        Track track;
        track.boneTag=sequence.boneTag;
        track.frames=DecodeFrames(bytes,sequence);
        track.times.reserve(track.frames.size());
        float time=0.0f;
        for (std::size_t i=0;i<track.frames.size();++i) {
            const float next=track.frames[i].deltaTime;
            if (next<time || next>3600.0f)
                throw std::runtime_error("Invalid animation duration");
            time=next;
            track.frames[i].rotation=Normalize(track.frames[i].rotation);
            track.times.push_back(time);
        }
        duration_=std::max(duration_,time);
        tracks_.push_back(std::move(track));
    }
}

std::optional<float> ClipPlayback::RootTravelSpeed() const {
    for (const auto& track : tracks_) {
        if (track.boneTag!=0 || track.frames.size()<2) continue;
        const float duration=track.times.back()-track.times.front();
        if (duration<=0.0f || !std::isfinite(duration)) return {};
        const auto& first=track.frames.front().translation;
        const auto& last=track.frames.back().translation;
        const float speed=std::hypot(last[0]-first[0],last[1]-first[1])/duration;
        if (!std::isfinite(speed) || speed<=0.0f) return {};
        return speed;
    }
    return {};
}

std::optional<float> ClipPlayback::RootTravelDistance(float fromSeconds,float toSeconds) const {
    if (!std::isfinite(fromSeconds) || !std::isfinite(toSeconds) ||
        fromSeconds<0.0f || toSeconds<fromSeconds)
        throw std::runtime_error("Invalid root travel interval");
    for (const auto& track : tracks_) {
        if (track.boneTag!=0 || track.frames.size()<2) continue;
        const float duration=track.times.back();
        if (duration<=0.0f) return {};
        const auto& first=track.frames.front().translation;
        const auto& last=track.frames.back().translation;
        const float dx=last[0]-first[0],dy=last[1]-first[1];
        const float travel=std::hypot(dx,dy);
        if (travel<=0.0f || !std::isfinite(travel)) return {};
        const float directionX=dx/travel,directionY=dy/travel;
        const auto along=[&](const KeyFrame& frame) {
            return (frame.translation[0]-first[0])*directionX+
                   (frame.translation[1]-first[1])*directionY;
        };
        const auto displacement=[&](float seconds) {
            const float phase=std::fmod(seconds,duration);
            const auto next=std::upper_bound(track.times.begin(),track.times.end(),phase);
            const std::size_t b=std::min<std::size_t>(next-track.times.begin(),
                                                       track.frames.size()-1);
            const std::size_t a=b ? b-1 : 0;
            const float span=track.times[b]-track.times[a];
            const float weight=span>0.0f ? (phase-track.times[a])/span : 0.0f;
            return std::floor(seconds/duration)*travel+along(track.frames[a])+
                   weight*(along(track.frames[b])-along(track.frames[a]));
        };
        return std::max(0.0f,displacement(toSeconds)-displacement(fromSeconds));
    }
    return {};
}

std::vector<BonePose> ClipPlayback::Sample(float seconds) const {
    if (!std::isfinite(seconds)) throw std::runtime_error("Invalid animation time");
    const float time=duration_>0.0f ? std::fmod(std::max(seconds,0.0f),duration_) : 0.0f;
    std::vector<BonePose> pose;
    pose.reserve(tracks_.size());
    for (const auto& track : tracks_) {
        if (track.frames.empty()) continue;
        const auto next=std::upper_bound(track.times.begin(),track.times.end(),time);
        const std::size_t b=static_cast<std::size_t>(next-track.times.begin());
        const std::size_t a=b ? b-1 : 0;
        const std::size_t end=std::min(b,track.frames.size()-1);
        float blend=0.0f;
        if (end>a && track.times[end]>track.times[a])
            blend=(time-track.times[a])/(track.times[end]-track.times[a]);
        const auto& first=track.frames[a];
        const auto& last=track.frames[end];
        BonePose bone;
        bone.boneTag=track.boneTag;
        bone.rotation=Slerp(first.rotation,last.rotation,blend);
        for (int i=0;i<3;++i)
            bone.translation[i]=first.translation[i]*(1.0f-blend)+last.translation[i]*blend;
        pose.push_back(bone);
    }
    return pose;
}

std::vector<BonePose> BlendPoses(const std::vector<BonePose>& first,
                                const std::vector<BonePose>& second,float weight) {
    if (!std::isfinite(weight)) throw std::runtime_error("Invalid animation blend weight");
    weight=std::clamp(weight,0.0f,1.0f);
    std::vector<BonePose> result;
    result.reserve(std::max(first.size(),second.size()));
    for (const auto& a : first) {
        const auto b=std::find_if(second.begin(),second.end(),
            [&](const BonePose& candidate) { return candidate.boneTag==a.boneTag; });
        if (b==second.end()) { result.push_back(a); continue; }
        BonePose bone;
        bone.boneTag=a.boneTag;
        bone.rotation=Slerp(a.rotation,b->rotation,weight);
        for (int axis=0;axis<3;++axis)
            bone.translation[axis]=a.translation[axis]*(1.0f-weight)+
                                   b->translation[axis]*weight;
        result.push_back(bone);
    }
    for (const auto& b : second)
        if (std::none_of(first.begin(),first.end(),
            [&](const BonePose& a) { return a.boneTag==b.boneTag; })) result.push_back(b);
    return result;
}

} // namespace gtasa::animation
