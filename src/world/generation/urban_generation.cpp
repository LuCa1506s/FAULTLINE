#include "world/generation/urban_generation.hpp"
#include "core/rng/deterministic_rng.hpp"
#include "world/geometry/rect_index.hpp"
#include <map>
#include <numeric>
#include <tuple>

namespace faultline::world {
namespace {
std::vector<Rect> reservationBounds(const std::vector<RoadReservation>& roads) {
    std::vector<Rect> bounds; bounds.reserve(roads.size());
    for (const auto& road : roads) bounds.push_back(road.boundary);
    return bounds;
}
std::optional<RoadFrontage> frontageFor(Rect lot, FrontageSide side, const RoadReservation& road) {
    if (road.vertical != (side == FrontageSide::East || side == FrontageSide::West)) return {};
    const auto r = road.boundary;
    const bool touches = side == FrontageSide::North ? lot.z == bottom(r)
        : side == FrontageSide::South ? bottom(lot) == r.z
        : side == FrontageSide::West ? lot.x == right(r) : right(lot) == r.x;
    if (!touches) return {};
    const auto start = std::max<std::int64_t>(road.start, road.vertical ? lot.z : lot.x);
    const auto end = std::min<std::int64_t>(road.end, road.vertical ? bottom(lot) : right(lot));
    if (end <= start) return {};
    return RoadFrontage{road.road, side, static_cast<std::int32_t>(start), static_cast<std::int32_t>(end - start)};
}
std::optional<RoadFrontage> bestFrontage(Rect lot, FrontageSide side, const std::vector<RoadReservation>& roads) {
    std::optional<RoadFrontage> best;
    for (const auto& road : roads) if (const auto candidate = frontageFor(lot, side, road)) {
        if (!best || candidate->length > best->length || (candidate->length == best->length && candidate->road < best->road)) best = candidate;
    }
    return best;
}
std::uint64_t blockSeed(const GenerationContext& context, DistrictId district, Rect block) {
    core::StateHash hash; hash.u64(core::deriveSeed(context.stageSeed("parcels"), "district", district));
    hash.u64(static_cast<std::uint64_t>(block.x)); hash.u64(static_cast<std::uint64_t>(block.z));
    hash.u64(static_cast<std::uint64_t>(block.width)); hash.u64(static_cast<std::uint64_t>(block.depth)); return hash.value();
}
const BuildingWeights& weightsFor(ZoneArchetype archetype, const UrbanGenerationConfig& config) {
    switch (archetype) {
    case ZoneArchetype::Industrial: case ZoneArchetype::Harbor: return config.industrial;
    case ZoneArchetype::Downtown: case ZoneArchetype::Financial: case ZoneArchetype::Entertainment: return config.commercial;
    case ZoneArchetype::Park: return config.park;
    default: return config.residential;
    }
}
constexpr std::array<std::uint64_t, 7> minimumBuildingArea{16, 64, 16, 64, 100, 100, 9};
}
void validateUrbanConfig(const UrbanGenerationConfig& c) {
    for (const auto width : c.roadHalfWidths) core::require(width > 0 && width <= 64, "Road half-width must be 1..64 meters");
    core::require(c.minParcelSpan >= 8 && c.minParcelSpan <= 128 && c.maxParcelDepth >= c.minParcelSpan && c.maxParcelDepth <= 256,
                  "Invalid parcel dimensions");
    core::require(c.targetParcelFrontage >= c.minParcelSpan * 2 && c.targetParcelFrontage <= 256,
                  "Invalid target frontage");
    core::require(c.buildingSetback >= 1 && static_cast<std::int64_t>(c.buildingSetback) * 2 + 4 <= c.minParcelSpan,
                  "Setback leaves no usable building footprint");
    core::require(c.minTerritoryArea > 0 && c.targetTerritoryArea >= c.minTerritoryArea && c.targetTerritoryArea <= 1000000000000ULL &&
        c.minTerritorySpan >= 16 && c.minTerritorySpan <= 1000000 && c.maxTerritoriesPerDistrict >= 1 &&
        c.maxTerritoriesPerDistrict <= 32 && c.maxParcels >= 1 && c.maxParcels <= 1000000, "Invalid M2 resource limits");
    for (const auto& weights : {c.residential, c.commercial, c.industrial, c.park})
        core::require(std::accumulate(weights.begin(), weights.end(), std::uint64_t{0}) > 0, "Empty building weight profile");
}
std::vector<RoadReservation> roadReservations(const GeneratedWorld& world) {
    validateUrbanConfig(world.config.urban);
    std::map<RoadNodeId, RoadNode> nodes;
    for (const auto& node : world.roads.nodes) core::require(nodes.emplace(node.id, node).second, "Duplicate road node");
    std::vector<RoadReservation> result;
    for (const auto& edge : world.roads.edges) {
        core::require(edge.category <= RoadCategory::Service, "Invalid road category");
        const auto a = nodes.at(edge.a), b = nodes.at(edge.b);
        core::require((a.x == b.x) != (a.z == b.z), "M2 requires orthogonal nonzero roads");
        const auto radius = world.config.urban.roadHalfWidths[static_cast<std::size_t>(edge.category)];
        // Square end caps cover intersections. Clip in int64 before constructing Rect.
        const auto x = std::max<std::int64_t>(world.bounds.x, static_cast<std::int64_t>(std::min(a.x,b.x)) - radius);
        const auto z = std::max<std::int64_t>(world.bounds.z, static_cast<std::int64_t>(std::min(a.z,b.z)) - radius);
        const auto endX = std::min(right(world.bounds), static_cast<std::int64_t>(std::max(a.x,b.x)) + radius);
        const auto endZ = std::min(bottom(world.bounds), static_cast<std::int64_t>(std::max(a.z,b.z)) + radius);
        core::require(endX > x && endZ > z, "Road outside world bounds");
        result.push_back({edge.id, {static_cast<std::int32_t>(x), static_cast<std::int32_t>(z),
            static_cast<std::int32_t>(endX-x), static_cast<std::int32_t>(endZ-z)}, a.x == b.x,
            a.x == b.x ? a.x : a.z, a.x == b.x ? std::min(a.z,b.z) : std::min(a.x,b.x),
            a.x == b.x ? std::max(a.z,b.z) : std::max(a.x,b.x), edge.category});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.road < b.road; });
    return result;
}
bool roadBoundaryCovers(bool vertical, std::int64_t axis, std::int64_t start, std::int64_t end,
                       std::span<const RoadReservation> roads) {
    if (end <= start) return false;
    std::vector<std::pair<std::int64_t,std::int64_t>> intervals;
    for (const auto& road : roads) if (road.vertical == vertical && road.axis == axis && road.category <= RoadCategory::Major)
        intervals.emplace_back(road.start, road.end);
    std::sort(intervals.begin(), intervals.end());
    auto reached = start;
    for (const auto& [a,b] : intervals) { if (a > reached) break; reached = std::max(reached, b); if (reached >= end) return true; }
    return false;
}
ParcelGenerationResult generateParcels(const GenerationContext& context, const GeneratedWorld& topology) {
    validateUrbanConfig(context.config.urban);
    const auto& config = context.config.urban;
    core::require(config.roadHalfWidths == topology.config.urban.roadHalfWidths, "Parcel/topology road rules differ");
    const auto roads = roadReservations(topology);
    const auto roadBounds = reservationBounds(roads); const RectIndex roadIndex{roadBounds};
    std::map<std::int32_t, std::int32_t> xs, zs;
    for (const auto& road : roads) {
        auto& axis = road.vertical ? xs : zs;
        axis[road.axis] = std::max(axis[road.axis], config.roadHalfWidths[static_cast<std::size_t>(road.category)]);
    }
    std::vector<GeneratedDistrict> districts = topology.districts;
    std::sort(districts.begin(), districts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    ParcelGenerationResult result;
    for (const auto& district : districts) for (auto x = xs.begin(); x != xs.end(); ++x) {
        const auto nx = std::next(x); if (nx == xs.end()) break;
        for (auto z = zs.begin(); z != zs.end(); ++z) {
            const auto nz = std::next(z); if (nz == zs.end()) break;
            const auto startX = static_cast<std::int64_t>(x->first) + x->second, startZ = static_cast<std::int64_t>(z->first) + z->second;
            const auto endX = static_cast<std::int64_t>(nx->first) - nx->second, endZ = static_cast<std::int64_t>(nz->first) - nz->second;
            if (endX <= startX || endZ <= startZ) continue;
            const Rect block{static_cast<std::int32_t>(startX), static_cast<std::int32_t>(startZ),
                static_cast<std::int32_t>(endX-startX), static_cast<std::int32_t>(endZ-startZ)};
            const auto clipped = intersection(block, district.boundary); if (!clipped) continue;
            Rect remaining = *clipped;
            core::DeterministicRng rng{blockSeed(context, district.id, block)};
            // Perimeter strips are carved in fixed order; corners are owned once.
            for (const auto side : {FrontageSide::North, FrontageSide::South, FrontageSide::West, FrontageSide::East}) {
                if (!valid(remaining)) break;
                const auto frontage = bestFrontage(remaining, side, roads);
                if (!frontage || frontage->length < config.minParcelSpan) continue;
                const bool vertical = side == FrontageSide::West || side == FrontageSide::East;
                const auto opposite = side == FrontageSide::North ? FrontageSide::South : side == FrontageSide::West ? FrontageSide::East
                    : side == FrontageSide::South ? FrontageSide::North : FrontageSide::West;
                const auto span = vertical ? remaining.width : remaining.depth;
                auto depth = std::min(config.maxParcelDepth, span);
                if (bestFrontage(remaining, opposite, roads) && span >= config.minParcelSpan * 2) depth = std::min(depth, span / 2);
                if (depth < config.minParcelSpan) continue;
                Rect strip = remaining;
                if (vertical) { strip.width = depth; if (side == FrontageSide::East) strip.x += remaining.width - depth; }
                else { strip.depth = depth; if (side == FrontageSide::South) strip.z += remaining.depth - depth; }
                if (side == FrontageSide::North) { remaining.z += depth; remaining.depth -= depth; }
                if (side == FrontageSide::South) remaining.depth -= depth;
                if (side == FrontageSide::West) { remaining.x += depth; remaining.width -= depth; }
                if (side == FrontageSide::East) remaining.width -= depth;
                const auto length = vertical ? strip.depth : strip.width;
                const auto target = static_cast<std::int32_t>(static_cast<std::int64_t>(config.targetParcelFrontage) * static_cast<std::int64_t>(rng.uniform(3, 6)) / 4);
                const auto count = std::max(1, (length + target - 1) / target);
                for (std::int32_t i = 0; i < count; ++i) {
                    const auto start = static_cast<std::int32_t>(static_cast<std::int64_t>(length) * i / count);
                    const auto end = static_cast<std::int32_t>(static_cast<std::int64_t>(length) * (i+1) / count);
                    Rect lot = strip;
                    if (vertical) { lot.z += start; lot.depth = end - start; } else { lot.x += start; lot.width = end - start; }
                    if (std::min(lot.width, lot.depth) < config.minParcelSpan) continue;
                    const auto access = bestFrontage(lot, side, roads);
                    if (!access || access->length < config.minParcelSpan || !roadIndex.query(lot).empty()) continue;
                    if (std::any_of(topology.water.begin(), topology.water.end(), [&](const auto& water) { return overlap(lot, water.boundary); })) continue;
                    core::require(result.parcels.size() < config.maxParcels, "M2 parcel resource limit exceeded");
                    result.parcels.push_back({ParcelId{result.parcels.size()+1}, district.id, lot, static_cast<std::uint64_t>(area(lot)), *access, true});
                }
            }
        }
    }
    return result;
}
BuildingGenerationResult generateBuildings(const GenerationContext& context, std::span<const GeneratedDistrict> districts,
                                           std::span<const Parcel> parcels) {
    validateUrbanConfig(context.config.urban);
    std::map<DistrictId, ZoneArchetype> archetypes;
    for (const auto& district : districts) archetypes.emplace(district.id, district.archetype);
    std::vector<Parcel> ordered(parcels.begin(), parcels.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    BuildingGenerationResult result;
    for (const auto& parcel : ordered) {
        if (!parcel.buildable) continue;
        core::DeterministicRng rng{core::deriveSeed(context.stageSeed("buildings"), "parcel", parcel.id)};
        const auto available = inset(parcel.boundary, context.config.urban.buildingSetback);
        const auto width = std::max(4, static_cast<std::int32_t>(static_cast<std::int64_t>(available.width) * static_cast<std::int64_t>(rng.uniform(60, 91)) / 100));
        const auto depth = std::max(4, static_cast<std::int32_t>(static_cast<std::int64_t>(available.depth) * static_cast<std::int64_t>(rng.uniform(60, 91)) / 100));
        Rect footprint{available.x + (available.width-width)/2, available.z + (available.depth-depth)/2, width, depth};
        // The front setback is fixed; unused lot depth stays behind the building.
        if (parcel.frontage.side == FrontageSide::North) footprint.z = available.z;
        if (parcel.frontage.side == FrontageSide::South) footprint.z = static_cast<std::int32_t>(bottom(available) - depth);
        if (parcel.frontage.side == FrontageSide::West) footprint.x = available.x;
        if (parcel.frontage.side == FrontageSide::East) footprint.x = static_cast<std::int32_t>(right(available) - width);
        auto weights = weightsFor(archetypes.at(parcel.district), context.config.urban);
        for (std::size_t i = 0; i < weights.size(); ++i) if (static_cast<std::uint64_t>(area(footprint)) < minimumBuildingArea[i]) weights[i] = 0;
        const auto total = std::accumulate(weights.begin(), weights.end(), std::uint64_t{0}); if (total == 0) continue;
        auto choice = rng.uniform(total); std::size_t selected = 0;
        for (; selected < weights.size(); ++selected) { if (choice < weights[selected]) break; choice -= weights[selected]; }
        result.buildings.push_back({BuildingId{result.buildings.size()+1}, parcel.district, parcel.id,
            static_cast<BuildingArchetype>(selected), footprint,
            {static_cast<std::int64_t>(footprint.x)*2 + footprint.width, static_cast<std::int64_t>(footprint.z)*2 + footprint.depth, parcel.frontage.side},
            BuildingFlags::NoInterior});
    }
    return result;
}
TerritoryGenerationResult generateTerritories(const GenerationContext& context, const GeneratedWorld& topology,
    std::span<const Parcel> parcels, std::span<const BuildingState> buildings) {
    validateUrbanConfig(context.config.urban); const auto& config = context.config.urban;
    const auto roads = roadReservations(topology);
    // A road axis can contain many graph edges. Evaluate each geometric cut once.
    std::map<std::pair<bool,std::int32_t>,RoadReservation> boundaries;
    for (const auto& road : roads) if (road.category <= RoadCategory::Major) {
        const auto key = std::pair{road.vertical,road.axis};
        const auto [it, inserted] = boundaries.emplace(key,road);
        if (!inserted && road.category < it->second.category) it->second = road;
    }
    std::vector<GeneratedDistrict> districts = topology.districts;
    std::sort(districts.begin(), districts.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    TerritoryGenerationResult result;
    for (const auto& district : districts) {
        core::DeterministicRng rng{core::deriveSeed(context.stageSeed("territories"), "district", district.id)};
        const auto buildingsInDistrict = std::count_if(buildings.begin(), buildings.end(), [&](const auto& b) { return b.district == district.id; });
        const auto target = std::min<std::uint64_t>(config.maxTerritoriesPerDistrict,
            1 + static_cast<std::uint64_t>(area(district.boundary)) / config.targetTerritoryArea + static_cast<std::uint64_t>(buildingsInDistrict) / 100);
        std::vector<Rect> regions{district.boundary};
        while (regions.size() < target) {
            struct Cut { std::size_t region{}; Rect a, b; std::uint64_t score{}; };
            std::vector<Cut> candidates;
            for (std::size_t i = 0; i < regions.size(); ++i) for (const auto& entry : boundaries) {
                const auto& road = entry.second;
                const auto r = regions[i]; Rect a = r, b = r;
                if (road.vertical) {
                    if (road.axis <= r.x || road.axis >= right(r)) continue;
                    a.width = road.axis - r.x; b.x = road.axis; b.width = static_cast<std::int32_t>(right(r) - road.axis);
                } else {
                    if (road.axis <= r.z || road.axis >= bottom(r)) continue;
                    a.depth = road.axis - r.z; b.z = road.axis; b.depth = static_cast<std::int32_t>(bottom(r) - road.axis);
                }
                if (std::min({a.width, a.depth, b.width, b.depth}) < config.minTerritorySpan ||
                    static_cast<std::uint64_t>(std::min(area(a),area(b))) < config.minTerritoryArea) continue;
                if (!roadBoundaryCovers(road.vertical,road.axis,road.vertical ? r.z : r.x,
                    road.vertical ? bottom(r) : right(r),roads)) continue;
                const auto score = static_cast<std::uint64_t>(std::min(area(a),area(b))) * (road.category == RoadCategory::Arterial ? 3ULL : 2ULL);
                candidates.push_back({i, a, b, score});
            }
            if (candidates.empty()) break; // No suitable real boundary; preserve the connected region.
            const auto best = std::max_element(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.score < b.score; })->score;
            std::erase_if(candidates, [best](const auto& cut) { return cut.score != best; });
            std::sort(candidates.begin(),candidates.end(),[](const auto& a, const auto& b) {
                return std::tie(a.region,a.a,a.b) < std::tie(b.region,b.a,b.b);
            });
            candidates.erase(std::unique(candidates.begin(),candidates.end(),[](const auto& a, const auto& b) {
                return a.region == b.region && a.a == b.a && a.b == b.b;
            }),candidates.end());
            const auto cut = candidates[static_cast<std::size_t>(rng.uniform(candidates.size()))];
            regions[cut.region] = cut.a; regions.push_back(cut.b);
        }
        std::sort(regions.begin(), regions.end());
        for (const auto region : regions) {
            std::uint32_t count = 0; std::uint64_t buildable = 0;
            for (const auto& building : buildings) if (building.district == district.id && inside(building.footprint, region)) ++count;
            for (const auto& parcel : parcels) if (parcel.buildable && parcel.district == district.id)
                if (const auto part = intersection(parcel.boundary, region)) buildable += static_cast<std::uint64_t>(area(*part));
            result.territories.push_back({TerritoryId{result.territories.size()+1}, district.id, region, count, buildable});
        }
    }
    return result;
}
}
