#include "world/generation/world_generator.hpp"
#include "world/generation/actor_generation.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
std::uint64_t integer(std::string_view text) {
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) throw std::invalid_argument("Invalid unsigned integer");
    return value;
}
}
int main(int argc, char** argv) {
    using namespace faultline;
    using namespace faultline::world;
    GenerationContext context{{12345}, currentGeneratorVersion, {}};
    bool range = false, explicitSeed = false, dumpFactions=false, dumpNpcs=false;
    const auto milestone=[&] {return context.version.value==1 ? "M1" : context.version.value==2 ? "M2" : "M3";};
    std::uint64_t first{}, last{};
    try {
        const auto argument = [&](int& i) -> std::string_view {
            if (++i >= argc) throw std::invalid_argument("Missing option value");
            return argv[i];
        };
        const auto u32 = [&](int& i) {
            const auto value = integer(argument(i));
            if (value > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("Integer out of range");
            return static_cast<std::uint32_t>(value);
        };
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--seed") { context.seed.value = integer(argument(i)); explicitSeed = true; }
            else if (option == "--version") context.version.value = u32(i);
            else if (option == "--range") { range = true; first = integer(argument(i)); last = integer(argument(i)); }
            else if (option == "--dump-factions") dumpFactions=true;
            else if (option == "--dump-npcs") dumpNpcs=true;
            else if (option == "--width" || option == "--depth") {
                const auto value = u32(i);
                if (value > 1000000) throw std::invalid_argument("World dimension exceeds M1 resource limit");
                if (option == "--width") context.config.width = static_cast<std::int32_t>(value);
                else context.config.depth = static_cast<std::int32_t>(value);
            } else if (option == "--water-percent") context.config.waterPercent = u32(i);
            else if (option == "--validate") { /* Generation always validates before accepting a world. */ }
            else if (option == "--help") {
                std::cout << "faultline_worldgen [--seed N | --range FIRST LAST] [--version 1|2|3] [--validate]\n"
                    "                  [--width METERS] [--depth METERS] [--water-percent 0..100]\n"
                    "                  [--dump-factions] [--dump-npcs]\n"; return 0;
            } else throw std::invalid_argument("Unknown option: " + std::string{option});
        }
        if (range && (explicitSeed || first > last || last - first >= 1000000))
            throw std::invalid_argument("Use a seed or an ascending range of at most 1000000 seeds");
        if (range && (dumpFactions || dumpNpcs)) throw std::invalid_argument("Actor dumps require a single seed");
        if (range) {
            std::uint64_t tested = 0;
            std::array<std::uint64_t,4> duplication{}, diplomacy{};
            std::map<std::size_t,std::uint64_t> npcCounts;
            std::uint64_t minShare=10000,maxShare=0;
            const auto started=std::chrono::steady_clock::now();
            for (auto seed = first;; ++seed) {
                context.seed.value = seed;
                const auto world = generateWorld(context);
                // generateWorld already validates the complete result before returning.
                if (context.version.value==3) {
                    std::array<std::uint32_t,4> counts{};
                    for (const auto& f : world.factions) ++counts[static_cast<std::size_t>(f.state.family)];
                    for (std::size_t i=0; i<counts.size(); ++i) if (counts[i]==2) ++duplication[i];
                    ++npcCounts[world.majorNpcs.size()];
                    std::map<TerritoryId,std::uint64_t> territoryAreas; std::uint64_t totalArea=0;
                    for (const auto& t : world.territories) {territoryAreas[t.id]=static_cast<std::uint64_t>(area(t.boundary)); totalArea+=static_cast<std::uint64_t>(area(t.boundary));}
                    std::map<FactionId,std::uint64_t> controlled;
                    for (const auto& a : world.territoryAssignments) if (a.owner) controlled[a.owner]+=territoryAreas.at(a.territory);
                    for (const auto& f : world.factions) {
                        const auto share=controlled.at(f.id)*10000/totalArea;
                        minShare=std::min(minShare,share); maxShare=std::max(maxShare,share);
                    }
                    for (const auto& r : world.factionRelationships) ++diplomacy[static_cast<std::size_t>(diplomaticState(r))];
                }
                ++tested; if (seed == last) break;
            }
            std::cout << "Generator Version: " << context.version.value << "\nSeed range: " << first << ".." << last
                      << "\nSeeds tested: " << tested << "\nValidation: PASS (" << milestone() << ")\n";
            if (context.version.value==3) {
                for (std::size_t i=0; i<duplication.size(); ++i) std::cout << "Duplicated family [" << familyNames[i] << "]: " << duplication[i] << '\n';
                for (const auto& [count,frequency] : npcCounts) std::cout << "MajorNPC count [" << count << "]: " << frequency << '\n';
                std::cout << "Faction territory share (basis points): " << minShare << ".." << maxShare << '\n';
                const std::array labels{"FRIENDLY","NEUTRAL","TENSE","HOSTILE"};
                for (std::size_t i=0; i<labels.size(); ++i) std::cout << "Diplomacy [" << labels[i] << "]: " << diplomacy[i] << '\n';
            }
            const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            std::cout << "Batch seconds: " << std::fixed << std::setprecision(3) << elapsed << "\nMilliseconds/seed: " << elapsed*1000/static_cast<double>(tested) << '\n';
            return 0;
        }
        const auto world = generateWorld(context);
        const auto validation = validateGeneratedWorld(world);
        std::cout << "FAULTLINE WORLD GENERATOR (" << milestone() << ")\n\nSeed: " << context.seed.value
            << "\nGenerator Version: " << context.version.value << "\nWorld: " << world.bounds.width << " x " << world.bounds.depth
            << " m\nWaterRegions: " << world.water.size() << "\nMacroZones: " << world.macroZones.size()
            << "\nRoadNodes: " << world.roads.nodes.size() << "\nRoadEdges: " << world.roads.edges.size()
            << "\nDistricts: " << world.districts.size()
            << "\nParcels: " << world.parcels.size() << "\nBuildings: " << world.buildings.size() << "\nTerritories: " << world.territories.size()
            << "\nFactions: " << world.factions.size() << "\nMajorNPCs: " << world.majorNpcs.size()
            << "\nValidation: " << (validation.passed() ? "PASS" : "FAIL")
            << "\nBaseWorldHash: " << std::hex << std::setfill('0') << std::setw(16) << baseWorldHash(world) << '\n';
        std::cout << std::dec;
        if (dumpFactions) for (const auto& f : world.factions) {
            const auto count=std::count_if(world.majorNpcs.begin(),world.majorNpcs.end(),[&](const auto& n){return n.state.faction==f.id;});
            std::cout << "Faction #" << f.id << " | " << f.state.name << " | " << familyNames[static_cast<std::size_t>(f.state.family)]
                << " | Home: Territory #" << f.state.homeTerritory << " | Leader: NPC #" << f.state.leader << " | MajorNPCs: " << count
                << " | Treasury: " << f.state.treasury << " | Manpower: " << factionManpower(world,f.id) << '\n';
        }
        if (dumpNpcs) for (const auto& n : world.majorNpcs) std::cout << "NPC #" << n.id << " | " << n.state.identity.name
            << " | Faction #" << n.state.faction << " | Authority: " << static_cast<unsigned>(n.state.authority) << " | Roles: " << n.state.roles.bits
            << " | Territory #" << n.state.location.territory << " | Building #" << n.state.location.building << '\n';
        return validation.passed() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Worldgen failed. Seed: " << context.seed.value << " GeneratorVersion: " << context.version.value
                  << "\n" << error.what() << '\n'; return 1;
    }
}
