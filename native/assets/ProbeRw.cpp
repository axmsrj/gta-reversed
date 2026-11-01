#include "ImgDirectory.h"
#include "RwChunks.h"

#include "rw.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <stdexcept>

static void PrintFrames(rw::Frame* frame, int depth=0) {
    if (!frame) return;
    const auto* transform=frame->getLTM();
    std::printf("%*sframe local=(%.2f,%.2f,%.2f) world=(%.2f,%.2f,%.2f)\n", depth*2, "",
        frame->matrix.pos.x,frame->matrix.pos.y,frame->matrix.pos.z,
        transform->pos.x,transform->pos.y,transform->pos.z);
    for (auto* child=frame->child; child; child=child->next) PrintFrames(child,depth+1);
}

int main(int argc, const char* argv[]) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: gta_sa_rw_probe ARCHIVE.img MODEL.dff TEXTURES.txd\n");
        return 2;
    }
    try {
        const auto directory = gtasa::assets::ImgDirectory::Open(argv[1]);
        auto model = directory.Read(argv[2]);
        auto textures = directory.Read(argv[3]);
        if (gtasa::assets::ReadRwChunk(model, 0, model.size()).type != 0x10 ||
            gtasa::assets::ReadRwChunk(textures, 0, textures.size()).type != 0x16) {
            throw std::runtime_error("Expected a DFF clump and TXD dictionary");
        }
        if (!rw::Engine::init()) {
            throw std::runtime_error("librw initialization failed");
        }
        rw::registerHAnimPlugin();
        rw::registerSkinPlugin();
        if (!rw::Engine::open(nullptr)) {
            throw std::runtime_error("librw open failed");
        }
        if (!rw::Engine::start()) {
            throw std::runtime_error("librw start failed");
        }

        rw::StreamMemory textureStream;
        textureStream.open(textures.data(), static_cast<rw::uint32>(textures.size()));
        rw::TexDictionary* dictionary = nullptr;
        if (rw::findChunk(&textureStream, rw::ID_TEXDICTIONARY, nullptr, nullptr)) {
            dictionary = rw::TexDictionary::streamRead(&textureStream);
        }
        textureStream.close();
        if (!dictionary) {
            throw std::runtime_error("librw could not parse TXD");
        }
        rw::TexDictionary::setCurrent(dictionary);
        rw::StreamMemory modelStream;
        modelStream.open(model.data(), static_cast<rw::uint32>(model.size()));
        rw::Clump* clump = nullptr;
        if (rw::findChunk(&modelStream, rw::ID_CLUMP, nullptr, nullptr)) {
            clump = rw::Clump::streamRead(&modelStream);
        }
        modelStream.close();
        if (!clump) {
            throw std::runtime_error("librw could not parse DFF");
        }
        std::printf("DFF parsed by librw: %d atomics\n", clump->countAtomics());
        PrintFrames(clump->getFrame());
        if (auto* hierarchy=rw::HAnimHierarchy::get(clump)) {
            hierarchy->attach();
            std::printf("  hierarchy: nodes=%d flags=0x%x\n",hierarchy->numNodes,hierarchy->flags);
            int attached=0;
            for (int i=0;i<hierarchy->numNodes;++i) attached+=hierarchy->nodeInfo[i].frame!=nullptr;
            std::printf("  hierarchy attached frames: %d\n",attached);
            if (hierarchy->numNodes>0 && hierarchy->nodeInfo[0].frame) {
                const auto& root=hierarchy->nodeInfo[0].frame->matrix;
                std::printf("  root basis right=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f) at=(%.3f,%.3f,%.3f)\n",
                    root.right.x,root.right.y,root.right.z,
                    root.up.x,root.up.y,root.up.z,
                    root.at.x,root.at.y,root.at.z);
            }
            for (int i=0;i<hierarchy->numNodes;++i)
                std::printf("    bone[%d]: id=%d index=%d flags=0x%x\n",i,
                    hierarchy->nodeInfo[i].id,hierarchy->nodeInfo[i].index,
                    hierarchy->nodeInfo[i].flags);
        }
        for (rw::LLLink* link = clump->atomics.link.next;
             link != clump->atomics.end(); link = link->next) {
            const auto* atomic = rw::Atomic::fromClump(link);
            const auto* geometry = atomic->geometry;
            std::printf("  geometry: vertices=%d triangles=%d flags=0x%x materials=%d uvSets=%d\n",
                geometry->numVertices, geometry->numTriangles, geometry->flags,
                geometry->matList.numMaterials, geometry->numTexCoordSets);
            if (const auto* skin=rw::Skin::get(geometry)) {
                std::printf("    skin: bones=%d used=%d weights=%d\n",
                    skin->numBones,skin->numUsedBones,skin->numWeights);
                std::printf("    skin hierarchy attached: %s\n",
                    rw::Skin::getHierarchy(atomic) ? "yes" : "no");
                if (auto* hierarchy=rw::HAnimHierarchy::get(clump);
                    hierarchy && hierarchy->numNodes==skin->numBones) {
                    rw::Matrix inverseAtomic,tmp,result;
                    rw::Matrix::invert(&inverseAtomic,atomic->getFrame()->getLTM());
                    auto* inverseBind=reinterpret_cast<rw::Matrix*>(skin->inverseMatrices);
                    const auto printBasis=[](const char* label,const rw::Matrix& matrix) {
                        std::printf("      %s right=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f) at=(%.3f,%.3f,%.3f)\n",
                            label,matrix.right.x,matrix.right.y,matrix.right.z,
                            matrix.up.x,matrix.up.y,matrix.up.z,
                            matrix.at.x,matrix.at.y,matrix.at.z);
                    };
                    printBasis("atomic",*atomic->getFrame()->getLTM());
                    printBasis("inverse-atomic",inverseAtomic);
                    printBasis("inverse-bind-root",inverseBind[0]);
                    if (hierarchy->parentFrame)
                        printBasis("hierarchy-parent",*hierarchy->parentFrame->getLTM());
                    for (int i=0;i<std::min(skin->numBones,3);++i) {
                        rw::Matrix::mult(&tmp,hierarchy->nodeInfo[i].frame->getLTM(),&inverseAtomic);
                        rw::Matrix::mult(&result,&inverseBind[i],&tmp);
                        std::printf("      bind[%d]: diag=%.3f,%.3f,%.3f pos=%.3f,%.3f,%.3f\n",
                            i,result.right.x,result.up.y,result.at.z,
                            result.pos.x,result.pos.y,result.pos.z);
                    }
                }
                for (int v=0;skin->indices && skin->weights &&
                             v<std::min(geometry->numVertices,3);++v)
                    std::printf("      vertex[%d]: bones=%u,%u,%u,%u weights=%.3f,%.3f,%.3f,%.3f\n",
                        v,skin->indices[v*4],skin->indices[v*4+1],
                        skin->indices[v*4+2],skin->indices[v*4+3],
                        skin->weights[v*4],skin->weights[v*4+1],
                        skin->weights[v*4+2],skin->weights[v*4+3]);
            }
            for (int j = 0; j < geometry->matList.numMaterials; ++j) {
                const auto* material = geometry->matList.materials[j];
                int triangles=0; float cx=0,cy=0,cz=0;
                for (int t=0; t<geometry->numTriangles; ++t) if (geometry->triangles[t].matId == j) {
                    ++triangles;
                    for (int v=0;v<3;++v) {
                        const auto& p=geometry->morphTargets[0].vertices[geometry->triangles[t].v[v]];
                        cx+=p.x; cy+=p.y; cz+=p.z;
                    }
                }
                if (triangles) { cx/=triangles*3; cy/=triangles*3; cz/=triangles*3; }
                std::printf("    material[%d]: tris=%d center=(%.2f,%.2f,%.2f) rgba=(%u,%u,%u,%u) texture=%s\n", j,triangles,cx,cy,cz,
                    material->color.red, material->color.green,
                    material->color.blue, material->color.alpha,
                    material->texture ? material->texture->name : "-");
            }
        }
        clump->destroy();
        std::printf("TXD parsed by librw: %d textures\n", dictionary->count());
        for (rw::LLLink* link = dictionary->textures.link.next;
             link != dictionary->textures.end(); link = link->next) {
            const auto* texture = rw::Texture::fromDict(link);
            auto* raster = texture->raster;
            std::printf("  texture: %s %dx%d platform=%d format=0x%x\n", texture->name,
                raster->width, raster->height, raster->platform, raster->format);
            auto* pixels = raster->lock(0, rw::Raster::LOCKREAD);
            std::printf("    pixels=%p stride=%d depth=%d first=%02x %02x\n", pixels,
                raster->stride, raster->depth, pixels ? pixels[0] : 0, pixels ? pixels[1] : 0);
            if (pixels) raster->unlock(0);
        }
        dictionary->destroy();

        rw::Engine::stop();
        rw::Engine::close();
        rw::Engine::term();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Resource parse failed: %s\n", error.what());
        return 1;
    }
}
