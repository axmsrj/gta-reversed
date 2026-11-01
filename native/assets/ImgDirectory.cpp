#include "ImgDirectory.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace gtasa::assets {
namespace {
std::uint16_t Read16(const unsigned char* bytes) {
    return std::uint16_t(bytes[0]) | (std::uint16_t(bytes[1]) << 8);
}
std::uint32_t Read32(const unsigned char* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
unsigned char FoldAscii(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}
bool EqualName(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
        [](unsigned char a, unsigned char b) { return FoldAscii(a) == FoldAscii(b); });
}
}

ImgDirectory ImgDirectory::Open(const std::filesystem::path& archive) {
    std::ifstream stream(archive, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Cannot open IMG archive");
    }
    const auto end = stream.tellg();
    if (end < 8) {
        throw std::runtime_error("Truncated IMG header");
    }
    const auto fileSize = static_cast<std::uint64_t>(end);
    stream.seekg(0);
    std::array<unsigned char, 8> header{};
    if (!stream.read(reinterpret_cast<char*>(header.data()), header.size()) ||
        !std::equal(header.begin(), header.begin() + 4, "VER2")) {
        throw std::runtime_error("Expected IMG VER2 header");
    }
    const auto count = Read32(header.data() + 4);
    const std::uint64_t directoryEnd = 8 + std::uint64_t{count} * 32;
    // Bound allocation independently of file size (including sparse hostile files).
    if (directoryEnd > fileSize || count > 1'000'000) {
        throw std::runtime_error("IMG directory count exceeds file or resource limit");
    }
    ImgDirectory directory;
    directory.m_Archive = archive;
    directory.m_Entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<unsigned char, 32> record{};
        if (!stream.read(reinterpret_cast<char*>(record.data()), record.size())) {
            throw std::runtime_error("Truncated IMG directory entry");
        }
        const auto nameStart = record.begin() + 8;
        const auto nameEnd = std::find(nameStart, record.end(), 0);
        if (nameEnd == nameStart || nameEnd == record.end()) {
            throw std::runtime_error("IMG entry name must be nonempty and null terminated");
        }
        ImgEntry entry{Read32(record.data()), Read16(record.data() + 4),
                       Read16(record.data() + 6), std::string(nameStart, nameEnd)};
        // The inspected original lookup returns the first uint16 sector count.
        // Retain the second field separately; do not invent decompression behavior.
        if (entry.ByteOffset() < directoryEnd || entry.ByteOffset() > fileSize ||
            entry.ByteSize() > fileSize - entry.ByteOffset()) {
            throw std::runtime_error("IMG entry payload lies outside archive bounds");
        }
        directory.m_Entries.push_back(std::move(entry));
    }
    return directory;
}

const ImgEntry* ImgDirectory::Find(std::string_view name) const {
    // Preserve original first-match search; game asset names use ASCII.
    const auto found = std::find_if(m_Entries.begin(), m_Entries.end(),
        [name](const ImgEntry& entry) { return EqualName(entry.name, name); });
    return found == m_Entries.end() ? nullptr : &*found;
}

std::vector<std::uint8_t> ImgDirectory::Read(std::string_view name,
                                             std::size_t maxBytes) const {
    const auto* entry = Find(name);
    if (!entry) {
        throw std::runtime_error("IMG entry not found");
    }
    if (entry->ByteSize() > maxBytes) {
        throw std::runtime_error("IMG entry exceeds read limit");
    }
    std::ifstream stream(m_Archive, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Cannot reopen IMG archive");
    }
    const auto fileSize = static_cast<std::uint64_t>(stream.tellg());
    if (entry->ByteOffset() > fileSize ||
        entry->ByteSize() > fileSize - entry->ByteOffset()) {
        throw std::runtime_error("IMG entry changed or truncated since directory scan");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry->ByteSize()));
    stream.seekg(static_cast<std::streamoff>(entry->ByteOffset()));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Could not read complete IMG entry");
    }
    return bytes;
}
}
