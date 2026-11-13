#include "world/generation/urban_generation.hpp"

namespace faultline::world {
namespace {
void rectHash(core::StateHash& hash, Rect r) {
    hash.u64(static_cast<std::uint64_t>(r.x)); hash.u64(static_cast<std::uint64_t>(r.z));
    hash.u64(static_cast<std::uint64_t>(r.width)); hash.u64(static_cast<std::uint64_t>(r.depth));
}
void appendParcels(core::StateHash& hash, std::span<const Parcel> parcels) {
    hash.u64(parcels.size());
    for (const auto& p : parcels) {
        hash.u64(p.id.value()); hash.u64(p.district.value()); rectHash(hash, p.boundary); hash.u64(p.areaSquareMeters);
        hash.u64(p.frontage.road.value()); hash.u64(static_cast<std::uint64_t>(p.frontage.side));
        hash.u64(static_cast<std::uint64_t>(p.frontage.start)); hash.u64(static_cast<std::uint64_t>(p.frontage.length)); hash.u64(p.buildable);
    }
}
void appendBuildings(core::StateHash& hash, std::span<const BuildingState> buildings) {
    hash.u64(buildings.size());
    for (const auto& b : buildings) {
        hash.u64(b.id.value()); hash.u64(b.district.value()); hash.u64(b.parcel.value()); hash.u64(static_cast<std::uint64_t>(b.archetype));
        rectHash(hash, b.footprint); hash.u64(static_cast<std::uint64_t>(b.transform.centerX2)); hash.u64(static_cast<std::uint64_t>(b.transform.centerZ2));
        hash.u64(static_cast<std::uint64_t>(b.transform.front)); hash.u64(static_cast<std::uint64_t>(b.flags));
    }
}
void appendTerritories(core::StateHash& hash, std::span<const GeneratedTerritory> territories) {
    hash.u64(territories.size());
    for (const auto& t : territories) {
        hash.u64(t.id.value()); hash.u64(t.district.value()); rectHash(hash, t.boundary); hash.u64(t.buildingCount); hash.u64(t.buildableArea);
    }
}
}
void appendUrbanHash(core::StateHash& hash, const GeneratedWorld& world) {
    const auto& c = world.config.urban;
    for (auto value : c.roadHalfWidths) hash.u64(static_cast<std::uint64_t>(value));
    for (auto value : {c.minParcelSpan, c.maxParcelDepth, c.targetParcelFrontage, c.buildingSetback, c.minTerritorySpan})
        hash.u64(static_cast<std::uint64_t>(value));
    hash.u64(c.minTerritoryArea); hash.u64(c.targetTerritoryArea); hash.u64(c.maxTerritoriesPerDistrict); hash.u64(c.maxParcels);
    for (const auto& weights : {c.residential, c.commercial, c.industrial, c.park}) for (auto value : weights) hash.u64(value);
    appendParcels(hash, world.parcels); appendBuildings(hash, world.buildings); appendTerritories(hash, world.territories);
}
std::uint64_t parcelLayoutHash(std::span<const Parcel> parcels) {
    core::StateHash hash; hash.string("FAULTLINE.parcels.v2"); appendParcels(hash, parcels); return hash.value();
}
std::uint64_t buildingLayoutHash(std::span<const BuildingState> buildings) {
    core::StateHash hash; hash.string("FAULTLINE.buildings.v2"); appendBuildings(hash, buildings); return hash.value();
}
std::uint64_t territoryLayoutHash(std::span<const GeneratedTerritory> territories) {
    core::StateHash hash; hash.string("FAULTLINE.territories.v2"); appendTerritories(hash, territories); return hash.value();
}
}
