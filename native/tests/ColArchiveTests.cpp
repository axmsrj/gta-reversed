#include "ColArchive.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <bit>
#include <stdexcept>
#include <vector>

static void Put16(std::vector<std::uint8_t>& b,std::size_t at,std::uint16_t value) {
    b[at]=value&255; b[at+1]=value>>8;
}
static void Put32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t value) {
    for (int i=0;i<4;++i) b[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
static void PutFloat(std::vector<std::uint8_t>& b,std::size_t at,float value) {
    Put32(b,at,std::bit_cast<std::uint32_t>(value));
}

int main() {
    std::vector<std::uint8_t> bytes(134);
    std::copy_n("COL2",4,bytes.begin());
    Put32(bytes,4,126);
    std::copy_n("ground",6,bytes.begin()+8);
    Put16(bytes,30,17500);
    Put16(bytes,76,1);
    Put32(bytes,80,2);
    Put32(bytes,96,104); // Vertex array begins at byte 108 (offsets start at byte 4).
    Put32(bytes,100,122); // Triangle begins at byte 126.
    Put16(bytes,114,128); // Second vertex: x = 1.
    Put16(bytes,122,128); // Third vertex: y = 1.
    Put16(bytes,124,128); // Third vertex: z = 1.
    Put16(bytes,126,0); Put16(bytes,128,1); Put16(bytes,130,2);
    bytes[132]=7; bytes[133]=9;
    const auto meshes=gtasa::collision::ParseColArchive(bytes);
    assert(meshes.size()==1 && meshes[0].name=="ground");
    assert(meshes[0].fileModelId==17500);
    assert(meshes[0].vertices.size()==3 && meshes[0].triangles.size()==1);
    assert(meshes[0].vertices[1][0]==1.0f);
    assert(meshes[0].vertices[2][1]==1.0f && meshes[0].vertices[2][2]==1.0f);
    assert(meshes[0].triangles[0].material==7);

    auto bad=bytes;
    Put32(bad,100,1000);
    bool rejected=false;
    try { (void)gtasa::collision::ParseColArchive(bad); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    bad=bytes;
    bad.resize(2048);
    bad.back()=1;
    rejected=false;
    try { (void)gtasa::collision::ParseColArchive(bad); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    bad=bytes;
    Put16(bad,130,100);
    rejected=false;
    try { (void)gtasa::collision::ParseColArchive(bad); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    bad=bytes;
    bad.pop_back();
    rejected=false;
    try { (void)gtasa::collision::ParseColArchive(bad); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);

    std::vector<std::uint8_t> volumes(156);
    std::copy_n("COL2",4,volumes.begin());
    Put32(volumes,4,148);
    std::copy_n("volumes",8,volumes.begin()+8);
    Put16(volumes,72,1);
    Put16(volumes,74,1);
    Put32(volumes,84,104); // Sphere starts at byte 108.
    Put32(volumes,88,124); // Box starts at byte 128.
    PutFloat(volumes,108,1.0f); PutFloat(volumes,112,2.0f);
    PutFloat(volumes,116,3.0f); PutFloat(volumes,120,0.5f);
    volumes[124]=9;
    PutFloat(volumes,128,-1.0f); PutFloat(volumes,132,-2.0f);
    PutFloat(volumes,136,0.0f); PutFloat(volumes,140,1.0f);
    PutFloat(volumes,144,2.0f); PutFloat(volumes,148,4.0f);
    volumes[152]=7;
    const auto parsedVolumes=gtasa::collision::ParseColArchive(volumes);
    assert(parsedVolumes.size()==1);
    assert(parsedVolumes[0].spheres.size()==1 && parsedVolumes[0].boxes.size()==1);
    assert((parsedVolumes[0].spheres[0].center==std::array<float,3>{1,2,3}));
    assert(parsedVolumes[0].spheres[0].radius==0.5f);
    assert(parsedVolumes[0].spheres[0].material==9);
    assert((parsedVolumes[0].boxes[0].minimum==std::array<float,3>{-1,-2,0}));
    assert((parsedVolumes[0].boxes[0].maximum==std::array<float,3>{1,2,4}));
    assert(parsedVolumes[0].boxes[0].material==7);
    bad=volumes; Put32(bad,88,1000); assert([&] {
        try { (void)gtasa::collision::ParseColArchive(bad); }
        catch (const std::runtime_error&) { return true; } return false;
    }());
    bad=volumes; PutFloat(bad,120,-1.0f); assert([&] {
        try { (void)gtasa::collision::ParseColArchive(bad); }
        catch (const std::runtime_error&) { return true; } return false;
    }());
    return 0;
}
