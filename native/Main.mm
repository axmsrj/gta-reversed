#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <vector>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include "world/Placement.h"
#include "world/Visibility.h"
#include "Camera.h"
#include "GameTimer.h"
#include "PlayerController.h"
#include "Maths.h"
#include <array>
#include <cstring>
#ifdef GTASA_HAS_LIBRW
#include "assets/ImgDirectory.h"
#include "assets/DxtDecode.h"
#include "collision/CollisionCatalog.h"
#include "collision/WorldGround.h"
#include "animation/ClipPlayback.h"
#include "rw.h"
#endif

namespace {
void Log(const char* message) {
    std::fprintf(stderr, "[native] %s\n", message);
    std::fflush(stderr);
}

struct Options {
    unsigned smokeFrames{};
    std::filesystem::path assets;
    std::string archive{"gta3"};
    bool playerPreview{};
    bool play{};
    bool autoWalk{};
    std::string model{"infernus"};
    std::string txd{"infernus"};
    std::optional<int> instanceId;
    std::optional<std::array<float,4>> worldCenter;
};

bool ParseOptions(int argc, const char* argv[], Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument == "--help") {
            std::puts("GTA San Andreas Native\nUsage: gta_sa_native [--assets GAME_DIR] [--archive gta3|player|gta_int] [--model NAME --txd NAME | --player-preview | --instance-id ID | --world-center X Y Z RADIUS [--play [--auto-walk]]] [--smoke-frames N]");
            std::exit(0);
        }
        if (argument == "--assets" && i + 1 < argc) {
            options.assets = argv[++i];
            continue;
        }
        if (argument == "--archive" && i + 1 < argc) {
            options.archive=argv[++i];
            if (options.archive!="gta3" && options.archive!="player" && options.archive!="gta_int") return false;
            continue;
        }
        if (argument == "--player-preview") {
            options.playerPreview=true;
            options.archive="player";
            continue;
        }
        if (argument == "--play") {
            options.play=true;
            continue;
        }
        if (argument == "--auto-walk") {
            options.autoWalk=true;
            continue;
        }
        if (argument == "--model" && i + 1 < argc) {
            options.model = argv[++i];
            continue;
        }
        if (argument == "--txd" && i + 1 < argc) {
            options.txd = argv[++i];
            continue;
        }
        if (argument == "--instance-id" && i + 1 < argc) {
            const std::string_view value{argv[++i]};
            int id{};
            const auto result=std::from_chars(value.data(),value.data()+value.size(),id);
            if (result.ec != std::errc{} || result.ptr != value.data()+value.size()) return false;
            options.instanceId=id;
            continue;
        }
        if (argument == "--world-center" && i + 4 < argc) {
            std::array<float,4> values{};
            for (int n=0;n<4;++n) {
                const std::string_view value{argv[++i]};
                const auto result=std::from_chars(value.data(),value.data()+value.size(),values[n]);
                if (result.ec != std::errc{} || result.ptr != value.data()+value.size() ||
                    !std::isfinite(values[n])) return false;
            }
            if (values[3] <= 0 || values[3] > 500) return false;
            options.worldCenter=values;
            continue;
        }
        if (argument != "--smoke-frames" || ++i >= argc) {
            Log("Unknown argument or missing --smoke-frames count");
            return false;
        }
        const std::string_view value{argv[i]};
        const auto result = std::from_chars(value.data(), value.data() + value.size(), options.smokeFrames);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
            options.smokeFrames == 0 || options.smokeFrames > 10000) {
            Log("Smoke frame count must be an integer from 1 to 10000");
            return false;
        }
    }
    return !(options.autoWalk && !options.play) &&
           !(options.play && (options.assets.empty() || !options.worldCenter ||
                              options.archive!="gta3" || options.playerPreview)) &&
           !(options.playerPreview && (options.worldCenter || options.instanceId ||
                                       options.archive!="player")) &&
           (options.archive=="gta3" || (!options.worldCenter && !options.instanceId));
}

struct Vertex { float x, y, z, r, g, b, u, v, a; };
static_assert(sizeof(Vertex) == 36, "Metal vertex layout must match nine packed floats");
struct TexturePixels {
    std::string name;
    int width{}, height{};
    std::vector<std::uint8_t> rgba;
    bool hasAlpha{};
};
struct DrawGroup { std::size_t first{}, count{}; int textureIndex{}; bool transparent{}; };
struct Model {
    std::vector<Vertex> vertices;
    std::vector<DrawGroup> groups;
    std::vector<TexturePixels> textures;
};

void AppendModel(Model& destination, Model&& source) {
    const auto vertexOffset=destination.vertices.size();
    std::vector<int> textureMap(source.textures.size());
    for (std::size_t i=0;i<source.textures.size();++i) {
        const auto& texture=source.textures[i];
        const auto found=std::find_if(destination.textures.begin(),destination.textures.end(),
            [&](const auto& existing) {
                return existing.name==texture.name && existing.width==texture.width &&
                       existing.height==texture.height && existing.rgba==texture.rgba;
            });
        if (found == destination.textures.end()) {
            textureMap[i]=static_cast<int>(destination.textures.size());
            destination.textures.push_back(std::move(source.textures[i]));
        } else {
            textureMap[i]=static_cast<int>(found-destination.textures.begin());
        }
    }
    destination.vertices.insert(destination.vertices.end(),source.vertices.begin(),source.vertices.end());
    for (auto group : source.groups) {
        group.first+=vertexOffset;
        if (group.textureIndex) group.textureIndex=textureMap[group.textureIndex-1]+1;
        destination.groups.push_back(group);
    }
}

void PlaceModel(Model& model, const gtasa::world::Instance& instance) {
    // CFileLoader conjugates IPL quaternion imaginary components before
    // applying its rotation (FileLoader.cpp::LoadObjectInstance).
    const auto& q=instance.rotation;
    const float x=-q[0], y=-q[1], z=-q[2], w=q[3];
    for (auto& vertex : model.vertices) {
        const float vx=vertex.x, vy=vertex.y, vz=vertex.z;
        const float cx=y*vz-z*vy, cy=z*vx-x*vz, cz=x*vy-y*vx;
        const float dx=y*cz-z*cy, dy=z*cx-x*cz, dz=x*cy-y*cx;
        vertex.x=vx+2.0f*(w*cx+dx)+instance.position[0];
        vertex.y=vy+2.0f*(w*cy+dy)+instance.position[1];
        vertex.z=vz+2.0f*(w*cz+dz)+instance.position[2];
    }
}

ModelBounds FindBounds(const Model& model) {
    ModelBounds bounds;
    for (const auto& vertex : model.vertices) {
        const std::array<float,3> point{vertex.x,vertex.y,vertex.z};
        for (int axis=0;axis<3;++axis) {
            bounds.minimum[axis]=std::min(bounds.minimum[axis],point[axis]);
            bounds.maximum[axis]=std::max(bounds.maximum[axis],point[axis]);
        }
    }
    return bounds;
}

#ifdef GTASA_HAS_LIBRW
struct RwSession {
    RwSession() {
        if (!rw::Engine::init()) throw std::runtime_error("librw initialization failed");
        rw::registerHAnimPlugin();
        rw::registerSkinPlugin();
        if (!rw::Engine::open(nullptr)) {
            rw::Engine::term();
            throw std::runtime_error("librw open failed");
        }
        if (!rw::Engine::start()) {
            rw::Engine::close(); rw::Engine::term();
            throw std::runtime_error("librw start failed");
        }
    }
    ~RwSession() { rw::Engine::stop(); rw::Engine::close(); rw::Engine::term(); }
};

struct SkinInfluence {
    std::array<std::uint8_t,4> indices{};
    std::array<float,4> weights{};
};

struct SkinCapture {
    std::size_t vertexFirst{};
    std::vector<SkinInfluence> vertices;
    std::vector<rw::Matrix> inverseBind;
    std::vector<rw::Matrix> bindLocal;
    std::vector<rw::V3d> bonePositions;
    std::vector<rw::Matrix> rootParent;
    std::vector<int> parent;
    std::vector<int> boneTags;
    rw::Matrix inverseAtomic{};
};

// CPed::ShoulderBoneRotation uses ZYX Tait-Bryan angles with SWAP_XZ and
// halves the resulting X angle. In that convention X is the raw Z angle.
void HalfShoulderX(rw::Matrix& matrix) {
    constexpr float epsilon=0.0000019073486328125f;
    const float cy=std::hypot(matrix.at.z,matrix.up.z);
    const float x=cy>epsilon ? std::atan2(matrix.right.y,matrix.right.x) :
        std::atan2(-matrix.up.x,matrix.up.y);
    const float y=std::atan2(-matrix.right.z,cy);
    const float z=cy>epsilon ? std::atan2(matrix.up.z,matrix.at.z)*0.5f : 0.0f;
    const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cyy=std::cos(y);
    const float sz=std::sin(z),cz=std::cos(z);
    matrix.at.z=cyy*cz;
    matrix.at.y=-cx*sz+cz*sx*sy;
    matrix.at.x=sx*sz+cx*cz*sy;
    matrix.up.z=cyy*sz;
    matrix.up.y=cx*cz+sx*sy*sz;
    matrix.up.x=-cz*sx+cx*sy*sz;
    matrix.right.z=-sy;
    matrix.right.y=cyy*sx;
    matrix.right.x=cx*cyy;
    matrix.flags=0;
}

std::vector<rw::Matrix> SkinMatrices(const SkinCapture& skin,
                                    const std::vector<gtasa::animation::BonePose>& pose,
                                    const std::vector<gtasa::animation::BonePose>& stancePose,
                                    const std::vector<gtasa::animation::BonePose>& idlePose={},
                                    float idleWeight=0.0f) {
    std::vector<rw::Matrix> world(skin.boneTags.size());
    std::vector<rw::Matrix> result(skin.boneTags.size());
    rw::Matrix outputAlignment;
    outputAlignment.setIdentity();
    for (std::size_t i=0;i<world.size();++i) {
        rw::Matrix local=skin.bindLocal[i];
        for (const auto& bone : pose) if (bone.boneTag==static_cast<std::uint32_t>(skin.boneTags[i])) {
            const rw::Quat q{bone.rotation[0],bone.rotation[1],
                             bone.rotation[2],bone.rotation[3]};
            if (i==0) {
                const auto initial=std::find_if(stancePose.begin(),stancePose.end(),
                    [&](const auto& candidate) { return candidate.boneTag==bone.boneTag; });
                if (initial==stancePose.end()) break;
                const rw::Quat q0{initial->rotation[0],initial->rotation[1],
                                  initial->rotation[2],initial->rotation[3]};
                // RenderWare applies the clip quaternion directly. Align the
                // resulting skinned geometry to the DFF's starting orientation
                // after skinning, rather than changing the animated root basis.
                rw::Matrix rawBase,rawWorld,desiredWorld,inverseRawWorld;
                rw::Matrix rootCorrection,atomic,temporary;
                rawBase.rotate(q0,rw::COMBINEREPLACE);
                rawBase.pos=skin.bindLocal[i].pos;
                rw::Matrix::mult(&rawWorld,&rawBase,&skin.rootParent[i]);
                rw::Matrix::mult(&desiredWorld,&skin.bindLocal[i],&skin.rootParent[i]);
                rw::Matrix::invert(&inverseRawWorld,&rawWorld);
                rw::Matrix::mult(&rootCorrection,&inverseRawWorld,&desiredWorld);
                rw::Matrix::invert(&atomic,&skin.inverseAtomic);
                rw::Matrix::mult(&temporary,&atomic,&rootCorrection);
                rw::Matrix::mult(&outputAlignment,&temporary,&skin.inverseAtomic);
                local.rotate(q,rw::COMBINEREPLACE);
                const auto idle=std::find_if(idlePose.begin(),idlePose.end(),
                    [&](const auto& candidate) { return candidate.boneTag==bone.boneTag; });
                // Walk/run extract forward (Y) travel, but retain root X sway and Z.
                local.pos.x+=bone.translation[0]-initial->translation[0];
                local.pos.z+=bone.translation[2]-initial->translation[2];
                if (idle!=idlePose.end()) {
                    local.pos.y+=idleWeight*(idle->translation[1]-initial->translation[1]);
                }
            } else {
                local.rotate(q,rw::COMBINEREPLACE);
                local.pos=skin.bonePositions[i];
            }
            break;
        }
        const int parent=skin.parent[i];
        if (parent>=static_cast<int>(i))
            throw std::runtime_error("Player skin bones are not parent ordered");
        rw::Matrix::mult(&world[i],&local,
            parent>=0 ? &world[parent] : &skin.rootParent[i]);
    }
    const auto shoulder=[&](int breastTag,int armTag,int clavicleTag) {
        const auto index=[&](int tag) {
            const auto found=std::find(skin.boneTags.begin(),skin.boneTags.end(),tag);
            return found==skin.boneTags.end() ? -1 : int(found-skin.boneTags.begin());
        };
        const int breast=index(breastTag),arm=index(armTag),clavicle=index(clavicleTag);
        if (breast<0 || arm<0 || clavicle<0) return;
        rw::Matrix inverseClavicle,relative;
        rw::Matrix::invert(&inverseClavicle,&world[clavicle]);
        rw::Matrix::mult(&relative,&world[arm],&inverseClavicle);
        HalfShoulderX(relative);
        rw::Matrix::mult(&world[breast],&relative,&world[clavicle]);
    };
    shoulder(302,32,31);
    shoulder(301,22,21);
    for (std::size_t i=0;i<world.size();++i) {
        rw::Matrix inverse=skin.inverseBind[i],tmp;
        inverse.flags=0;
        rw::Matrix::mult(&tmp,&world[i],&skin.inverseAtomic);
        rw::Matrix::mult(&result[i],&inverse,&tmp);
        rw::Matrix adjusted;
        rw::Matrix::mult(&adjusted,&result[i],&outputAlignment);
        result[i]=adjusted;
    }
    return result;
}

std::array<float,3> SkinPoint(const rw::Matrix& matrix,const Vertex& vertex) {
    return {vertex.x*matrix.right.x+vertex.y*matrix.up.x+vertex.z*matrix.at.x+matrix.pos.x,
            vertex.x*matrix.right.y+vertex.y*matrix.up.y+vertex.z*matrix.at.y+matrix.pos.y,
            vertex.x*matrix.right.z+vertex.y*matrix.up.z+vertex.z*matrix.at.z+matrix.pos.z};
}

Model LoadModel(const gtasa::assets::ImgDirectory& archive, const std::string& modelName,
                const std::string& txdName, bool clothingTexture=false,
                SkinCapture* skinCapture=nullptr) {
    static RwSession session;
    auto bytes = archive.Read(modelName + ".dff");
    auto textureBytes = archive.Read(txdName + ".txd");
    rw::StreamMemory textureStream;
    textureStream.open(textureBytes.data(), static_cast<rw::uint32>(textureBytes.size()));
    rw::TexDictionary* dictionary = nullptr;
    if (rw::findChunk(&textureStream, rw::ID_TEXDICTIONARY, nullptr, nullptr))
        dictionary = rw::TexDictionary::streamRead(&textureStream);
    textureStream.close();
    if (!dictionary) throw std::runtime_error("Could not decode TXD");
    rw::TexDictionary::setCurrent(dictionary);
    Model model;
    for (rw::LLLink* link=dictionary->textures.link.next;
         link != dictionary->textures.end(); link=link->next) {
        auto* texture=rw::Texture::fromDict(link);
        model.textures.push_back(TexturePixels{});
        auto& target=model.textures.back();
        if (!texture || !texture->raster) continue;
        target.name=texture->name;
        auto* raster = texture->raster;
        const int format = raster->format & 0xF00;
        const auto* nativeRaster=GETD3DRASTEREXT(raster);
        if (nativeRaster->customFormat) {
            const auto* compressed=raster->lock(0,rw::Raster::LOCKREAD);
            if (compressed) {
                target.width=raster->width; target.height=raster->height;
                const auto byteCount=rw::d3d::getLevelSize(raster,0);
                if (byteCount>0)
                    target.rgba=gtasa::assets::DecodeDxt(compressed,byteCount,
                        target.width,target.height,nativeRaster->format);
                for (std::size_t pixel=3;pixel<target.rgba.size();pixel+=4)
                    if (target.rgba[pixel]<255) { target.hasAlpha=true; break; }
                raster->unlock(0);
            }
            continue;
        }
        const bool packed16=nativeRaster->bpp==2 && raster->depth==16 &&
            (format==rw::Raster::C1555 || format==rw::Raster::C565 ||
             format==rw::Raster::C4444);
        const bool packed32=nativeRaster->bpp==4 && raster->depth==32 &&
            (format==rw::Raster::C888 || format==rw::Raster::C8888);
        if (!packed16 && !packed32)
            continue;
        auto* source = raster->lock(0, rw::Raster::LOCKREAD);
        if (!source) continue;
        target.width = raster->width; target.height = raster->height;
        const auto levelBytes=rw::d3d::getLevelSize(raster,0);
        if (target.width<=0 || target.height<=0 || target.width>8192 || target.height>8192 ||
            raster->stride<target.width*int(nativeRaster->bpp) || levelBytes<0 ||
            std::uint64_t(raster->stride)*target.height>std::uint64_t(levelBytes)) {
            raster->unlock(0);
            continue;
        }
        target.rgba.resize(std::size_t(target.width)*target.height*4);
        for (int y = 0; y < target.height; ++y) for (int x = 0; x < target.width; ++x) {
            const auto* pixelSource=source+y*raster->stride+x*nativeRaster->bpp;
            const auto offset = (std::size_t(y)*target.width+x)*4;
            if (packed32) {
                target.rgba[offset]=pixelSource[2];
                target.rgba[offset+1]=pixelSource[1];
                target.rgba[offset+2]=pixelSource[0];
                target.rgba[offset+3]=format==rw::Raster::C8888 ? pixelSource[3] : 255;
                continue;
            }
            std::uint16_t pixel;
            std::memcpy(&pixel,pixelSource,2);
            if (format == rw::Raster::C565) {
                target.rgba[offset] = ((pixel >> 11) & 31)*255/31;
                target.rgba[offset+1] = ((pixel >> 5) & 63)*255/63;
                target.rgba[offset+2] = (pixel & 31)*255/31;
                target.rgba[offset+3] = 255;
            } else if (format==rw::Raster::C4444) {
                target.rgba[offset] = ((pixel >> 8) & 15)*17;
                target.rgba[offset+1] = ((pixel >> 4) & 15)*17;
                target.rgba[offset+2] = (pixel & 15)*17;
                target.rgba[offset+3] = ((pixel >> 12) & 15)*17;
            } else {
                target.rgba[offset] = ((pixel >> 10) & 31)*255/31;
                target.rgba[offset+1] = ((pixel >> 5) & 31)*255/31;
                target.rgba[offset+2] = (pixel & 31)*255/31;
                target.rgba[offset+3] = (pixel & 0x8000) ? 255 : 0;
            }
        }
        raster->unlock(0);
        for (std::size_t pixel=3;pixel<target.rgba.size();pixel+=4)
            if (target.rgba[pixel]<255) { target.hasAlpha=true; break; }
    }
    rw::StreamMemory stream;
    stream.open(bytes.data(), static_cast<rw::uint32>(bytes.size()));
    rw::Clump* clump = nullptr;
    if (rw::findChunk(&stream, rw::ID_CLUMP, nullptr, nullptr))
        clump = rw::Clump::streamRead(&stream);
    stream.close();
    if (!clump) throw std::runtime_error("Could not decode DFF");
    const int textureSlots=static_cast<int>(model.textures.size())+1;
    std::vector<std::vector<Vertex>> groups(textureSlots*2);
    std::vector<std::vector<SkinInfluence>> skinGroups;
    if (skinCapture) skinGroups.resize(groups.size());
    const bool vehicle=modelName=="infernus";
    int atomicIndex=0;
    for (rw::LLLink* link = clump->atomics.link.next; link != clump->atomics.end(); link = link->next, ++atomicIndex) {
        const auto* atomic = rw::Atomic::fromClump(link);
        const auto* geometry = atomic->geometry;
        if (!geometry || !geometry->morphTargets || !geometry->morphTargets[0].vertices) continue;
        const rw::Matrix* frame = atomic->getFrame()->getLTM();
        const rw::Skin* skin=nullptr;
        if (skinCapture) {
            if (atomicIndex!=0) throw std::runtime_error("Player clothing has multiple atomics");
            skin=rw::Skin::get(geometry);
            auto* hierarchy=rw::HAnimHierarchy::get(clump);
            if (!skin || !skin->indices || !skin->weights || !skin->inverseMatrices ||
                !hierarchy || skin->numBones<=0 || skin->numBones!=hierarchy->numNodes ||
                skin->numBones>64)
                throw std::runtime_error("Player clothing skin is incomplete");
            hierarchy->attach();
            skinCapture->inverseBind.assign(reinterpret_cast<rw::Matrix*>(skin->inverseMatrices),
                reinterpret_cast<rw::Matrix*>(skin->inverseMatrices)+skin->numBones);
            skinCapture->bindLocal.resize(skin->numBones);
            skinCapture->rootParent.resize(skin->numBones);
            skinCapture->parent.assign(skin->numBones,-1);
            skinCapture->boneTags.resize(skin->numBones);
            for (int bone=0;bone<skin->numBones;++bone) {
                auto* nodeFrame=hierarchy->nodeInfo[bone].frame;
                if (!nodeFrame) throw std::runtime_error("Player skin bone has no frame");
                skinCapture->boneTags[bone]=hierarchy->nodeInfo[bone].id;
                skinCapture->bindLocal[bone]=nodeFrame->matrix;
                auto* parentFrame=nodeFrame->getParent();
                for (int candidate=0;candidate<skin->numBones;++candidate)
                    if (hierarchy->nodeInfo[candidate].frame==parentFrame)
                        skinCapture->parent[bone]=candidate;
                if (skinCapture->parent[bone]<0) {
                    if (parentFrame) skinCapture->rootParent[bone]=*parentFrame->getLTM();
                    else skinCapture->rootParent[bone].setIdentity();
                }
            }
            rw::Matrix::invert(&skinCapture->inverseAtomic,frame);
            skinCapture->bonePositions.resize(skin->numBones);
            skinCapture->bonePositions[0]={0.0f,0.0f,0.0f};
            for (int bone=1;bone<skin->numBones;++bone) {
                const int parent=skinCapture->parent[bone];
                if (parent<0 || parent>=bone)
                    throw std::runtime_error("Player skin bone hierarchy is invalid");
                rw::Matrix boneToSkin;
                rw::Matrix::invert(&boneToSkin,&skinCapture->inverseBind[bone]);
                const auto& point=boneToSkin.pos;
                const auto& parentInverse=skinCapture->inverseBind[parent];
                skinCapture->bonePositions[bone]={
                    point.x*parentInverse.right.x+point.y*parentInverse.up.x+
                        point.z*parentInverse.at.x+parentInverse.pos.x,
                    point.x*parentInverse.right.y+point.y*parentInverse.up.y+
                        point.z*parentInverse.at.y+parentInverse.pos.y,
                    point.x*parentInverse.right.z+point.y*parentInverse.up.z+
                        point.z*parentInverse.at.z+parentInverse.pos.z};
            }
            for (int bone=0;bone<skin->numBones;++bone) {
                const auto& bind=skinCapture->bindLocal[bone].pos;
                const auto& animated=skinCapture->bonePositions[bone];
                if (std::hypot(bind.x-animated.x,bind.y-animated.y,bind.z-animated.z)>0.02f)
                    throw std::runtime_error("Player skin bone positions do not match DFF hierarchy");
            }
            const auto rest=SkinMatrices(*skinCapture,{},{});
            for (const auto& matrix : rest) {
                const float error=std::max({std::abs(matrix.right.x-1.0f),
                    std::abs(matrix.up.y-1.0f),std::abs(matrix.at.z-1.0f),
                    std::abs(matrix.right.y),std::abs(matrix.right.z),
                    std::abs(matrix.up.x),std::abs(matrix.up.z),
                    std::abs(matrix.at.x),std::abs(matrix.at.y),
                    std::abs(matrix.pos.x),std::abs(matrix.pos.y),std::abs(matrix.pos.z)});
                if (error>0.02f)
                    throw std::runtime_error("Player skin bind matrices do not match frames");
            }
        }
        // The DFF stores one wheel mesh; the original vehicle renderer instances
        // it at all four wheel dummy frames rather than storing four atomics.
        constexpr float wheelPositions[4][3] = {
            {0.95f,1.71f,-0.35f}, {-0.95f,1.71f,-0.35f},
            {0.95f,-1.71f,-0.35f}, {-0.95f,-1.71f,-0.35f}};
        for (int wheelCopy=0; wheelCopy<(vehicle && atomicIndex == 0 ? 4 : 1); ++wheelCopy)
        for (int i = 0; i < geometry->numTriangles; ++i) {
            const auto& triangle = geometry->triangles[i];
            if (triangle.matId >= geometry->matList.numMaterials) continue;
            const auto* material = geometry->matList.materials[triangle.matId];
            float r = material->color.red / 255.0f;
            float g = material->color.green / 255.0f;
            float b = material->color.blue / 255.0f;
            if (vehicle && g > 0.9f && r < 0.3f && b < 0.1f) {
                r=0.16f; g=0.31f; b=0.70f;
            }
            if (vehicle && atomicIndex == 0 && triangle.matId == 1) {
                // Tire rubber is untinted white in this DFF; the game applies dark vehicle lighting.
                r=g=b=0.08f;
            }
            if (vehicle && atomicIndex == 14 && triangle.matId == 13) {
                // Untextured cabin and underside should not be a bright white mass.
                r=0.15f; g=0.17f; b=0.20f;
            }
            if (vehicle && atomicIndex == 14 && (triangle.matId == 9 || triangle.matId == 12)) {
                // The DFF stores window polygons as plain white; vehicle rendering tints them.
                r=0.10f; g=0.17f; b=0.24f;
            }
            int textureIndex = clothingTexture && !model.textures.empty() ? 1 : 0;
            if (material->texture) for (int t = 0; t < static_cast<int>(model.textures.size()); ++t)
                if (material->texture->name == model.textures[t].name) textureIndex=t+1;
            const bool transparent = material->color.alpha < 255 ||
                (textureIndex && model.textures[textureIndex-1].hasAlpha);
            if (vehicle && transparent && !material->texture) {
                // The vehicle glass material is white in the DFF and tinted by the game.
                r=0.10f; g=0.16f; b=0.22f;
            } else if (vehicle && !material->texture && r > 0.95f && g > 0.95f && b > 0.95f) {
                // Plain white is a placeholder for vehicle lighting, not white body paint.
                r=0.22f; g=0.27f; b=0.33f;
            }
            auto& group = groups[textureIndex + (transparent ? textureSlots : 0)];
            auto* skinGroup=skinCapture ? &skinGroups[textureIndex + (transparent ? textureSlots : 0)] : nullptr;
            for (unsigned corner = 0; corner < 3; ++corner) {
                if (triangle.v[corner] >= geometry->numVertices) continue;
                const auto& p = geometry->morphTargets[0].vertices[triangle.v[corner]];
                const auto* uv = geometry->texCoords[0];
                float light=0.75f;
                if (geometry->morphTargets[0].normals) {
                    const auto& n = geometry->morphTargets[0].normals[triangle.v[corner]];
                    const float nx=clothingTexture ? n.x : frame->right.x*n.x+frame->up.x*n.y+frame->at.x*n.z;
                    const float ny=clothingTexture ? n.y : frame->right.y*n.x+frame->up.y*n.y+frame->at.y*n.z;
                    const float nz=clothingTexture ? n.z : frame->right.z*n.x+frame->up.z*n.y+frame->at.z*n.z;
                    light=0.38f+0.62f*std::max(0.0f, -0.3f*nx-0.4f*ny+0.86f*nz);
                }
                const float dx=vehicle && atomicIndex == 0 ? wheelPositions[wheelCopy][0]-frame->pos.x : 0.0f;
                const float dy=vehicle && atomicIndex == 0 ? wheelPositions[wheelCopy][1]-frame->pos.y : 0.0f;
                const float dz=vehicle && atomicIndex == 0 ? wheelPositions[wheelCopy][2]-frame->pos.z : 0.0f;
                group.push_back({
                    clothingTexture ? p.x : frame->right.x*p.x + frame->up.x*p.y + frame->at.x*p.z + frame->pos.x + dx,
                    clothingTexture ? p.y : frame->right.y*p.x + frame->up.y*p.y + frame->at.y*p.z + frame->pos.y + dy,
                    clothingTexture ? p.z : frame->right.z*p.x + frame->up.z*p.y + frame->at.z*p.z + frame->pos.z + dz,
                    r*light, g*light, b*light, uv ? uv[triangle.v[corner]].u : 0.0f,
                    uv ? uv[triangle.v[corner]].v : 0.0f,
                    material->color.alpha/255.0f});
                if (skinGroup) {
                    SkinInfluence influence;
                    const auto vertex=triangle.v[corner];
                    for (int weight=0;weight<4;++weight) {
                        influence.indices[weight]=skin->indices[vertex*4+weight];
                        influence.weights[weight]=skin->weights[vertex*4+weight];
                        if (influence.weights[weight]>0.0f &&
                            influence.indices[weight]>=skin->numBones)
                            throw std::runtime_error("Player skin bone index outside hierarchy");
                    }
                    skinGroup->push_back(influence);
                }
            }
        }
    }
    clump->destroy();
    rw::TexDictionary::setCurrent(nullptr);
    dictionary->destroy();
    // TXDs often contain textures for several models. Keep only those actually
    // referenced by this DFF so the world preview does not upload entire TXDs.
    std::vector<int> textureMap(model.textures.size()+1,0);
    std::vector<TexturePixels> usedTextures;
    for (std::size_t i=1;i<static_cast<std::size_t>(textureSlots);++i) {
        if (groups[i].empty() && groups[i+textureSlots].empty()) continue;
        textureMap[i]=static_cast<int>(usedTextures.size())+1;
        usedTextures.push_back(std::move(model.textures[i-1]));
    }
    model.textures=std::move(usedTextures);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].empty()) continue;
        model.groups.push_back({model.vertices.size(),groups[i].size(),
                                textureMap[i%textureSlots],i>=static_cast<std::size_t>(textureSlots)});
        model.vertices.insert(model.vertices.end(), groups[i].begin(), groups[i].end());
        if (skinCapture)
            skinCapture->vertices.insert(skinCapture->vertices.end(),
                skinGroups[i].begin(),skinGroups[i].end());
    }
    if (model.vertices.empty()) throw std::runtime_error("DFF has no renderable triangles");
    return model;
}

struct StaleRegion final : std::exception {};

Model BuildWorldModel(const gtasa::world::Region& world,
                      const gtasa::assets::ImgDirectory& archive,
                      const std::array<float,4>& center,
                      std::unordered_map<int,Model>& modelCache,
                      std::size_t& cacheBytes,
                      const std::atomic<float>& desiredX,
                      const std::atomic<float>& desiredY) {
    Model scene;
    constexpr float geometryMargin=160.0f;
    auto nearby=gtasa::world::InstancesNear(world,{center[0],center[1],center[2]},
                                             center[3]+geometryMargin);
    std::sort(nearby.begin(),nearby.end(),[&](const auto* a,const auto* b) {
        const auto dist=[&](const auto* item) {
            const double x=item->position[0]-center[0], y=item->position[1]-center[1];
            return x*x+y*y;
        };
        return dist(a)<dist(b);
    });
    std::size_t loaded=0, missing=0, failed=0, edgeRejected=0;
    std::array<std::size_t,4> loadedByPass{};
    std::unordered_set<std::int32_t> representedLods;
    // Load terrain first, then the detailed children of LOD buildings before
    // small props. A LOD is a fallback only when none of its detail is loaded.
    for (int pass=0;pass<4;++pass) for (const auto* instance : nearby) {
        const double focusX=desiredX.load(std::memory_order_relaxed)-center[0];
        const double focusY=desiredY.load(std::memory_order_relaxed)-center[1];
        if (focusX*focusX+focusY*focusY>=25.0*25.0) throw StaleRegion{};
        if (pass==3) {
            if (!instance->lodChildren ||
                representedLods.contains(static_cast<std::int32_t>(instance-world.instances.data()))) continue;
        } else {
            if (instance->lodChildren) continue;
            if (pass==0 && instance->modelId<16000) continue;
            if (pass==1 && (instance->modelId>=16000 || instance->lodTarget<0)) continue;
            if (pass==2 && (instance->modelId>=16000 || instance->lodTarget>=0)) continue;
        }
        const double ox=double(instance->position[0])-center[0];
        const double oy=double(instance->position[1])-center[1];
        const double oz=double(instance->position[2])-center[2];
        const bool originInside=ox*ox+oy*oy+oz*oz<=double(center[3])*center[3];
        const auto& type=world.types.at(instance->modelId);
        const auto drawOrigin=instance->lodTarget>=0 ?
            world.instances[instance->lodTarget].position : instance->position;
        if (!gtasa::world::WithinLoadedDrawDistance(type.drawDistance,
                {center[0],center[1],center[2]},drawOrigin)) continue;
        if (pass!=3 && type.model.size()>=3 &&
            std::tolower(type.model[0])=='l' && std::tolower(type.model[1])=='o' &&
            std::tolower(type.model[2])=='d') continue;
        if (!archive.Find(type.model+".dff") ||
            !archive.Find(type.textureDictionary+".txd")) { ++missing; continue; }
        try {
            Model object;
            const auto cached=modelCache.find(instance->modelId);
            if (cached!=modelCache.end()) {
                object=cached->second;
            } else {
                object=LoadModel(archive,type.model,type.textureDictionary);
                std::size_t bytes=object.vertices.size()*sizeof(Vertex)+
                                  object.groups.size()*sizeof(DrawGroup);
                for (const auto& texture : object.textures) bytes+=texture.rgba.size();
                constexpr std::size_t cacheLimit=256u*1024u*1024u;
                if (bytes<=cacheLimit-cacheBytes) {
                    modelCache.emplace(instance->modelId,object);
                    cacheBytes+=bytes;
                }
            }
            PlaceModel(object,*instance);
            if (!originInside) {
                float minX=INFINITY,maxX=-INFINITY,minY=INFINITY,maxY=-INFINITY;
                for (const auto& vertex : object.vertices) {
                    minX=std::min(minX,vertex.x); maxX=std::max(maxX,vertex.x);
                    minY=std::min(minY,vertex.y); maxY=std::max(maxY,vertex.y);
                }
                const double closestX=std::clamp(double(center[0]),double(minX),double(maxX));
                const double closestY=std::clamp(double(center[1]),double(minY),double(maxY));
                const double dx=closestX-center[0],dy=closestY-center[1];
                if (!std::isfinite(dx) || !std::isfinite(dy) ||
                    dx*dx+dy*dy>double(center[3])*center[3]) {
                    ++edgeRejected;
                    continue;
                }
            }
            AppendModel(scene,std::move(object));
            if (pass!=3 && instance->lodTarget>=0)
                representedLods.insert(instance->lodTarget);
            ++loaded;
            ++loadedByPass[pass];
            std::fprintf(stderr,"[native]   world id=%d model=%s pos=(%.1f,%.1f,%.1f)\n",
                instance->modelId,type.model.c_str(),instance->position[0],
                instance->position[1],instance->position[2]);
        } catch (const std::exception& error) {
            ++failed;
            std::fprintf(stderr,"[native] World object %d skipped: %s\n",instance->modelId,error.what());
        }
    }
    if (!loaded) throw std::runtime_error("No nearby world model could be loaded");
    std::fprintf(stderr,"[native] World preview: center=(%.0f,%.0f,%.0f) candidates=%zu loaded=%zu (static=%zu detailed=%zu props=%zu lod=%zu) edgeRejected=%zu missing=%zu failed=%zu\n",
        center[0],center[1],center[2],nearby.size(),loaded,
        loadedByPass[0],loadedByPass[1],loadedByPass[2],loadedByPass[3],edgeRejected,missing,failed);
    return scene;
}
#endif
}

@interface NativeWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic) BOOL shouldClose;
@end
@implementation NativeWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    self.shouldClose = YES;
    return NO;
}
@end

int main(int argc, const char* argv[]) {
    Options options;
    if (!ParseOptions(argc, argv, options)) {
        return 2;
    }
    @autoreleasepool {
        Log("Starting ARM64 native renderer");
        CMaths::InitMathsTables();
        NSApplication* application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyRegular];
        [application finishLaunching];

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            Log("ERROR: No Metal device is available");
            return 1;
        }
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) {
            Log("ERROR: Could not create Metal command queue");
            return 1;
        }
        std::fprintf(stderr, "[native] Metal device: %s\n", device.name.UTF8String);

        Model model;
#ifdef GTASA_HAS_LIBRW
        std::optional<gtasa::assets::ImgDirectory> archive;
        std::optional<gtasa::world::Region> world;
        std::optional<gtasa::collision::CollisionCatalog> collision;
        Model playerTemplate;
        float playerBindMinZ{};
        std::optional<gtasa::native::PlayerController> player;
        std::vector<SkinCapture> playerSkins;
        std::optional<gtasa::animation::ClipPlayback> walkClip;
        std::optional<gtasa::animation::ClipPlayback> runClip;
        std::optional<gtasa::animation::ClipPlayback> idleClip;
        std::vector<gtasa::animation::BonePose> stanceBasePose;
        float walkSeconds{};
        float runSeconds{};
        float idleSeconds{};
        float walkBlend{};
        std::optional<float> idleGrounding;
        float runBlend{1.0f};
        bool playerWalking{};
        std::size_t playerVertexFirst{},playerGroupFirst{};
        std::array<float,3> playerDrawPosition{};
        float playerDrawHeading{};
        const auto appendPlayer=[&](Model& scene) {
            if (!player) return;
            playerVertexFirst=scene.vertices.size();
            playerGroupFirst=scene.groups.size();
            Model body=playerTemplate;
            playerDrawPosition=player->Position();
            playerDrawHeading=player->Heading();
            const float c=std::cos(playerDrawHeading),s=std::sin(playerDrawHeading);
            for (auto& vertex : body.vertices) {
                const float x=vertex.x,y=vertex.y;
                vertex.x=c*x-s*y+playerDrawPosition[0];
                vertex.y=s*x+c*y+playerDrawPosition[1];
                vertex.z+=playerDrawPosition[2];
            }
            AppendModel(scene,std::move(body));
        };
        std::unordered_map<int,Model> worldModelCache;
        std::size_t worldCacheBytes{};
        std::atomic<float> desiredFocusX{options.worldCenter ? (*options.worldCenter)[0] : 0.0f};
        std::atomic<float> desiredFocusY{options.worldCenter ? (*options.worldCenter)[1] : 0.0f};
        if (!options.assets.empty()) {
            try {
                archive.emplace(gtasa::assets::ImgDirectory::Open(
                    options.assets / "models" / (options.archive+".img")));
                std::optional<gtasa::world::Instance> placement;
                if (options.playerPreview) {
                    for (const auto* part : {"head","torso","legs","feet","hands"})
                        AppendModel(model,LoadModel(*archive,part,part,true));
                    options.model="CJ clothing preview";
                } else if (options.worldCenter) {
                    world.emplace(gtasa::world::LoadWorldCatalog(options.assets));
                    collision.emplace(gtasa::collision::CollisionCatalog::Load(*archive));
                    std::fprintf(stderr,"[native] Collision catalog: %zu models, %zu triangles\n",
                        collision->Size(),collision->TriangleCount());
                    const auto& focus=*options.worldCenter;
                    if (const auto ground=gtasa::collision::GroundAt(*world,*collision,
                            {focus[0],focus[1],focus[2]+2.0f}))
                        std::fprintf(stderr,"[native] Ground at focus: z=%.3f model=%s\n",
                            ground->height,ground->modelName.c_str());
                    model=BuildWorldModel(*world,*archive,*options.worldCenter,
                                          worldModelCache,worldCacheBytes,
                                          desiredFocusX,desiredFocusY);
                    if (options.play) {
                        const auto playerArchive=gtasa::assets::ImgDirectory::Open(
                            options.assets / "models/player.img");
                        for (const auto* part : {"head","torso","legs","feet","hands"}) {
                            SkinCapture skin;
                            skin.vertexFirst=playerTemplate.vertices.size();
                            AppendModel(playerTemplate,LoadModel(playerArchive,part,part,true,&skin));
                            if (skin.vertices.size()!=playerTemplate.vertices.size()-skin.vertexFirst)
                                throw std::runtime_error("Player skin vertex count mismatch");
                            playerSkins.push_back(std::move(skin));
                        }
                        std::ifstream ifpFile(options.assets / "anim/ped.ifp",std::ios::binary);
                        if (!ifpFile) throw std::runtime_error("Could not open ped.ifp");
                        const std::vector<std::uint8_t> ifpBytes{
                            std::istreambuf_iterator<char>(ifpFile),{}};
                        const auto ifp=gtasa::animation::ReadAnp3(ifpBytes);
                        const auto walk=std::find_if(ifp.clips.begin(),ifp.clips.end(),
                            [](const auto& clip) { return clip.name=="WALK_player"; });
                        if (walk==ifp.clips.end())
                            throw std::runtime_error("WALK_player absent from ped.ifp");
                        walkClip.emplace(ifpBytes,*walk);
                        const auto walkingSpeed=walkClip->RootTravelSpeed();
                        if (!walkingSpeed || *walkingSpeed>10.0f)
                            throw std::runtime_error("Invalid WALK_player root travel speed");
                        const auto run=std::find_if(ifp.clips.begin(),ifp.clips.end(),
                            [](const auto& clip) { return clip.name=="run_player"; });
                        if (run==ifp.clips.end())
                            throw std::runtime_error("run_player absent from ped.ifp");
                        runClip.emplace(ifpBytes,*run);
                        const auto runningSpeed=runClip->RootTravelSpeed();
                        if (!runningSpeed || *runningSpeed>10.0f)
                            throw std::runtime_error("Invalid run_player root travel speed");
                        const auto idle=std::find_if(ifp.clips.begin(),ifp.clips.end(),
                            [](const auto& clip) { return clip.name=="IDLE_stance"; });
                        if (idle==ifp.clips.end())
                            throw std::runtime_error("IDLE_stance absent from ped.ifp");
                        idleClip.emplace(ifpBytes,*idle);
                        stanceBasePose=idleClip->Sample(0.0f);
                        const auto bounds=FindBounds(playerTemplate);
                        playerBindMinZ=bounds.minimum[2];
                        const auto ground=gtasa::collision::GroundAt(*world,*collision,
                            {focus[0],focus[1],focus[2]+2.0f});
                        if (!ground) throw std::runtime_error("No collision ground at player spawn");
                        player.emplace(std::array<float,3>{focus[0],focus[1],
                                       ground->height-bounds.minimum[2]},bounds.minimum[2],
                                       *walkingSpeed);
                        (*options.worldCenter)[2]=player->Position()[2]+0.8f;
                        appendPlayer(model);
                    }
                    options.model=options.play ? "CJ in world" : "World preview";
                } else if (options.instanceId) {
                    world.emplace(gtasa::world::LoadWorldCatalog(options.assets));
                    const auto type=world->types.find(*options.instanceId);
                    if (type == world->types.end()) throw std::runtime_error("World model ID absent from IDE catalog");
                    const auto found=std::find_if(world->instances.begin(),world->instances.end(),
                        [&](const auto& candidate) { return candidate.modelId==*options.instanceId && candidate.interior==0; });
                    if (found == world->instances.end()) throw std::runtime_error("World model ID has no exterior IPL instance");
                    options.model=type->second.model;
                    options.txd=type->second.textureDictionary;
                    placement=*found;
                    std::fprintf(stderr,"[native] IPL instance %d at (%.2f,%.2f,%.2f)\n",
                        *options.instanceId,found->position[0],found->position[1],found->position[2]);
                }
                if (!options.worldCenter && !options.playerPreview) {
                    model = LoadModel(*archive, options.model, options.txd);
                    if (placement) PlaceModel(model,*placement);
                }
            }
            catch (const std::exception& error) {
                std::fprintf(stderr, "[native] Asset ERROR: %s\n", error.what());
                return 1;
            }
            std::fprintf(stderr, "[native] %s loaded: %zu triangles, %zu textures\n",
                options.model.c_str(), model.vertices.size()/3, model.textures.size());
        }
#endif
        id<MTLBuffer> vertexBuffer = nil;
        id<MTLRenderPipelineState> pipeline = nil;
        id<MTLDepthStencilState> depthState = nil;
        id<MTLDepthStencilState> transparentDepthState = nil;
        std::vector<id<MTLTexture>> metalTextures;
        id<MTLSamplerState> sampler = nil;
        ViewControls viewControls;
        if (options.play) {
            viewControls.yaw=-1.57079632679f; // initial camera behind +X facing CJ
            viewControls.elevation=0.22f;
        }
        ModelBounds modelBounds;
        std::vector<ModelBounds> groupBounds;
        std::vector<std::array<float,3>> groupCenters;
        std::vector<std::size_t> transparentOrder;
        const auto uploadScene=[&]() -> bool {
            modelBounds=FindBounds(model);
            groupBounds.assign(model.groups.size(),{});
            groupCenters.assign(model.groups.size(),{});
            transparentOrder.clear();
            for (std::size_t i=0;i<model.groups.size();++i) {
                const auto& group=model.groups[i];
                if (!group.count) continue;
                auto& bounds=groupBounds[i];
                const auto& first=model.vertices[group.first];
                bounds.minimum={first.x,first.y,first.z};
                bounds.maximum=bounds.minimum;
                for (std::size_t j=group.first+1;j<group.first+group.count;++j) {
                    const auto& vertex=model.vertices[j];
                    const std::array<float,3> point{vertex.x,vertex.y,vertex.z};
                    for (int axis=0;axis<3;++axis) {
                        bounds.minimum[axis]=std::min(bounds.minimum[axis],point[axis]);
                        bounds.maximum[axis]=std::max(bounds.maximum[axis],point[axis]);
                    }
                }
                for (int axis=0;axis<3;++axis)
                    groupCenters[i][axis]=(bounds.minimum[axis]+bounds.maximum[axis])*0.5f;
                if (group.transparent) transparentOrder.push_back(i);
            }
            vertexBuffer=[device newBufferWithBytes:model.vertices.data() length:model.vertices.size()*sizeof(Vertex)
                                          options:MTLResourceStorageModeShared];
            if (!vertexBuffer) { Log("ERROR: Could not allocate Metal vertex buffer"); return false; }
            metalTextures.assign(model.textures.size(),nil);
            for (std::size_t i = 0; i < model.textures.size(); ++i) {
                const auto& image=model.textures[i];
                if (image.rgba.empty()) continue;
                MTLTextureDescriptor* textureDescriptor=[MTLTextureDescriptor
                    texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                    width:image.width height:image.height mipmapped:YES];
                textureDescriptor.usage=MTLTextureUsageShaderRead;
                metalTextures[i]=[device newTextureWithDescriptor:textureDescriptor];
                if (!metalTextures[i]) { Log("ERROR: Could not allocate Metal texture"); return false; }
                [metalTextures[i] replaceRegion:MTLRegionMake2D(0,0,image.width,image.height)
                    mipmapLevel:0 withBytes:image.rgba.data() bytesPerRow:image.width*4];
            }
            id<MTLCommandBuffer> mipCommand=[queue commandBuffer];
            id<MTLBlitCommandEncoder> mipBlit=[mipCommand blitCommandEncoder];
            for (id<MTLTexture> texture : metalTextures)
                if (texture) [mipBlit generateMipmapsForTexture:texture];
            [mipBlit endEncoding];
            [mipCommand commit];
            [mipCommand waitUntilCompleted];
            if (mipCommand.status == MTLCommandBufferStatusError) {
                std::fprintf(stderr,"[native] Mipmap ERROR: %s\n",mipCommand.error.localizedDescription.UTF8String);
                return false;
            }
            return true;
        };
        const auto updatePlayerGeometry=[&] {
#ifdef GTASA_HAS_LIBRW
            if (!player || !vertexBuffer) return;
            const auto& position=player->Position();
            const float heading=player->Heading();
            const float c=std::cos(heading),s=std::sin(heading);
            auto* gpu=static_cast<Vertex*>(vertexBuffer.contents);
            const auto idlePose=idleClip ? idleClip->Sample(idleSeconds) :
                std::vector<gtasa::animation::BonePose>{};
            const auto walkPose=walkClip ? walkClip->Sample(walkSeconds) :
                std::vector<gtasa::animation::BonePose>{};
            const auto runPose=runClip ? runClip->Sample(runSeconds) :
                std::vector<gtasa::animation::BonePose>{};
            const auto movingPose=gtasa::animation::BlendPoses(walkPose,runPose,runBlend);
            const auto pose=gtasa::animation::BlendPoses(idlePose,movingPose,walkBlend);
            std::vector<std::vector<rw::Matrix>> skinMatrices;
            if (!pose.empty()) {
                skinMatrices.reserve(playerSkins.size());
                for (const auto& skin : playerSkins)
                    skinMatrices.push_back(SkinMatrices(skin,pose,stanceBasePose,idlePose,
                                                        1.0f-walkBlend));
            }
            float minimumSkinnedZ=std::numeric_limits<float>::infinity();
            for (std::size_t i=playerVertexFirst;i<model.vertices.size();++i) {
                auto& vertex=model.vertices[i];
                const auto& original=playerTemplate.vertices[i-playerVertexFirst];
                std::array<float,3> point{original.x,original.y,original.z};
                if (!pose.empty()) for (std::size_t part=0;part<playerSkins.size();++part) {
                    const auto& skin=playerSkins[part];
                    const auto local=i-playerVertexFirst;
                    if (local<skin.vertexFirst || local>=skin.vertexFirst+skin.vertices.size())
                        continue;
                    const auto& influence=skin.vertices[local-skin.vertexFirst];
                    std::array<float,3> weighted{};
                    float total=0.0f;
                    for (int joint=0;joint<4;++joint) {
                        const float weight=influence.weights[joint];
                        if (weight<=0.0f) continue;
                        const auto bone=influence.indices[joint];
                        const auto transformed=SkinPoint(skinMatrices[part][bone],original);
                        for (int axis=0;axis<3;++axis)
                            weighted[axis]+=transformed[axis]*weight;
                        total+=weight;
                    }
                    if (total>0.0f)
                        for (int axis=0;axis<3;++axis) point[axis]=weighted[axis]/total;
                    break;
                }
                vertex.x=c*point[0]-s*point[1]+position[0];
                vertex.y=s*point[0]+c*point[1]+position[1];
                vertex.z=point[2]+position[2];
                minimumSkinnedZ=std::min(minimumSkinnedZ,vertex.z);
            }
            if (std::isfinite(minimumSkinnedZ)) {
                const float sampledGrounding=position[2]+playerBindMinZ-minimumSkinnedZ;
                if (!idleGrounding) idleGrounding=sampledGrounding;
                const float grounding=*idleGrounding*(1.0f-walkBlend)+
                                      sampledGrounding*walkBlend;
                for (std::size_t i=playerVertexFirst;i<model.vertices.size();++i) {
                    model.vertices[i].z+=grounding;
                    gpu[i]=model.vertices[i];
                }
            }
            if (std::getenv("GTASA_IDLE_CONTACT_PROBE") && walkBlend==0.0f &&
                playerSkins.size()>3) {
                const auto& feet=playerSkins[3];
                static std::array<std::size_t,2> contacts{};
                static bool selected=false;
                if (!selected) {
                    std::array<float,2> lowest{INFINITY,INFINITY};
                    for (std::size_t v=0;v<feet.vertices.size();++v) {
                        const auto& influence=feet.vertices[v];
                        for (int side=0;side<2;++side) {
                            bool belongs=false;
                            for (int joint=0;joint<4;++joint) {
                                const int tag=feet.boneTags[influence.indices[joint]];
                                belongs|=influence.weights[joint]>0.5f &&
                                    tag>=(side ? 51 : 41) && tag<=(side ? 54 : 44);
                            }
                            const auto index=playerVertexFirst+feet.vertexFirst+v;
                            if (belongs && model.vertices[index].z<lowest[side]) {
                                lowest[side]=model.vertices[index].z;
                                contacts[side]=index;
                            }
                        }
                    }
                    selected=std::isfinite(lowest[0]) && std::isfinite(lowest[1]);
                }
                if (!selected) throw std::runtime_error("Idle contact probe found no weighted sole vertices");
                const auto& left=model.vertices[contacts[0]];
                const auto& right=model.vertices[contacts[1]];
                std::fprintf(stderr,
                    "[idle-contact] time=%.6f player=%.6f,%.6f,%.6f left=%.6f,%.6f,%.6f right=%.6f,%.6f,%.6f\n",
                    idleSeconds,position[0],position[1],position[2],
                    left.x,left.y,left.z,right.x,right.y,right.z);
            }
            for (std::size_t i=playerGroupFirst;i<model.groups.size();++i) {
                const auto& group=model.groups[i];
                auto& bounds=groupBounds[i];
                if (!group.count) continue;
                const auto& first=model.vertices[group.first];
                bounds.minimum={first.x,first.y,first.z};
                bounds.maximum=bounds.minimum;
                for (std::size_t j=group.first+1;j<group.first+group.count;++j) {
                    const auto& vertex=model.vertices[j];
                    const std::array<float,3> point{vertex.x,vertex.y,vertex.z};
                    for (int axis=0;axis<3;++axis) {
                        bounds.minimum[axis]=std::min(bounds.minimum[axis],point[axis]);
                        bounds.maximum[axis]=std::max(bounds.maximum[axis],point[axis]);
                    }
                }
                for (int axis=0;axis<3;++axis)
                    groupCenters[i][axis]=(bounds.minimum[axis]+bounds.maximum[axis])*0.5f;
            }
            playerDrawPosition=position;
            playerDrawHeading=heading;
#endif
        };
        if (!model.vertices.empty()) {
            if (!uploadScene()) return 1;
            MTLSamplerDescriptor* samplerDescriptor=[MTLSamplerDescriptor new];
            samplerDescriptor.minFilter=MTLSamplerMinMagFilterLinear;
            samplerDescriptor.magFilter=MTLSamplerMinMagFilterLinear;
            samplerDescriptor.mipFilter=MTLSamplerMipFilterLinear;
            samplerDescriptor.sAddressMode=MTLSamplerAddressModeRepeat;
            samplerDescriptor.tAddressMode=MTLSamplerAddressModeRepeat;
            sampler=[device newSamplerStateWithDescriptor:samplerDescriptor];
            // Read nine packed floats directly; camera projection is a small
            // per-frame uniform instead of rewriting the vertex buffer.
            NSString* source=@"#include <metal_stdlib>\nusing namespace metal;\n"
                "struct O { float4 pos [[position]]; float3 color; float2 uv; float alpha; };\n"
                "vertex O vs_main(uint id [[vertex_id]], const device float* v [[buffer(0)]],"
                " constant float* p [[buffer(1)]]) {"
                " uint k=id*9; O o; float x=v[k], y=v[k+1], z=v[k+2];"
                " float sx=p[0]*x+p[1]*y;"
                " float sy=-p[1]*p[3]*x+p[0]*p[3]*y+p[2]*z;"
                " float d=p[1]*p[2]*x-p[0]*p[2]*y+p[3]*z;"
                " if(p[11]>0.5){ float distance=p[12]-d;"
                " o.pos=float4((sx-p[4])*p[13]+p[7]*distance,"
                " (sy-p[5])*p[14]+p[8]*distance,"
                " (distance-p[15])*p[16]/(p[16]-p[15]),distance); }"
                " else o.pos=float4((sx-p[4])*p[6]+p[7],(sy-p[5])*p[6]+p[8],"
                " 0.05+0.90*(p[9]-d)*p[10],1);"
                " o.color=float3(v[k+3],v[k+4],v[k+5]);"
                " o.uv=float2(v[k+6],v[k+7]); o.alpha=v[k+8]; return o; }\n"
                "fragment float4 fs_main(O i [[stage_in]], texture2d<float> tex [[texture(0)]],"
                " sampler samp [[sampler(0)]], constant int& useTex [[buffer(0)]]) {"
                " float4 c=useTex ? tex.sample(samp,i.uv) : float4(1);"
                " return float4(i.color*c.rgb, i.alpha*c.a); }\n";
            NSError* shaderError=nil;
            id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&shaderError];
            if (!library) { std::fprintf(stderr,"[native] Shader ERROR: %s\n",shaderError.localizedDescription.UTF8String); return 1; }
            MTLRenderPipelineDescriptor* descriptor=[MTLRenderPipelineDescriptor new];
            descriptor.vertexFunction=[library newFunctionWithName:@"vs_main"];
            descriptor.fragmentFunction=[library newFunctionWithName:@"fs_main"];
            descriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
            descriptor.colorAttachments[0].blendingEnabled=YES;
            descriptor.colorAttachments[0].rgbBlendOperation=MTLBlendOperationAdd;
            descriptor.colorAttachments[0].alphaBlendOperation=MTLBlendOperationAdd;
            descriptor.colorAttachments[0].sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;
            descriptor.colorAttachments[0].destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
            descriptor.colorAttachments[0].sourceAlphaBlendFactor=MTLBlendFactorOne;
            descriptor.colorAttachments[0].destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
            descriptor.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
            pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&shaderError];
            if (!pipeline) { std::fprintf(stderr,"[native] Pipeline ERROR: %s\n",shaderError.localizedDescription.UTF8String); return 1; }
            MTLDepthStencilDescriptor* depthDescriptor=[MTLDepthStencilDescriptor new];
            depthDescriptor.depthCompareFunction=MTLCompareFunctionLess;
            depthDescriptor.depthWriteEnabled=YES;
            depthState=[device newDepthStencilStateWithDescriptor:depthDescriptor];
            depthDescriptor.depthWriteEnabled=NO;
            transparentDepthState=[device newDepthStencilStateWithDescriptor:depthDescriptor];
        }

        NSWindow* window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, 960, 600)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        if (!window) {
            Log("ERROR: Could not create native window");
            return 1;
        }
        window.releasedWhenClosed = NO;
        window.title = model.vertices.empty() ? @"GTA San Andreas Native" :
            [NSString stringWithFormat:@"GTA San Andreas Native — %s",options.model.c_str()];
        NativeWindowDelegate* delegate = [NativeWindowDelegate new];
        window.delegate = delegate;
        NSView* view = window.contentView;
        view.wantsLayer = YES;
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.device = device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
        layer.opaque = YES;
        view.layer = layer;
        [window center];
        window.acceptsMouseMovedEvents = options.play;
        [window makeKeyAndOrderFront:nil];
        [application activateIgnoringOtherApps:YES];

        unsigned presentedFrames = 0;
        int exitCode = 0;
        id<MTLTexture> depthTexture=nil;
        bool cameraDirty=false;
        auto loadedCenter=options.worldCenter;
        std::array<bool,4> movementHeld{}; // west, east, south, north
        bool walkHeld=false;
        bool mouseCaptured=false;
        bool captureRequested=options.play;
#ifdef GTASA_HAS_LIBRW
        std::future<Model> pendingWorld;
        std::optional<std::array<float,4>> loadingCenter;
#endif
        const auto started = std::chrono::steady_clock::now();
        gtasa::native::GameTimer gameTimer(started);
        while (!delegate.shouldClose && (!options.smokeFrames || presentedFrames < options.smokeFrames)) {
            @autoreleasepool {
                NSEvent* event;
                while ((event = [application nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])) {
                    if (options.play && (event.type==NSEventTypeFlagsChanged ||
                        event.type==NSEventTypeKeyDown || event.type==NSEventTypeKeyUp))
                        walkHeld=(event.modifierFlags & NSEventModifierFlagOption)!=0;
                    if (options.play && event.type==NSEventTypeKeyDown &&
                        event.keyCode==53) { // Escape releases the pointer.
                        captureRequested=false;
                        continue;
                    }
                    if (options.play && event.type==NSEventTypeLeftMouseDown &&
                        !captureRequested) captureRequested=true;
                    if (mouseCaptured && event.type==NSEventTypeMouseMoved) {
                        viewControls.yaw+=float(event.deltaX)*0.0035f;
                        viewControls.elevation=std::clamp(
                            viewControls.elevation-float(event.deltaY)*0.0035f,
                            -0.785398f,1.562f);
                        cameraDirty=true;
                        continue;
                    }
                    if (pipeline && event.type==NSEventTypeScrollWheel) {
                        viewControls.zoom=std::clamp(viewControls.zoom*(event.scrollingDeltaY>0 ? 1.10f : 0.90f),0.25f,8.0f);
                        cameraDirty=true;
                        continue;
                    }
                    if (pipeline && options.worldCenter &&
                        (event.type==NSEventTypeKeyDown || event.type==NSEventTypeKeyUp)) {
                        NSString* characters=event.charactersIgnoringModifiers;
                        const unichar key=characters.length ? [characters characterAtIndex:0] : 0;
                        const int direction=options.play ?
                            (key=='a' ? 0 : key=='d' ? 1 : key=='s' ? 2 : key=='w' ? 3 : -1) :
                            (key=='q' ? 0 : key=='e' ? 1 : key=='z' ? 2 : key=='c' ? 3 : -1);
                        if (direction>=0) {
                            const bool down=event.type==NSEventTypeKeyDown;
                            movementHeld[direction]=down;
                            if (down && !event.isARepeat && !options.play) {
                                const int axis=direction<2 ? 0 : 1;
                                (*options.worldCenter)[axis]+=(direction%2 ? 4.0f : -4.0f);
                                cameraDirty=true;
                            }
                            continue;
                        }
                    }
                    if (pipeline && !options.play && event.type==NSEventTypeKeyDown) {
                        NSString* characters=event.charactersIgnoringModifiers;
                        const unichar key=characters.length ? [characters characterAtIndex:0] : 0;
                        bool handled=true;
                        switch (key) {
                        case 'a': case NSLeftArrowFunctionKey: viewControls.yaw-=0.12f; break;
                        case 'd': case NSRightArrowFunctionKey: viewControls.yaw+=0.12f; break;
                        case 'w': case NSUpArrowFunctionKey:
                            viewControls.elevation=std::min(1.35f,viewControls.elevation+0.10f); break;
                        case 's': case NSDownArrowFunctionKey:
                            viewControls.elevation=std::max(0.05f,viewControls.elevation-0.10f); break;
                        case '+': case '=': viewControls.zoom=std::min(8.0f,viewControls.zoom*1.15f); break;
                        case '-': viewControls.zoom=std::max(0.25f,viewControls.zoom/1.15f); break;
                        case 'j': viewControls.panX-=0.08f; break;
                        case 'l': viewControls.panX+=0.08f; break;
                        case 'i': viewControls.panY+=0.08f; break;
                        case 'k': viewControls.panY-=0.08f; break;
                        case 'r':
                            viewControls=ViewControls{};
                            break;
                        default: handled=false; break;
                        }
                        if (handled) { cameraDirty=true; continue; }
                    }
                    [application sendEvent:event];
                }
                [application updateWindows];
                const bool shouldCapture=options.play && captureRequested &&
                    !options.smokeFrames &&
                    window.isKeyWindow && !delegate.shouldClose;
                if (mouseCaptured!=shouldCapture) {
                    CGAssociateMouseAndMouseCursorPosition(!shouldCapture);
                    if (shouldCapture) [NSCursor hide]; else [NSCursor unhide];
                    mouseCaptured=shouldCapture;
                }
                if (delegate.shouldClose) {
                    break;
                }
                const auto frameTime=std::chrono::steady_clock::now();
                const auto timing=gameTimer.Tick(frameTime);
                const float elapsed=static_cast<float>(std::min(timing.realSeconds,0.05));
                const auto movementStart=std::chrono::steady_clock::now();
                if (!window.isKeyWindow) {
                    movementHeld.fill(false);
                    walkHeld=false;
                }
#ifdef GTASA_HAS_LIBRW
                playerWalking=false;
                if (player && walkClip && runClip) {
                    const float target=walkHeld ? 0.0f : 1.0f;
                    runBlend=std::clamp(runBlend+
                        std::clamp(target-runBlend,-elapsed/0.18f,elapsed/0.18f),0.0f,1.0f);
                }
#endif
                if (options.worldCenter) {
                    const float dx=options.autoWalk ? 1.0f :
                        float(movementHeld[1])-float(movementHeld[0]);
                    const float dy=float(movementHeld[3])-float(movementHeld[2]);
                    if (dx!=0.0f || dy!=0.0f) {
#ifdef GTASA_HAS_LIBRW
                        if (options.play && player && world && collision) {
                            const float cy=std::cos(viewControls.yaw),sy=std::sin(viewControls.yaw);
                            const float worldX=cy*dx-sy*dy,worldY=sy*dx+cy*dy;
                            const auto ground=[&](float x,float y,float maxZ) -> std::optional<float> {
                                const auto hit=gtasa::collision::GroundAt(*world,*collision,{x,y,maxZ});
                                return hit ? std::optional<float>(hit->height) : std::nullopt;
                            };
                            const auto blocked=[&](std::array<float,3> from,
                                                   std::array<float,3> to) {
                                return gtasa::collision::BlockedByWorldGeometry(
                                    *world,*collision,from,to);
                            };
                            const auto walkTravel=walkClip->RootTravelDistance(
                                walkSeconds,walkSeconds+elapsed);
                            const auto runTravel=runClip->RootTravelDistance(
                                runSeconds,runSeconds+elapsed);
                            if (!walkTravel || !runTravel)
                                throw std::runtime_error("Player locomotion clip has no root travel");
                            const float travel=(1.0f-runBlend)*(*walkTravel)+
                                               runBlend*(*runTravel);
                            if (player->Step(elapsed,worldX,worldY,ground,blocked,travel)) {
                                playerWalking=true;
                                walkSeconds+=elapsed;
                                runSeconds+=elapsed;
                                const auto& position=player->Position();
                                (*options.worldCenter)[0]=position[0];
                                (*options.worldCenter)[1]=position[1];
                                (*options.worldCenter)[2]=position[2]+0.8f;
                                cameraDirty=true;
                            }
                        } else
#endif
                        if (!options.play) {
                            const float rate=90.0f*elapsed;
                            const float norm=std::hypot(dx,dy);
                            (*options.worldCenter)[0]+=rate*dx/norm;
                            (*options.worldCenter)[1]+=rate*dy/norm;
                            cameraDirty=true;
                        }
                    }
                    desiredFocusX.store((*options.worldCenter)[0],std::memory_order_relaxed);
                    desiredFocusY.store((*options.worldCenter)[1],std::memory_order_relaxed);
                }
#ifdef GTASA_HAS_LIBRW
                if (player && idleClip && walkClip) {
                    idleSeconds+=elapsed;
                    walkBlend=std::clamp(walkBlend+
                        (playerWalking ? 1.0f : -1.0f)*elapsed/0.18f,0.0f,1.0f);
                }
#endif
                const auto movementEnd=std::chrono::steady_clock::now();
                updatePlayerGeometry();
                const auto skinEnd=std::chrono::steady_clock::now();
#ifdef GTASA_HAS_LIBRW
                const auto centerMoved=[](const std::array<float,4>& a,
                                           const std::array<float,4>& b) {
                    const double dx=double(a[0])-b[0],dy=double(a[1])-b[1];
                    return dx*dx+dy*dy>=25.0*25.0;
                };
                if (pendingWorld.valid() &&
                    pendingWorld.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
                    const auto completedCenter=*loadingCenter;
                    try {
                        Model next=pendingWorld.get();
                        if (options.worldCenter && centerMoved(*options.worldCenter,completedCenter)) {
                            std::fprintf(stderr,"[native] Discarded stale region (%.0f, %.0f); focus is (%.0f, %.0f)\n",
                                completedCenter[0],completedCenter[1],
                                (*options.worldCenter)[0],(*options.worldCenter)[1]);
                        } else {
                            model=std::move(next);
                            appendPlayer(model);
                            if (!uploadScene()) { exitCode=1; break; }
                            updatePlayerGeometry();
                            loadedCenter=completedCenter;
                            if (collision && world && options.worldCenter) {
                                const auto& focus=*options.worldCenter;
                                if (const auto ground=gtasa::collision::GroundAt(*world,*collision,
                                        {focus[0],focus[1],focus[2]+2.0f}))
                                    std::fprintf(stderr,"[native] Ground at focus: z=%.3f model=%s\n",
                                        ground->height,ground->modelName.c_str());
                            }
                        }
                    } catch (const StaleRegion&) {
                        std::fprintf(stderr,"[native] Cancelled stale region (%.0f, %.0f)\n",
                            completedCenter[0],completedCenter[1]);
                    } catch (const std::exception& error) {
                        std::fprintf(stderr,"[native] World reload failed: %s\n",error.what());
                        if (options.worldCenter && *options.worldCenter==completedCenter)
                            options.worldCenter=loadedCenter;
                    }
                    loadingCenter.reset();
                }
                const auto needsWorld=[&] {
                    if (!options.worldCenter || !loadedCenter) return false;
                    return centerMoved(*options.worldCenter,*loadedCenter);
                };
                if (!pendingWorld.valid() && world && archive && needsWorld()) {
                    const auto requested=*options.worldCenter;
                    pendingWorld=std::async(std::launch::async,
                        [&world,&archive,&worldModelCache,&worldCacheBytes,
                         &desiredFocusX,&desiredFocusY,requested] {
                            return BuildWorldModel(*world,*archive,requested,
                                                   worldModelCache,worldCacheBytes,
                                                   desiredFocusX,desiredFocusY);
                        });
                    loadingCenter=requested;
                }
#endif
                if (cameraDirty) {
                    if (options.worldCenter)
                        window.title=[NSString stringWithFormat:@"GTA San Andreas Native — World (%.0f, %.0f)",
                            (*options.worldCenter)[0],(*options.worldCenter)[1]];
                    cameraDirty=false;
                }
                const NSRect bounds = view.bounds;
                const CGFloat scale = window.backingScaleFactor;
                layer.contentsScale = scale;
                layer.drawableSize = CGSizeMake(std::max<CGFloat>(1, bounds.size.width * scale),
                                                std::max<CGFloat>(1, bounds.size.height * scale));
                id<CAMetalDrawable> drawable = [layer nextDrawable];
                if (!drawable) {
                    Log("ERROR: Metal drawable unavailable");
                    exitCode = 1;
                    break;
                }
                MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = drawable.texture;
                pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                pass.colorAttachments[0].clearColor = MTLClearColorMake(0.025, 0.045, 0.075, 1.0);
                if (pipeline) {
                    if (!depthTexture || depthTexture.width!=drawable.texture.width ||
                        depthTexture.height!=drawable.texture.height) {
                        MTLTextureDescriptor* depthDescriptor=[MTLTextureDescriptor
                            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                            width:drawable.texture.width height:drawable.texture.height mipmapped:NO];
                        depthDescriptor.usage=MTLTextureUsageRenderTarget;
                        depthTexture=[device newTextureWithDescriptor:depthDescriptor];
                        if (!depthTexture) {
                            Log("ERROR: Could not create Metal depth texture");
                            exitCode=1;
                            break;
                        }
                    }
                    pass.depthAttachment.texture=depthTexture;
                    pass.depthAttachment.loadAction=MTLLoadActionClear;
                    pass.depthAttachment.storeAction=MTLStoreActionDontCare;
                    pass.depthAttachment.clearDepth=1.0;
                }
                id<MTLCommandBuffer> command = [queue commandBuffer];
                id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
                if (!command || !encoder) {
                    Log("ERROR: Could not encode Metal frame");
                    exitCode = 1;
                    break;
                }
                if (pipeline) {
                    std::size_t drawnGroups=0,drawnTriangles=0;
                    [encoder setRenderPipelineState:pipeline];
                    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
                    const float aspect=float(drawable.texture.width)/float(drawable.texture.height);
                    const auto projection=ProjectionParameters(modelBounds,viewControls,options.worldCenter,aspect,options.play);
                    [encoder setVertexBytes:projection.data() length:sizeof(projection) atIndex:1];
                    [encoder setCullMode:MTLCullModeNone];
                    [encoder setFragmentSamplerState:sampler atIndex:0];
                    const auto drawGroup=[&](const DrawGroup& group) {
                        ++drawnGroups;
                        drawnTriangles+=group.count/3;
                        [encoder setDepthStencilState:group.transparent ? transparentDepthState : depthState];
                        const int textureSlot=group.textureIndex;
                        const int useTexture=textureSlot && metalTextures[textureSlot-1] ? 1 : 0;
                        [encoder setFragmentBytes:&useTexture length:sizeof(useTexture) atIndex:0];
                        if (useTexture) [encoder setFragmentTexture:metalTextures[textureSlot-1] atIndex:0];
                        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:group.first vertexCount:group.count];
                    };
                    for (std::size_t i=0;i<model.groups.size();++i) {
                        const auto& group=model.groups[i];
                        if (group.count && !group.transparent && VisibleBounds(groupBounds[i],projection))
                            drawGroup(group);
                    }
                    const auto distance=[&](std::size_t index) {
                        const auto& point=groupCenters[index];
                        const float d=projection[1]*projection[2]*point[0]-
                                      projection[0]*projection[2]*point[1]+projection[3]*point[2];
                        return projection[12]-d;
                    };
                    std::stable_sort(transparentOrder.begin(),transparentOrder.end(),
                        [&](std::size_t a,std::size_t b) { return distance(a)>distance(b); });
                    for (const auto index : transparentOrder) {
                        if (VisibleBounds(groupBounds[index],projection))
                            drawGroup(model.groups[index]);
                    }
                    if (options.smokeFrames && presentedFrames==0)
                        std::fprintf(stderr,"[native] First frame: %zu/%zu nonempty groups visible, %zu triangles submitted\n",
                            drawnGroups,std::count_if(model.groups.begin(),model.groups.end(),
                                [](const auto& group) { return group.count!=0; }),drawnTriangles);
                }
                [encoder endEncoding];
                [command presentDrawable:drawable];
                [command commit];
                // Deliberately bound in-flight work during initial bootstrap verification.
                [command waitUntilCompleted];
                if (command.status == MTLCommandBufferStatusError) {
                    std::fprintf(stderr, "[native] GPU ERROR: %s\n", command.error.localizedDescription.UTF8String);
                    exitCode = 1;
                    break;
                }
                ++presentedFrames;
                if (options.autoWalk && presentedFrames%30==0) {
                    const auto milliseconds=[](auto first,auto last) {
                        return std::chrono::duration<double,std::milli>(last-first).count();
                    };
                    std::fprintf(stderr,
                        "[native] Frame profile: real=%.1fms movement=%.1fms skin=%.1fms rest=%.1fms\n",
                        timing.realSeconds*1000.0,
                        milliseconds(movementStart,movementEnd),
                        milliseconds(movementEnd,skinEnd),
                        milliseconds(skinEnd,std::chrono::steady_clock::now()));
                }
                if (options.smokeFrames && std::chrono::steady_clock::now() - started > std::chrono::seconds(20)) {
                    Log("ERROR: Smoke test exceeded time budget");
                    exitCode = 1;
                    break;
                }
            }
        }
        if (options.smokeFrames && presentedFrames != options.smokeFrames) {
            exitCode = 1;
        }
        if (mouseCaptured) {
            CGAssociateMouseAndMouseCursorPosition(true);
            [NSCursor unhide];
        }
        [window orderOut:nil];
        window.delegate = nil;
        [window close];
        view.layer = nil;
        std::fprintf(stderr, "[native] Shutdown complete; frames=%u, exit=%d\n", presentedFrames, exitCode);
        return exitCode;
    }
}
