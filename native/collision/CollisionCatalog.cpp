#include "CollisionCatalog.h"
#include "ImgDirectory.h"

#include <algorithm>
#include <cctype>

namespace gtasa::collision {
namespace {
std::string Lower(std::string_view value) {
    std::string result(value);
    for (char& ch : result)
        ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return result;
}
}

CollisionCatalog CollisionCatalog::Load(const assets::ImgDirectory& archive) {
    CollisionCatalog catalog;
    for (const auto& entry : archive.Entries()) {
        if (!std::string_view(entry.name).ends_with(".col")) continue;
        for (auto& mesh : ParseColArchive(archive.Read(entry.name))) {
            auto name=Lower(mesh.name);
            if (const auto previous=catalog.meshes_.find(name);
                previous!=catalog.meshes_.end())
                catalog.triangleCount_-=previous->second.triangles.size();
            catalog.triangleCount_+=mesh.triangles.size();
            catalog.meshes_.insert_or_assign(std::move(name),std::move(mesh));
        }
    }
    return catalog;
}

const Mesh* CollisionCatalog::Find(std::string_view modelName) const {
    const auto found=meshes_.find(Lower(modelName));
    return found==meshes_.end() ? nullptr : &found->second;
}

} // namespace gtasa::collision
