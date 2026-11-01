#include "ImgDirectory.h"
#include "RwChunks.h"

#include <cstdio>
#include <exception>
#include <string_view>

int main(int argc, const char* argv[]) {
    if (argc < 2 || argc > 4 || (argc == 4 && std::string_view(argv[3]) != "--inspect-rw")) {
        std::fprintf(stderr, "Usage: gta_sa_assets ARCHIVE.img [ENTRY_NAME [--inspect-rw]]\n");
        return 2;
    }
    try {
        const auto directory = gtasa::assets::ImgDirectory::Open(argv[1]);
        std::printf("IMG VER2: %zu validated directory entries\n", directory.Entries().size());
        if (argc >= 3) {
            const auto* entry = directory.Find(argv[2]);
            if (!entry) {
                std::fprintf(stderr, "Entry not found\n");
                return 1;
            }
            std::printf("%s: offset=%llu bytes, size=%llu bytes, archiveSectors=%u\n",
                entry->name.c_str(), static_cast<unsigned long long>(entry->ByteOffset()),
                static_cast<unsigned long long>(entry->ByteSize()), entry->archiveSectorCount);
            if (argc == 4) {
                const auto bytes = directory.Read(argv[2]);
                const auto root = gtasa::assets::ReadRwChunk(bytes, 0, bytes.size());
                std::printf("RW root: type=0x%x, payload=%u bytes, libraryId=0x%x\n",
                    root.type, root.length, root.libraryId);
                if (root.type != 0x10 && root.type != 0x16) {
                    std::fprintf(stderr, "Expected DFF clump (0x10) or TXD dictionary (0x16)\n");
                    return 1;
                }
                const auto children = gtasa::assets::ReadRwChildren(bytes, root);
                std::printf("RW direct children: %zu\n", children.size());
                if (children.empty() || children.front().type != 1) {
                    std::fprintf(stderr, "Missing RenderWare STRUCT child\n");
                    return 1;
                }
                for (const auto& child : children) {
                    std::printf("  type=0x%x, payload=%u bytes\n", child.type, child.length);
                }
                const auto& structure = children.front();
                if (root.type == 0x10 && structure.length >= 4) {
                    const auto* data = bytes.data() + structure.dataOffset;
                    const unsigned atomics = unsigned(data[0]) | (unsigned(data[1]) << 8) |
                                             (unsigned(data[2]) << 16) | (unsigned(data[3]) << 24);
                    std::printf("DFF clump atomics: %u\n", atomics);
                } else if (root.type == 0x16 && structure.length >= 4) {
                    const auto* data = bytes.data() + structure.dataOffset;
                    const unsigned textures = unsigned(data[0]) | (unsigned(data[1]) << 8);
                    const unsigned platform = unsigned(data[2]) | (unsigned(data[3]) << 8);
                    std::printf("TXD textures: %u, device ID: %u\n", textures, platform);
                }
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Asset error: %s\n", error.what());
        return 1;
    }
}
