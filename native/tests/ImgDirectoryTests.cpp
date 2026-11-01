#include "ImgDirectory.h"
#include "RwChunks.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
void Write32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
    }
}
void Save(const std::filesystem::path& file, const std::vector<unsigned char>& bytes) {
    std::ofstream stream(file, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    Require(bool(stream), "Fixture write failed");
}
void ExpectRejected(const std::filesystem::path& file, const std::vector<unsigned char>& bytes) {
    Save(file, bytes);
    bool rejected = false;
    try {
        gtasa::assets::ImgDirectory::Open(file);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "Malformed archive accepted");
}
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        return 2;
    }
    try {
        const std::filesystem::path root{argv[1]};
        std::filesystem::create_directories(root);
        const auto file = root / "fixture.img";
        std::vector<unsigned char> valid(4096);
        std::copy_n("VER2", 4, valid.begin());
        Write32(valid, 4, 1);
        Write32(valid, 8, 1); // sector offset
        valid[12] = 1; // sector count, uint16 LE
        std::copy_n("TeSt.dff", 8, valid.begin() + 16);
        Save(file, valid);
        const auto directory = gtasa::assets::ImgDirectory::Open(file);
        const auto* entry = directory.Find("test.DFF");
        Require(entry && entry->ByteOffset() == 2048 && entry->ByteSize() == 2048,
                "Original sector decoding/case-insensitive lookup failed");
        Require(directory.Find("missing.dff") == nullptr, "Missing lookup must return null");
        const auto payload = directory.Read("TEST.dff");
        Require(payload.size() == 2048, "Read must return exactly the entry's padded sectors");
        bool rejectedReadLimit = false;
        try {
            directory.Read("test.dff", 2047);
        } catch (const std::runtime_error&) {
            rejectedReadLimit = true;
        }
        Require(rejectedReadLimit, "Entry read limit ignored");
        auto rw = payload;
        Write32(rw, 0, 0x10);
        Write32(rw, 4, 12);
        Write32(rw, 8, 0x1803ffff);
        Write32(rw, 12, 1);
        Write32(rw, 16, 0);
        const auto rootChunk = gtasa::assets::ReadRwChunk(rw, 0, rw.size());
        const auto children = gtasa::assets::ReadRwChildren(rw, rootChunk);
        Require(children.size() == 1 && children[0].type == 1,
                "Nested RenderWare chunk bounds decoded incorrectly");
        Write32(rw, 4, 0xffffffff);
        bool rejectedChunk = false;
        try {
            gtasa::assets::ReadRwChunk(rw, 0, rw.size());
        } catch (const std::runtime_error&) {
            rejectedChunk = true;
        }
        Require(rejectedChunk, "Oversized RenderWare chunk accepted");
        Save(file, std::vector<unsigned char>(2048));
        bool rejectedTruncation = false;
        try {
            directory.Read("test.dff");
        } catch (const std::runtime_error&) {
            rejectedTruncation = true;
        }
        Require(rejectedTruncation, "Archive truncated after index scan was accepted");
        auto bad = valid;
        bad[0] = 'X';
        ExpectRejected(file, bad);
        bad = valid;
        Write32(bad, 4, 0xffffffff);
        ExpectRejected(file, bad);
        bad = valid;
        Write32(bad, 8, 0xffffffff);
        ExpectRejected(file, bad);
        bad = valid;
        Write32(bad, 8, 0);
        ExpectRejected(file, bad);
        bad = valid;
        std::fill(bad.begin() + 16, bad.begin() + 40, 'x');
        ExpectRejected(file, bad);
        bad = valid;
        bad.resize(4095);
        ExpectRejected(file, bad);
        ExpectRejected(file, std::vector<unsigned char>{'V', 'E', 'R'});
        std::filesystem::remove(file);
        std::puts("IMG tests passed: case folding, sectors, missing names and malformed bounds");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
