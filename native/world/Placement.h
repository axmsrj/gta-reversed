#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <span>

namespace gtasa::world {

struct ObjectType {
    std::int32_t id{};
    std::string model;
    std::string textureDictionary;
    float drawDistance{};
    std::uint32_t flags{};
};

struct Instance {
    std::int32_t modelId{};
    std::string modelName;
    std::int32_t interior{};
    std::array<float, 3> position{};
    std::array<float, 4> rotation{}; // x, y, z, w
    std::int32_t lodIndex{};
    std::string sourceIpl;
    std::int32_t lodTarget{-1}; // Global instance index, resolved from source IPL.
    std::uint32_t lodChildren{};
};

struct Region {
    struct Cell {
        std::int64_t x{}, y{};
        bool operator==(const Cell&) const = default;
    };
    struct CellHash {
        std::size_t operator()(Cell cell) const noexcept;
    };
    std::unordered_map<std::int32_t, ObjectType> types;
    std::vector<Instance> instances;
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> spatialCells;
};

// Reads the original text IDE/IPL sections; no game executable or global engine state.
Region LoadRegion(const std::filesystem::path& ide, const std::filesystem::path& ipl);
Region LoadWorldCatalog(const std::filesystem::path& gameRoot);
void AppendBinaryIpl(Region& world, std::span<const std::uint8_t> bytes,
                     std::string_view sourceIpl = {});
void BuildSpatialIndex(Region& world);
std::vector<const Instance*> InstancesNear(const Region& world,
                                           std::array<float, 3> center, float radius,
                                           std::int32_t interior = 0);

// Finds a gta.dat path component-by-component on case-sensitive macOS volumes.
std::filesystem::path ResolveGamePath(const std::filesystem::path& root,
                                      const std::string& windowsPath);

} // namespace gtasa::world
