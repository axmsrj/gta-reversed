#include "Placement.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <bit>
#include <algorithm>

static void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int i=0;i<4;++i) bytes[offset+i]=static_cast<std::uint8_t>(value >> (i*8));
}
static void PutFloat(std::vector<std::uint8_t>& bytes, std::size_t offset, float value) {
    Put32(bytes,offset,std::bit_cast<std::uint32_t>(value));
}

int main(int argc, const char* argv[]) {
    assert(argc == 2);
    const std::filesystem::path root=argv[1];
    std::filesystem::create_directories(root / "Data/Maps/LA");
    const auto ide=root / "Data/Maps/LA/Block.IDE";
    const auto ipl=root / "Data/Maps/LA/Block.IPL";
    { std::ofstream file(ide); file << "# object types\nobjs\n17500, tower, towers, 150, 128\nend\n"
                                     "tobj\n17501, lamp, lights, 900, 140, 21, 6\nend\n"; }
    { std::ofstream file(ipl); file << "inst\n17500, tower, 0, 1.5, -2, 3, 0, 0, 0, 1, -1\nend\n"; }
    assert(gtasa::world::ResolveGamePath(root,"DATA\\MAPS\\la\\block.ide") == ide);
    const auto region=gtasa::world::LoadRegion(ide,ipl);
    assert(region.types.size()==2 && region.instances.size()==1);
    assert(region.types.at(17500).model=="tower");
    assert(region.types.at(17500).drawDistance==150.0f);
    assert(region.types.at(17500).flags==128u);
    assert(region.types.at(17501).drawDistance==900.0f);
    assert(region.instances[0].position[0]==1.5f);
    assert(region.instances[0].position[1]==-2.0f);
    assert(region.instances[0].rotation[3]==1.0f);
    { std::ofstream file(root / "Data/GTA.DAT"); file << "IDE DATA\\MAPS\\LA\\BLOCK.IDE\nIPL DATA\\MAPS\\LA\\BLOCK.IPL\n"; }
    std::filesystem::create_directories(root / "Models");
    std::vector<std::uint8_t> archive(4096);
    std::copy_n("VER2",4,archive.begin()); Put32(archive,4,1); Put32(archive,8,1);
    archive[12]=1; std::copy_n("block_stream0.ipl",17,archive.begin()+16);
    const std::size_t base=2048;
    std::copy_n("bnry",4,archive.begin()+base);
    Put32(archive,base+4,1); Put32(archive,base+28,76);
    PutFloat(archive,base+76,2.0f); PutFloat(archive,base+80,-2.0f);
    PutFloat(archive,base+84,3.0f); PutFloat(archive,base+100,1.0f);
    Put32(archive,base+104,17500); Put32(archive,base+108,0); Put32(archive,base+112,0);
    { std::ofstream file(root / "Models/gta3.img",std::ios::binary);
      file.write(reinterpret_cast<const char*>(archive.data()),archive.size()); }
    const auto catalog=gtasa::world::LoadWorldCatalog(root);
    assert(catalog.types.size()==2 && catalog.instances.size()==2);
    assert(catalog.instances[1].sourceIpl=="block_stream0");
    assert(catalog.instances[1].lodTarget==0);
    assert(catalog.instances[0].lodChildren==1);
    assert(gtasa::world::InstancesNear(catalog,{1.5f,-2.0f,3.0f},1.0f).size()==2);
    assert(gtasa::world::InstancesNear(catalog,{1.5f,-2.0f,3.0f},1.0f,1).empty());
    auto indexed=catalog;
    indexed.instances[0].position={-32.1f,-0.1f,3.0f};
    indexed.instances[1].position={32.1f,0.1f,3.0f};
    const auto before=gtasa::world::InstancesNear(indexed,{0,0,3},40);
    gtasa::world::BuildSpatialIndex(indexed);
    const auto after=gtasa::world::InstancesNear(indexed,{0,0,3},40);
    assert(after.size()==before.size());
    for (std::size_t i=0;i<after.size();++i) assert(after[i]==&indexed.instances[i]);
    assert(gtasa::world::InstancesNear(indexed,{-32.1f,-0.1f,3},0.1f).size()==1);
    assert(gtasa::world::InstancesNear(indexed,{0,0,3},30).empty());
    gtasa::world::AppendBinaryIpl(indexed,
        std::span<const std::uint8_t>(archive).subspan(base,2048));
    assert(indexed.spatialCells.empty());
    assert(gtasa::world::InstancesNear(indexed,{2,-2,3},0.1f).size()==1);
    bool rejected=false;
    auto corrupt=std::span<const std::uint8_t>(archive).subspan(base,2048);
    std::vector<std::uint8_t> bad(corrupt.begin(),corrupt.end());
    Put32(bad,28,2040);
    try { gtasa::world::Region empty; gtasa::world::AppendBinaryIpl(empty,bad); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    try { (void)gtasa::world::ResolveGamePath(root,"..\\secret"); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    { std::ofstream file(ipl); file << "inst\n17500, tower, 0, NaN, -2, 3, 0, 0, 0, 1, -1\nend\n"; }
    rejected=false;
    try { (void)gtasa::world::LoadRegion(ide,ipl); }
    catch (const std::runtime_error&) { rejected=true; }
    assert(rejected);
    std::filesystem::remove_all(root);
    return 0;
}
