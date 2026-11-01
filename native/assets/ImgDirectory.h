#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gtasa::assets {
// Disk fields follow CDirectory::DirectoryInfo. Runtime state owns its storage;
// it deliberately does not preserve the 32-bit CDirectory object layout.
struct ImgEntry {
    std::uint32_t sectorOffset;
    std::uint16_t sectorCount;
    std::uint16_t archiveSectorCount;
    std::string name;

    std::uint64_t ByteOffset() const { return std::uint64_t{sectorOffset} * 2048; }
    std::uint64_t ByteSize() const { return std::uint64_t{sectorCount} * 2048; }
};

class ImgDirectory {
public:
    // Reads only the directory. Does not copy, extract or execute game resources.
    static ImgDirectory Open(const std::filesystem::path& archive);
    const std::vector<ImgEntry>& Entries() const { return m_Entries; }
    const ImgEntry* Find(std::string_view name) const;
    // Reads at most maxBytes from a validated entry without extracting an archive.
    std::vector<std::uint8_t> Read(std::string_view name,
                                   std::size_t maxBytes = 64 * 1024 * 1024) const;
private:
    std::filesystem::path m_Archive;
    std::vector<ImgEntry> m_Entries;
};
}
