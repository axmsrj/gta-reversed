#include "Placement.h"
#include "ImgDirectory.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <bit>
#include <cstring>
#include <limits>

namespace gtasa::world {
namespace {
constexpr double spatialCellSize=32.0;

std::int64_t CellCoordinate(double coordinate) {
    const double cell=std::floor(coordinate/spatialCellSize);
    constexpr double limit=double(std::numeric_limits<std::int64_t>::max()/2);
    return static_cast<std::int64_t>(std::clamp(cell,-limit,limit));
}

std::string_view Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return value;
}

std::string Lower(std::string_view value) {
    std::string result(value);
    for (char& ch : result) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return result;
}

std::vector<std::string_view> Fields(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto comma = line.find(',');
        fields.push_back(Trim(line.substr(0, comma)));
        if (comma == std::string_view::npos) return fields;
        line.remove_prefix(comma + 1);
    }
}

std::uint32_t Read32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size()-offset < 4)
        throw std::runtime_error("Truncated binary IPL field");
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset+1]) << 8) |
           (std::uint32_t(bytes[offset+2]) << 16) | (std::uint32_t(bytes[offset+3]) << 24);
}

float ReadFloat(std::span<const std::uint8_t> bytes, std::size_t offset) {
    const auto value=std::bit_cast<float>(Read32(bytes,offset));
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite binary IPL coordinate");
    return value;
}

template<class T> T Number(std::string_view value) {
    T result{};
    const auto parsed = std::from_chars(value.data(), value.data()+value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size())
        throw std::runtime_error("Invalid IDE/IPL numeric field");
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(result)) throw std::runtime_error("Nonfinite IDE/IPL coordinate");
    }
    return result;
}

template<class Callback> void ReadSections(const std::filesystem::path& file, Callback callback) {
    std::ifstream input(file);
    if (!input) throw std::runtime_error("Cannot open IDE/IPL file: " + file.string());
    std::string section, line;
    std::size_t lineNumber=0;
    while (std::getline(input,line)) {
        ++lineNumber;
        if (line.size() > 4096) throw std::runtime_error("IDE/IPL line exceeds limit");
        const auto text=Trim(line);
        if (text.empty() || text.front()=='#') continue;
        if (text == "end") { section.clear(); continue; }
        if (section.empty()) { section=Lower(text); continue; }
        try { callback(section,text); }
        catch (const std::exception& error) {
            throw std::runtime_error(file.string()+":"+std::to_string(lineNumber)+": "+error.what());
        }
    }
}
}

void AppendTypes(Region& region, const std::filesystem::path& ide) {
    region.spatialCells.clear();
    ReadSections(ide,[&](const std::string& section, std::string_view text) {
        if (section != "objs" && section != "tobj") return;
        const auto values=Fields(text);
        if (values.size() != (section=="tobj" ? 7u : 5u) ||
            values[1].empty() || values[2].empty())
            throw std::runtime_error("Malformed object type");
        const auto id=Number<std::int32_t>(values[0]);
        const auto drawDistance=Number<float>(values[3]);
        if (drawDistance<0.0f) throw std::runtime_error("Negative object draw distance");
        const auto flags=Number<std::uint32_t>(values[4]);
        region.types.insert_or_assign(id,ObjectType{id,std::string(values[1]),
                                                    std::string(values[2]),drawDistance,flags});
    });
}

void AppendInstances(Region& region, const std::filesystem::path& ipl) {
    region.spatialCells.clear();
    const auto source=Lower(ipl.stem().string());
    ReadSections(ipl,[&](const std::string& section, std::string_view text) {
        if (section != "inst") return;
        const auto values=Fields(text);
        if (values.size() != 11) throw std::runtime_error("Instance requires 11 fields");
        Instance instance;
        instance.modelId=Number<std::int32_t>(values[0]);
        instance.modelName=std::string(values[1]);
        instance.interior=Number<std::int32_t>(values[2]);
        for (int i=0;i<3;++i) instance.position[i]=Number<float>(values[3+i]);
        for (int i=0;i<4;++i) instance.rotation[i]=Number<float>(values[6+i]);
        instance.lodIndex=Number<std::int32_t>(values[10]);
        instance.sourceIpl=source;
        if (instance.modelName.empty()) throw std::runtime_error("Instance has empty model name");
        region.instances.push_back(std::move(instance));
    });
}

void ResolveLodLinks(Region& world);

Region LoadRegion(const std::filesystem::path& ide, const std::filesystem::path& ipl) {
    Region region;
    AppendTypes(region,ide);
    AppendInstances(region,ipl);
    ResolveLodLinks(region);
    BuildSpatialIndex(region);
    return region;
}

void ResolveLodLinks(Region& world) {
    std::unordered_map<std::string,std::vector<std::size_t>> sourceIndices;
    for (std::size_t i=0;i<world.instances.size();++i) {
        const auto& source=world.instances[i].sourceIpl;
        if (!source.empty() && source.find("_stream")==std::string::npos)
            sourceIndices[source].push_back(i);
    }
    for (auto& instance : world.instances) {
        if (instance.lodIndex<0 || instance.sourceIpl.empty()) continue;
        auto base=instance.sourceIpl;
        if (const auto stream=base.find("_stream");stream!=std::string::npos)
            base.resize(stream);
        const auto found=sourceIndices.find(base);
        if (found==sourceIndices.end() ||
            static_cast<std::size_t>(instance.lodIndex)>=found->second.size()) continue;
        instance.lodTarget=static_cast<std::int32_t>(found->second[instance.lodIndex]);
    }
    for (const auto& instance : world.instances) {
        if (instance.lodTarget>=0)
            ++world.instances[instance.lodTarget].lodChildren;
    }
}

Region LoadWorldCatalog(const std::filesystem::path& gameRoot) {
    const auto manifest=ResolveGamePath(gameRoot,"DATA\\GTA.DAT");
    std::ifstream input(manifest);
    if (!input) throw std::runtime_error("Cannot read gta.dat");
    std::vector<std::string> ides, ipls;
    std::string line;
    while (std::getline(input,line)) {
        const auto text=Trim(line);
        if (text.empty() || text.front()=='#') continue;
        const auto separator=text.find_first_of(" \t");
        if (separator==std::string_view::npos) continue;
        const auto directive=Lower(text.substr(0,separator));
        const auto path=Trim(text.substr(separator+1));
        if (directive=="ide") ides.emplace_back(path);
        else if (directive=="ipl") ipls.emplace_back(path);
    }
    Region world;
    for (const auto& path : ides) AppendTypes(world,ResolveGamePath(gameRoot,path));
    for (const auto& path : ipls) AppendInstances(world,ResolveGamePath(gameRoot,path));
    const auto archive=assets::ImgDirectory::Open(ResolveGamePath(gameRoot,"MODELS\\GTA3.IMG"));
    for (const auto& entry : archive.Entries()) {
        const auto name=Lower(entry.name);
        if (!name.ends_with(".ipl")) continue;
        AppendBinaryIpl(world,archive.Read(entry.name),name);
    }
    ResolveLodLinks(world);
    BuildSpatialIndex(world);
    return world;
}

void AppendBinaryIpl(Region& world, std::span<const std::uint8_t> bytes,
                     std::string_view sourceIpl) {
    world.spatialCells.clear();
    if (bytes.size() < 76 || std::memcmp(bytes.data(),"bnry",4) != 0)
        throw std::runtime_error("Invalid binary IPL header");
    const auto count=Read32(bytes,4);
    const auto offset=Read32(bytes,28);
    constexpr std::size_t recordSize=40;
    if (count > 1'000'000 || offset > bytes.size() ||
        std::uint64_t(count)*recordSize > bytes.size()-offset)
        throw std::runtime_error("Binary IPL instances exceed entry bounds");
    for (std::uint32_t i=0; i<count; ++i) {
        const auto base=std::size_t(offset)+std::size_t(i)*recordSize;
        Instance instance;
        for (int axis=0;axis<3;++axis) instance.position[axis]=ReadFloat(bytes,base+axis*4);
        for (int axis=0;axis<4;++axis) instance.rotation[axis]=ReadFloat(bytes,base+12+axis*4);
        instance.modelId=static_cast<std::int32_t>(Read32(bytes,base+28));
        instance.interior=static_cast<std::int32_t>(Read32(bytes,base+32) & 0xff);
        instance.lodIndex=static_cast<std::int32_t>(Read32(bytes,base+36));
        if (!sourceIpl.empty()) {
            const auto source=sourceIpl.ends_with(".ipl") ? sourceIpl.substr(0,sourceIpl.size()-4) : sourceIpl;
            instance.sourceIpl=Lower(source);
        }
        world.instances.push_back(std::move(instance));
    }
}

std::size_t Region::CellHash::operator()(Cell cell) const noexcept {
    const auto x=static_cast<std::uint64_t>(cell.x);
    const auto y=static_cast<std::uint64_t>(cell.y);
    return static_cast<std::size_t>((x*0x9e3779b97f4a7c15ULL) ^
                                    (y+0x517cc1b727220a95ULL));
}

void BuildSpatialIndex(Region& world) {
    world.spatialCells.clear();
    for (std::size_t index=0;index<world.instances.size();++index) {
        const auto& position=world.instances[index].position;
        if (!std::isfinite(position[0]) || !std::isfinite(position[1])) continue;
        world.spatialCells[{CellCoordinate(position[0]),CellCoordinate(position[1])}]
            .push_back(index);
    }
}

std::vector<const Instance*> InstancesNear(const Region& world,
                                           std::array<float, 3> center, float radius,
                                           std::int32_t interior) {
    if (!std::isfinite(radius) || radius < 0) throw std::runtime_error("Invalid world radius");
    std::vector<const Instance*> result;
    const double squared=double(radius)*radius;
    const auto consider=[&](const Instance& instance) {
        if (instance.interior != interior || !world.types.contains(instance.modelId)) return;
        const double dx=double(instance.position[0])-center[0];
        const double dy=double(instance.position[1])-center[1];
        const double dz=double(instance.position[2])-center[2];
        if (dx*dx+dy*dy+dz*dz <= squared) result.push_back(&instance);
    };
    if (!world.spatialCells.empty() && radius<=512.0f &&
        std::isfinite(center[0]) && std::isfinite(center[1]) &&
        std::abs(center[0])<1.0e9f && std::abs(center[1])<1.0e9f) {
        const auto minX=CellCoordinate(double(center[0])-radius);
        const auto maxX=CellCoordinate(double(center[0])+radius);
        const auto minY=CellCoordinate(double(center[1])-radius);
        const auto maxY=CellCoordinate(double(center[1])+radius);
        std::vector<std::size_t> candidates;
        for (auto x=minX;x<=maxX;++x) {
            for (auto y=minY;y<=maxY;++y) {
                const auto found=world.spatialCells.find({x,y});
                if (found==world.spatialCells.end()) continue;
                candidates.insert(candidates.end(),found->second.begin(),found->second.end());
            }
        }
        std::sort(candidates.begin(),candidates.end());
        for (const auto index : candidates) consider(world.instances[index]);
    } else {
        for (const auto& instance : world.instances) consider(instance);
    }
    return result;
}

std::filesystem::path ResolveGamePath(const std::filesystem::path& root,
                                      const std::string& windowsPath) {
    std::filesystem::path current=root;
    std::string component;
    auto append=[&] {
        if (component.empty() || component=="." || component=="..")
            throw std::runtime_error("Invalid game path component");
        const auto wanted=Lower(component);
        bool found=false;
        for (const auto& entry : std::filesystem::directory_iterator(current)) {
            if (Lower(entry.path().filename().string()) == wanted) {
                current=entry.path(); found=true; break;
            }
        }
        if (!found) throw std::runtime_error("Game path not found: "+windowsPath);
        component.clear();
    };
    for (char ch : windowsPath) {
        if (ch=='\\' || ch=='/') append();
        else component+=ch;
    }
    append();
    return current;
}
} // namespace gtasa::world
