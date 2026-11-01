#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gtasa::collision {

struct Triangle {
    std::array<std::uint16_t,3> indices{};
    std::uint8_t material{};
    std::uint8_t light{};
};

struct Sphere {
    std::array<float,3> center{};
    float radius{};
    std::uint8_t material{};
};

struct Box {
    std::array<float,3> minimum{};
    std::array<float,3> maximum{};
    std::uint8_t material{};
};

struct Mesh {
    std::string name;
    std::uint16_t fileModelId{};
    std::vector<std::array<float,3>> vertices;
    std::vector<Triangle> triangles;
    std::vector<Sphere> spheres;
    std::vector<Box> boxes;
};

// Parses bounded COL2/COL3 records from one IMG entry. Padding after the last
// record is allowed; disk offsets are decoded explicitly, never cast to pointers.
std::vector<Mesh> ParseColArchive(std::span<const std::uint8_t> bytes);

} // namespace gtasa::collision
