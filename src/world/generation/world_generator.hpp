#pragma once
#include "core/id/id.hpp"
#include "core/rng/stable_hash.hpp"
#include "world/generation/urban_types.hpp"
#include "world/state/actor_types.hpp"
#include <string>
#include <vector>

namespace faultline {
struct WorldSeed { std::uint64_t value{}; auto operator<=>(const WorldSeed&) const = default; };
struct GeneratorVersion { std::uint32_t value{1}; auto operator<=>(const GeneratorVersion&) const = default; };
struct WorldMetadata { WorldSeed seed; GeneratorVersion generatorVersion; };
inline constexpr GeneratorVersion currentGeneratorVersion{3};
struct RoadNodeTag; using RoadNodeId = Id<RoadNodeTag>;
struct WaterRegionTag; using WaterRegionId = Id<WaterRegionTag>;
}
namespace faultline::world {
struct WorldGenerationConfig {
    std::int32_t width{8000}, depth{8000}, originX{-4000}, originZ{-4000};
    std::uint32_t minMacroZones{5}, maxMacroZones{10}, waterPercent{50};
    UrbanGenerationConfig urban;
};
struct GenerationContext {
    WorldSeed seed;
    GeneratorVersion version;
    WorldGenerationConfig config;
    [[nodiscard]] std::uint64_t stageSeed(std::string_view tag) const {
        core::StateHash hash; hash.u64(core::deriveSeed(seed.value, tag)); hash.u64(version.value); return hash.value();
    }
};
enum class ZoneArchetype : std::uint8_t { Downtown, Industrial, Park, Financial, Residential, Suburbs, Entertainment, University, Harbor, Waterfront };
enum class RoadCategory : std::uint8_t { Arterial, Major, Local, Service };
struct WaterRegion { WaterRegionId id; Rect boundary; };
struct MacroZone { MacroZoneId id; Rect boundary; ZoneArchetype archetype{}; };
struct ZoneAdjacency { MacroZoneId a, b; auto operator<=>(const ZoneAdjacency&) const = default; };
struct RoadNode { RoadNodeId id; std::int32_t x{}, z{}; };
struct RoadEdge { RoadEdgeId id; RoadNodeId a, b; RoadCategory category{}; };
struct RoadGraph { std::vector<RoadNode> nodes; std::vector<RoadEdge> edges; };
struct GeneratedDistrict {
    DistrictId id; MacroZoneId macroZone; Rect boundary; ZoneArchetype archetype{};
    std::uint64_t population{};
    std::uint32_t density{}, wealth{};
};
struct GeneratedWorld {
    WorldMetadata metadata;
    WorldGenerationConfig config;
    Rect bounds, buildableLand;
    std::vector<WaterRegion> water;
    std::vector<MacroZone> macroZones;
    std::vector<ZoneAdjacency> adjacency;
    RoadGraph roads;
    std::vector<GeneratedDistrict> districts;
    std::vector<Parcel> parcels;
    std::vector<BuildingState> buildings;
    std::vector<GeneratedTerritory> territories;
    std::vector<GeneratedFaction> factions;
    std::vector<GeneratedNpc> majorNpcs;
    std::vector<TerritoryAssignment> territoryAssignments;
    std::vector<FactionRelationship> factionRelationships;
    std::vector<NpcRelationship> npcRelationships;
};
struct ValidationReport {
    std::vector<std::string> errors;
    [[nodiscard]] bool passed() const { return errors.empty(); }
};
void validateConfig(const WorldGenerationConfig& config);
[[nodiscard]] GeneratedWorld generateWorld(const GenerationContext& context);
[[nodiscard]] ValidationReport validateGeneratedWorld(const GeneratedWorld& world);
[[nodiscard]] std::uint64_t baseWorldHash(const GeneratedWorld& world);
}
