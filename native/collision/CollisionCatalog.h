#pragma once

#include "ColArchive.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gtasa::assets { class ImgDirectory; }

namespace gtasa::collision {

class CollisionCatalog {
public:
    static CollisionCatalog Load(const assets::ImgDirectory& archive);
    const Mesh* Find(std::string_view modelName) const;
    std::size_t Size() const { return meshes_.size(); }
    std::size_t TriangleCount() const { return triangleCount_; }

private:
    std::unordered_map<std::string,Mesh> meshes_;
    std::size_t triangleCount_{};
};

} // namespace gtasa::collision
