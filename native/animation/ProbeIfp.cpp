#include "PedIfp.h"
#include "ClipPlayback.h"

#include <fstream>
#include <cmath>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

int main(int argc,char** argv) {
    if (argc!=2 && argc!=3) {
        std::cerr << "Usage: gta_sa_ifp_probe ped.ifp [name-filter]\n";
        return 2;
    }
    try {
        std::ifstream file(argv[1],std::ios::binary);
        if (!file) throw std::runtime_error("Could not open IFP");
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
        const auto catalog=gtasa::animation::ReadAnp3(bytes);
        std::size_t sequences=0,frames=0;
        for (const auto& clip : catalog.clips) for (const auto& sequence : clip.sequences) {
            const auto decoded=gtasa::animation::DecodeFrames(bytes,sequence);
            ++sequences;
            frames+=decoded.size();
        }
        std::cout << "block=" << catalog.blockName << " clips=" << catalog.clips.size()
                  << " sequences=" << sequences << " frames=" << frames << '\n';
        for (const auto& clip : catalog.clips) {
            const gtasa::animation::ClipPlayback playback(bytes,clip);
            const auto pose=playback.Sample(playback.Duration()*0.5f);
            if (argc==3 && clip.name.find(argv[2])!=std::string::npos) {
                std::cout << "clip=" << clip.name << " duration=" << playback.Duration()
                          << " posedBones=" << pose.size() << '\n';
                for (float fraction : {0.0f,0.5f,0.9f}) {
                    const auto sample=playback.Sample(playback.Duration()*fraction);
                    for (const auto& bone : sample) if (bone.boneTag==0)
                        std::cout << "  root t=" << fraction << " position=("
                                  << bone.translation[0] << ',' << bone.translation[1]
                                  << ',' << bone.translation[2] << ")\n";
                }
                for (const auto& sequence : clip.sequences) {
                    const auto frames=gtasa::animation::DecodeFrames(bytes,sequence);
                    if (sequence.boneTag==0 && !frames.empty())
                        std::cout << "  root final position=(" << frames.back().translation[0]
                                  << ',' << frames.back().translation[1] << ','
                                  << frames.back().translation[2] << ")\n";
                    if (sequence.boneTag==0 && !frames.empty())
                        std::cout << "  root first quaternion=(" << frames.front().rotation[0]
                                  << ',' << frames.front().rotation[1] << ','
                                  << frames.front().rotation[2] << ','
                                  << frames.front().rotation[3] << ")\n";
                    if (sequence.boneTag==0) {
                        std::cout << "  root frame times=";
                        for (std::size_t i=0;i<std::min<std::size_t>(frames.size(),12);++i)
                            std::cout << ' ' << frames[i].deltaTime;
                        std::cout << '\n';
                    }
                    if (sequence.boneTag!=0 && !frames.empty() &&
                        (std::abs(frames.front().translation[0])>0.001f ||
                         std::abs(frames.front().translation[1])>0.001f ||
                         std::abs(frames.front().translation[2])>0.001f))
                        std::cout << "  translated bone=" << sequence.boneTag
                                  << " name=" << sequence.name << "\n";
                }
            }
            if (clip.name=="WALK_player")
                std::cout << "walk=" << clip.name << " duration=" << playback.Duration()
                          << " posedBones=" << pose.size() << " rootTravelSpeed="
                          << playback.RootTravelSpeed().value_or(0.0f) << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "IFP error: " << error.what() << '\n';
        return 1;
    }
}
