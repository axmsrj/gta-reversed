#include "PedIfp.h"
#include "ClipPlayback.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

void U32(std::vector<std::uint8_t>& bytes,std::uint32_t value) {
    for (int i=0;i<4;++i) bytes.push_back(std::uint8_t(value>>(8*i)));
}

void Name(std::vector<std::uint8_t>& bytes,const char* name) {
    const auto start=bytes.size();
    while (*name) bytes.push_back(std::uint8_t(*name++));
    bytes.resize(start+24,0);
}

bool Rejected(const std::vector<std::uint8_t>& bytes) {
    try { (void)gtasa::animation::ReadAnp3(bytes); }
    catch (const std::runtime_error&) { return true; }
    return false;
}

} // namespace

int main() {
    std::vector<std::uint8_t> bytes{'A','N','P','3'};
    U32(bytes,0);
    Name(bytes,"ped");
    U32(bytes,1);
    Name(bytes,"walk");
    U32(bytes,1);
    U32(bytes,20);
    U32(bytes,0);
    Name(bytes,"Root");
    U32(bytes,3);
    U32(bytes,2);
    U32(bytes,0);
    const auto framesOffset=bytes.size();
    bytes.resize(bytes.size()+20,0);
    const auto payload=std::uint32_t(bytes.size()-8);
    for (int i=0;i<4;++i) bytes[4+i]=std::uint8_t(payload>>(8*i));

    const auto catalog=gtasa::animation::ReadAnp3(bytes);
    assert(catalog.blockName=="ped" && catalog.clips.size()==1);
    assert(catalog.clips[0].name=="walk" && catalog.clips[0].sequences.size()==1);
    const auto& sequence=catalog.clips[0].sequences[0];
    assert(sequence.name=="Root" && sequence.frameType==3);
    assert(sequence.frameCount==2 && sequence.framesOffset==framesOffset);
    bytes[framesOffset]=0x00;
    bytes[framesOffset+1]=0x10; // quaternion x = 1
    bytes[framesOffset+12]=0x00;
    bytes[framesOffset+13]=0x10; // quaternion y = 1
    bytes[framesOffset+18]=60; // second frame is at 1 second
    const auto frames=gtasa::animation::DecodeFrames(bytes,sequence);
    assert(frames.size()==2 && frames[0].rotation[0]==1.0f);
    assert(frames[0].deltaTime==0.0f && frames[1].deltaTime==1.0f);
    const gtasa::animation::ClipPlayback playback(bytes,catalog.clips[0]);
    assert(playback.Duration()==1.0f);
    const auto pose=playback.Sample(0.5f);
    assert(pose.size()==1);
    assert(std::abs(pose[0].rotation[0]-0.7071068f)<1.0e-5f);
    assert(std::abs(pose[0].rotation[1]-0.7071068f)<1.0e-5f);
    const auto quarter=playback.Sample(0.25f);
    assert(std::abs(quarter[0].rotation[0]-0.9238795f)<1.0e-5f);
    assert(std::abs(quarter[0].rotation[1]-0.3826834f)<1.0e-5f);
    const auto wrapped=playback.Sample(1.5f);
    assert(std::abs(wrapped[0].rotation[0]-pose[0].rotation[0])<1.0e-5f);
    const gtasa::animation::BonePose idle{0,{0,0,0,1},{0,0,0}};
    const gtasa::animation::BonePose walking{0,{0,0,0,-1},{2,0,0}};
    const auto blended=gtasa::animation::BlendPoses({idle},{walking},0.5f);
    assert(blended.size()==1 && std::abs(blended[0].rotation[3]-1.0f)<1.0e-5f);
    assert(blended[0].translation[0]==1.0f);
    std::vector<std::uint8_t> travelFrames(32,0);
    travelFrames[6]=0x00; travelFrames[7]=0x10; // identity quaternion w
    travelFrames[22]=0x00; travelFrames[23]=0x10;
    travelFrames[24]=60; // one second
    travelFrames[26]=0x00; travelFrames[27]=0x08; // two world units in x
    gtasa::animation::ClipInfo travelClip{"travel",0,{{"Root",0,4,2,0}}};
    const gtasa::animation::ClipPlayback travel(travelFrames,travelClip);
    assert(travel.RootTravelSpeed());
    assert(std::abs(*travel.RootTravelSpeed()-2.0f)<1.0e-5f);
    assert(std::abs(*travel.RootTravelDistance(0.25f,0.75f)-1.0f)<1.0e-5f);
    assert(std::abs(*travel.RootTravelDistance(0.75f,1.25f)-1.0f)<1.0e-5f);
    assert(std::abs(*travel.RootTravelDistance(0.0f,2.0f)-4.0f)<1.0e-5f);
    travelClip.sequences[0].boneTag=1;
    assert(!gtasa::animation::ClipPlayback(travelFrames,travelClip).RootTravelSpeed());
    auto outOfBounds=sequence;
    outOfBounds.framesOffset=bytes.size();
    assert([&] { try { (void)gtasa::animation::DecodeFrames(bytes,outOfBounds); }
                 catch (const std::runtime_error&) { return true; } return false; }());

    auto bad=bytes;
    bad[0]='X'; assert(Rejected(bad));
    bad=bytes; bad.pop_back(); assert(Rejected(bad));
    bad=bytes; bad[96]=0xff; assert(Rejected(bad)); // frame type
    bad=bytes; bad[100]=0xff; assert(Rejected(bad)); // frame count
    bad=bytes; bad[64]=0xff; assert(Rejected(bad)); // frame allocation size
    return 0;
}
