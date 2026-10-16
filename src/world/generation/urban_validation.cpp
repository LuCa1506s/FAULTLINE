#include "world/generation/urban_generation.hpp"
#include "world/geometry/rect_index.hpp"
#include <map>
#include <set>

namespace faultline::world {
namespace {
bool frontageMatches(const Parcel& parcel, const RoadReservation& road, const UrbanGenerationConfig& config) {
    const auto r = parcel.boundary; const auto side = parcel.frontage.side;
    if (side > FrontageSide::West || road.vertical != (side == FrontageSide::East || side == FrontageSide::West)) return false;
    const bool touches = side == FrontageSide::North ? r.z == bottom(road.boundary)
        : side == FrontageSide::South ? bottom(r) == road.boundary.z
        : side == FrontageSide::West ? r.x == right(road.boundary) : right(r) == road.boundary.x;
    const auto start = std::max<std::int64_t>(road.start, road.vertical ? r.z : r.x);
    const auto end = std::min<std::int64_t>(road.end, road.vertical ? bottom(r) : right(r));
    return touches && end - start >= config.minParcelSpan && start == parcel.frontage.start && end - start == parcel.frontage.length;
}
}
ValidationReport validateUrbanWorld(const GeneratedWorld& world) {
    ValidationReport report;
    const auto fail = [&](const char* message, std::uint64_t id = 0) {
        report.errors.push_back(std::string{message} + " [id=" + std::to_string(id) + "]");
    };
    try { validateUrbanConfig(world.config.urban); } catch (const std::exception& e) { report.errors.push_back(e.what()); return report; }
    const auto& config = world.config.urban;
    if (world.parcels.empty() || world.buildings.empty() || world.territories.empty()) {
        fail("M2 requires parcels, buildings and territories"); return report;
    }
    if (world.parcels.size() > config.maxParcels) { fail("Parcel resource limit exceeded"); return report; }
    std::map<DistrictId, Rect> districts;
    for (const auto& district : world.districts) districts.emplace(district.id, district.boundary);
    std::vector<RoadReservation> roads;
    try { roads = roadReservations(world); } catch (const std::exception& e) { report.errors.push_back(e.what()); return report; }
    std::map<RoadEdgeId, const RoadReservation*> roadById;
    std::vector<Rect> roadBounds;
    for (const auto& road : roads) { roadBounds.push_back(road.boundary); roadById.emplace(road.road, &road); }
    const RectIndex roadIndex{roadBounds};
    const auto forbidden = [&](Rect r) {
        return !inside(r, world.bounds) || !inside(r, world.buildableLand) || !roadIndex.query(r).empty() ||
            std::any_of(world.water.begin(), world.water.end(), [&](const auto& water) { return overlap(r, water.boundary); });
    };
    std::map<ParcelId, const Parcel*> parcels; std::vector<Rect> parcelBounds;
    for (const auto& p : world.parcels) {
        if (!p.id || !parcels.emplace(p.id,&p).second) fail("Duplicate/null parcel ID", p.id.value());
        if (!valid(p.boundary) || !districts.contains(p.district) || !inside(p.boundary, districts.at(p.district)) ||
            forbidden(p.boundary) || static_cast<std::uint64_t>(area(p.boundary)) != p.areaSquareMeters ||
            std::min(p.boundary.width,p.boundary.depth) < config.minParcelSpan) fail("Invalid parcel geometry/area/district", p.id.value());
        if (!roadById.contains(p.frontage.road) || !frontageMatches(p, *roadById.at(p.frontage.road), config))
            fail("Invalid parcel road frontage", p.id.value());
        parcelBounds.push_back(p.boundary);
    }
    if (!report.passed()) return report;
    const RectIndex parcelIndex{parcelBounds};
    for (std::size_t i = 0; i < parcelBounds.size(); ++i)
        for (auto other : parcelIndex.query(parcelBounds[i])) if (other > i) fail("Overlapping parcels", world.parcels[i].id.value());
    std::set<BuildingId> buildingIds; std::set<ParcelId> occupied; std::vector<Rect> buildingBounds;
    for (const auto& b : world.buildings) {
        if (!b.id || !buildingIds.insert(b.id).second) fail("Duplicate/null building ID", b.id.value());
        if (!parcels.contains(b.parcel)) { fail("Missing building parcel", b.id.value()); continue; }
        const auto& p = *parcels.at(b.parcel);
        if (!occupied.insert(b.parcel).second || !p.buildable) fail("Invalid building parcel occupancy", b.id.value());
        if (!valid(b.footprint) || b.district != p.district || forbidden(b.footprint) ||
            !inside(b.footprint, inset(p.boundary,config.buildingSetback))) fail("Invalid building footprint/district", b.id.value());
        if (b.archetype > BuildingArchetype::Garage || b.flags != BuildingFlags::NoInterior)
            fail("Invalid building archetype/flags", b.id.value());
        if (b.transform.front != p.frontage.side || b.transform.front > FrontageSide::West ||
            b.transform.centerX2 != static_cast<std::int64_t>(b.footprint.x)*2 + b.footprint.width ||
            b.transform.centerZ2 != static_cast<std::int64_t>(b.footprint.z)*2 + b.footprint.depth)
            fail("Invalid building transform/orientation", b.id.value());
        buildingBounds.push_back(b.footprint);
    }
    if (!report.passed()) return report;
    const RectIndex buildingIndex{buildingBounds};
    for (std::size_t i = 0; i < buildingBounds.size(); ++i)
        for (auto other : buildingIndex.query(buildingBounds[i])) if (other > i) fail("Overlapping buildings", world.buildings[i].id.value());
    std::set<TerritoryId> territoryIds; std::map<DistrictId, std::int64_t> coverage;
    std::map<DistrictId,std::uint32_t> territoryCount; std::vector<Rect> territoryBounds;
    for (const auto& t : world.territories) {
        if (!t.id || !territoryIds.insert(t.id).second) fail("Duplicate/null territory ID", t.id.value());
        if (!valid(t.boundary) || !districts.contains(t.district) || !inside(t.boundary,districts.at(t.district))) {
            fail("Invalid territory geometry/district", t.id.value()); continue;
        }
        const auto parent = districts.at(t.district);
        if (t.boundary != parent && (static_cast<std::uint64_t>(area(t.boundary)) < config.minTerritoryArea ||
            std::min(t.boundary.width,t.boundary.depth) < config.minTerritorySpan)) fail("Territory subdivision too small", t.id.value());
        if ((t.boundary.x != parent.x && !roadBoundaryCovers(true,t.boundary.x,t.boundary.z,bottom(t.boundary),roads)) ||
            (right(t.boundary) != right(parent) && !roadBoundaryCovers(true,right(t.boundary),t.boundary.z,bottom(t.boundary),roads)) ||
            (t.boundary.z != parent.z && !roadBoundaryCovers(false,t.boundary.z,t.boundary.x,right(t.boundary),roads)) ||
            (bottom(t.boundary) != bottom(parent) && !roadBoundaryCovers(false,bottom(t.boundary),t.boundary.x,right(t.boundary),roads)))
            fail("Territory boundary lacks a major road", t.id.value());
        coverage[t.district] += area(t.boundary); ++territoryCount[t.district]; territoryBounds.push_back(t.boundary);
        std::uint64_t buildable = 0; std::uint32_t count = 0;
        for (auto i : parcelIndex.query(t.boundary)) {
            const auto& parcel = world.parcels[i];
            if (parcel.buildable) buildable += static_cast<std::uint64_t>(area(*intersection(parcel.boundary,t.boundary)));
        }
        for (auto i : buildingIndex.query(t.boundary)) if (inside(world.buildings[i].footprint,t.boundary)) ++count;
        if (count != t.buildingCount || buildable != t.buildableArea) fail("Invalid territory baseline metadata", t.id.value());
    }
    if (!report.passed()) return report;
    const RectIndex territoryIndex{territoryBounds};
    for (std::size_t i = 0; i < territoryBounds.size(); ++i)
        for (auto other : territoryIndex.query(territoryBounds[i])) if (other > i) fail("Overlapping territories", world.territories[i].id.value());
    for (const auto& [id,bounds] : districts)
        if (coverage[id] != area(bounds) || territoryCount[id] > config.maxTerritoriesPerDistrict) fail("Territory coverage/count mismatch",id.value());
    for (const auto& b : world.buildings) {
        std::uint32_t owners = 0;
        for (auto i : territoryIndex.query(b.footprint)) if (world.territories[i].district == b.district && inside(b.footprint,world.territories[i].boundary)) ++owners;
        if (owners != 1) fail("Building must belong to exactly one territory",b.id.value());
    }
    return report;
}
}
