#include "ColArchive.h"
#include "ImgDirectory.h"

#include <cstdio>
#include <exception>
#include <string_view>

int main(int argc,const char* argv[]) {
    if (argc!=3 && argc!=4) {
        std::fprintf(stderr,"Usage: gta_sa_collision_probe GTA3_IMG (COL_ENTRY [MODEL_NAME] | --all | --find MODEL_NAME)\n");
        return 2;
    }
    try {
        const auto archive=gtasa::assets::ImgDirectory::Open(argv[1]);
        if (std::string_view(argv[2])=="--find" && argc==4) {
            for (const auto& entry : archive.Entries()) {
                if (!std::string_view(entry.name).ends_with(".col")) continue;
                for (const auto& mesh : gtasa::collision::ParseColArchive(archive.Read(entry.name)))
                    if (mesh.name==std::string_view(argv[3])) {
                        std::printf("archive=%s model=%s triangles=%zu spheres=%zu boxes=%zu\n",
                            entry.name.c_str(),mesh.name.c_str(),mesh.triangles.size(),
                            mesh.spheres.size(),mesh.boxes.size());
                        if (!mesh.boxes.empty()) {
                            const auto& box=mesh.boxes.front();
                            std::printf("first-box min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
                                box.minimum[0],box.minimum[1],box.minimum[2],
                                box.maximum[0],box.maximum[1],box.maximum[2]);
                        }
                        return 0;
                    }
            }
            return 1;
        }
        if (std::string_view(argv[2])=="--all") {
            std::size_t entries=0,models=0,triangles=0,spheres=0,boxes=0,failed=0;
            for (const auto& entry : archive.Entries()) {
                if (!std::string_view(entry.name).ends_with(".col")) continue;
                ++entries;
                try {
                    const auto parsed=gtasa::collision::ParseColArchive(archive.Read(entry.name));
                    models+=parsed.size();
                    for (const auto& mesh : parsed) {
                        triangles+=mesh.triangles.size();
                        spheres+=mesh.spheres.size();
                        boxes+=mesh.boxes.size();
                    }
                } catch (const std::exception& error) {
                    ++failed;
                    std::fprintf(stderr,"%s: %s\n",entry.name.c_str(),error.what());
                }
            }
            std::printf("COL audit entries=%zu models=%zu triangles=%zu spheres=%zu boxes=%zu failed=%zu\n",
                entries,models,triangles,spheres,boxes,failed);
            return failed ? 1 : 0;
        }
        const auto meshes=gtasa::collision::ParseColArchive(archive.Read(argv[2]));
        std::size_t triangles=0;
        bool found=argc==3;
        for (const auto& mesh : meshes) {
            triangles+=mesh.triangles.size();
            if (argc==4 && mesh.name==std::string_view(argv[3])) {
                std::printf("model=%s file-id=%u vertices=%zu triangles=%zu spheres=%zu boxes=%zu\n",
                    mesh.name.c_str(),mesh.fileModelId,mesh.vertices.size(),mesh.triangles.size(),
                    mesh.spheres.size(),mesh.boxes.size());
                found=true;
            }
        }
        std::printf("archive=%s models=%zu triangles=%zu\n",argv[2],meshes.size(),triangles);
        return found ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"Collision parse failed: %s\n",error.what());
        return 1;
    }
}
