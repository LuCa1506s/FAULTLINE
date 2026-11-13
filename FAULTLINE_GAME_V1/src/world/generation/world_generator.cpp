#include "world/generation/world_generator.hpp"
#include "core/diagnostics/diagnostics.hpp"
#include "core/rng/deterministic_rng.hpp"
#include "world/generation/urban_generation.hpp"
#include "world/generation/actor_generation.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace faultline::world {
namespace {
std::vector<Rect> partition(Rect root, std::uint32_t count, core::DeterministicRng& rng) {
    std::vector<Rect> result{root};
    while (result.size() < count) {
        // Split the largest remaining region. Work is bounded by the configured count.
        auto chosen = std::max_element(result.begin(), result.end(), [](Rect a, Rect b) { return area(a) < area(b); });
        Rect a = *chosen, b = a;
        const bool splitX = a.width > a.depth || (a.width == a.depth && rng.chance(1, 2));
        const auto length = splitX ? a.width : a.depth;
        const auto cut = static_cast<std::int32_t>(static_cast<std::int64_t>(length) *
            static_cast<std::int64_t>(rng.uniform(35, 66)) / 100);
        core::require(cut > 0 && cut < length, "Partition has insufficient space");
        if (splitX) { a.width = cut; b.x += cut; b.width -= cut; }
        else { a.depth = cut; b.z += cut; b.depth -= cut; }
        *chosen = a; result.push_back(b);
    }
    std::sort(result.begin(), result.end()); return result;
}
std::pair<Rect, std::vector<WaterRegion>> generateGeography(const GenerationContext& ctx, Rect bounds) {
    core::DeterministicRng geography{ctx.stageSeed("geography")};
    core::DeterministicRng water{ctx.stageSeed("water")};
    Rect land = bounds;
    std::vector<WaterRegion> regions;
    if (water.chance(ctx.config.waterPercent, 100)) {
        const auto width = static_cast<std::int32_t>(static_cast<std::int64_t>(bounds.width) *
            static_cast<std::int64_t>(water.uniform(5, 16)) / 100);
        const bool west = geography.chance(1, 2);
        const Rect coast{west ? bounds.x : bounds.x + bounds.width - width, bounds.z, width, bounds.depth};
        land.width -= width; if (west) land.x += width;
        regions.push_back({WaterRegionId{1}, coast});
    }
    return {land, regions};
}
std::vector<MacroZone> generateZones(const GenerationContext& ctx, Rect land, const std::vector<WaterRegion>& water) {
    core::DeterministicRng rng{ctx.stageSeed("macro_zones")};
    const auto count = static_cast<std::uint32_t>(rng.uniform(ctx.config.minMacroZones,
        static_cast<std::uint64_t>(ctx.config.maxMacroZones) + 1));
    const auto regions = partition(land, count, rng);
    std::vector<MacroZone> zones;
    const std::array dry{ZoneArchetype::Industrial, ZoneArchetype::Park, ZoneArchetype::Financial,
        ZoneArchetype::Residential, ZoneArchetype::Suburbs, ZoneArchetype::Entertainment, ZoneArchetype::University};
    std::size_t central = 0; std::int64_t closest = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const auto r = regions[i];
        const auto dx = static_cast<std::int64_t>(r.x) * 2 + r.width - (static_cast<std::int64_t>(land.x) * 2 + land.width);
        const auto dz = static_cast<std::int64_t>(r.z) * 2 + r.depth - (static_cast<std::int64_t>(land.z) * 2 + land.depth);
        const auto score = dx * dx + dz * dz;
        if (score < closest) { closest = score; central = i; }
        auto type = dry[static_cast<std::size_t>(rng.uniform(dry.size()))];
        if (!water.empty() && adjacent(r, water.front().boundary) && rng.chance(1, 2))
            type = rng.chance(1, 2) ? ZoneArchetype::Harbor : ZoneArchetype::Waterfront;
        zones.push_back({MacroZoneId{i + 1}, r, type});
    }
    zones[central].archetype = ZoneArchetype::Downtown;
    return zones;
}
RoadGraph generateRoads(const GenerationContext& ctx, Rect land, const std::vector<MacroZone>& zones) {
    core::DeterministicRng rng{ctx.stageSeed("roads")};
    const auto cx = land.x + land.width / 2, cz = land.z + land.depth / 2;
    std::set<std::int32_t> xs{land.x, land.x + land.width, cx}, zs{land.z, land.z + land.depth, cz};
    std::set<std::int32_t> zoneXs, zoneZs;
    for (const auto& zone : zones) {
        const auto r = zone.boundary;
        zoneXs.insert(r.x); zoneXs.insert(r.x + r.width);
        zoneZs.insert(r.z); zoneZs.insert(r.z + r.depth);
        xs.insert(r.x); xs.insert(r.x + r.width); zs.insert(r.z); zs.insert(r.z + r.depth);
        xs.insert(r.x + static_cast<std::int32_t>(static_cast<std::int64_t>(r.width) * static_cast<std::int64_t>(rng.uniform(40, 61)) / 100));
        zs.insert(r.z + static_cast<std::int32_t>(static_cast<std::int64_t>(r.depth) * static_cast<std::int64_t>(rng.uniform(40, 61)) / 100));
    }
    RoadGraph graph;
    std::map<std::pair<std::int32_t, std::int32_t>, RoadNodeId> ids;
    for (auto x : xs) for (auto z : zs) {
        const RoadNodeId id{graph.nodes.size() + 1}; graph.nodes.push_back({id, x, z}); ids.emplace(std::pair{x,z}, id);
    }
    const auto add = [&](std::int32_t x, std::int32_t z, std::int32_t nx, std::int32_t nz, RoadCategory category) {
        graph.edges.push_back({RoadEdgeId{graph.edges.size() + 1}, ids.at({x,z}), ids.at({nx,nz}), category});
    };
    // Every crossing is a node, so no unconnected geometric intersections exist.
    for (auto x = xs.begin(); x != xs.end(); ++x) for (auto z = zs.begin(); z != zs.end(); ++z) {
        if (std::next(x) != xs.end()) add(*x, *z, *std::next(x), *z,
            *z == cz ? RoadCategory::Arterial : (zoneZs.contains(*z) ? RoadCategory::Major : RoadCategory::Local));
        if (std::next(z) != zs.end()) add(*x, *z, *x, *std::next(z),
            *x == cx ? RoadCategory::Arterial : (zoneXs.contains(*x) ? RoadCategory::Major : RoadCategory::Local));
    }
    return graph;
}
std::vector<GeneratedDistrict> generateDistricts(const GenerationContext& ctx, const std::vector<MacroZone>& zones) {
    std::vector<GeneratedDistrict> districts;
    for (const auto& zone : zones) {
        core::DeterministicRng rng{core::deriveSeed(ctx.stageSeed("districts"), "macro_zone", zone.id)};
        const auto count = static_cast<std::uint32_t>(rng.uniform(1, 4));
        for (const auto r : partition(zone.boundary, count, rng)) {
            const auto density = static_cast<std::uint32_t>(rng.uniform(20, 101));
            const auto wealth = static_cast<std::uint32_t>(rng.uniform(10, 101));
            const auto population = static_cast<std::uint64_t>(area(r)) * density / 20000 + 1;
            districts.push_back({DistrictId{districts.size() + 1}, zone.id, r, zone.archetype, population, density, wealth});
        }
    }
    return districts;
}
}
void validateConfig(const WorldGenerationConfig& config) {
    core::require(config.width >= 512 && config.depth >= 512 && config.width <= 1000000 && config.depth <= 1000000,
                  "World dimensions must be 512..1000000 meters (M1 resource limits)");
    core::require(config.minMacroZones >= 5 && config.maxMacroZones >= config.minMacroZones && config.maxMacroZones <= 10,
                  "M1 macro-zone range must be within 5..10");
    core::require(config.waterPercent <= 100, "Water probability outside 0..100");
    core::require(static_cast<std::int64_t>(config.originX) + config.width <= std::numeric_limits<std::int32_t>::max() &&
                  static_cast<std::int64_t>(config.originZ) + config.depth <= std::numeric_limits<std::int32_t>::max(), "World coordinate overflow");
}
GeneratedWorld generateWorld(const GenerationContext& ctx) {
    validateConfig(ctx.config); core::require(ctx.version.value >= 1 && ctx.version.value <= 3, "Unsupported world generator version");
    if (ctx.version.value >= 2) validateUrbanConfig(ctx.config.urban);
    // Version 2 adds urban stages while deliberately retaining v1 geography/topology.
    // This explicit compatibility mapping prevents building rules from moving roads.
    auto topology = ctx; topology.version = {1};
    GeneratedWorld result; result.metadata = {ctx.seed, ctx.version}; result.config = ctx.config;
    result.bounds = {ctx.config.originX, ctx.config.originZ, ctx.config.width, ctx.config.depth};
    auto [land, water] = generateGeography(topology, result.bounds); result.buildableLand = land; result.water = std::move(water);
    result.macroZones = generateZones(topology, land, result.water);
    for (std::size_t i = 0; i < result.macroZones.size(); ++i) for (std::size_t j = i+1; j < result.macroZones.size(); ++j)
        if (adjacent(result.macroZones[i].boundary, result.macroZones[j].boundary))
            result.adjacency.push_back({result.macroZones[i].id, result.macroZones[j].id});
    result.roads = generateRoads(topology, land, result.macroZones);
    result.districts = generateDistricts(topology, result.macroZones);
    if (ctx.version.value >= 2) {
        auto urbanContext=ctx; urbanContext.version={2};
        result.parcels = generateParcels(urbanContext, result).parcels;
        result.buildings = generateBuildings(urbanContext, result.districts, result.parcels).buildings;
        result.territories = generateTerritories(urbanContext, result, result.parcels, result.buildings).territories;
    }
    if (ctx.version.value == 3) {
        auto factions=generateFactions(ctx,result);
        result.factions=std::move(factions.factions); result.territoryAssignments=std::move(factions.assignments);
        result.majorNpcs=generateMajorNpcs(ctx,result,result.factions).npcs;
        for (const auto& npc : result.majorNpcs) if (npc.state.roles.has(NpcRole::FactionLeader)) {
            const auto faction=std::find_if(result.factions.begin(),result.factions.end(),[&](const auto& f){return f.id==npc.state.faction;});
            faction->state.leader=npc.id;
        }
        auto relationships=generateInitialRelationships(ctx,result,result.factions,result.territoryAssignments,result.majorNpcs);
        result.factionRelationships=std::move(relationships.factions); result.npcRelationships=std::move(relationships.npcs);
    }
    const auto report = validateGeneratedWorld(result);
    if (!report.passed()) throw std::runtime_error("Worldgen seed=" + std::to_string(ctx.seed.value) + " version=" +
        std::to_string(ctx.version.value) + ": " + report.errors.front());
    return result;
}

ValidationReport validateGeneratedWorld(const GeneratedWorld& world) {
    ValidationReport report;
    const auto check = [&](bool condition, const char* message) { if (!condition) report.errors.emplace_back(message); };
    try { validateConfig(world.config); } catch (const std::exception& e) { report.errors.push_back(e.what()); return report; }
    check(world.metadata.generatorVersion.value >= 1 && world.metadata.generatorVersion.value <= 3, "Unsupported generator version");
    check(world.bounds == Rect{world.config.originX, world.config.originZ, world.config.width, world.config.depth}, "Bounds/config mismatch");
    check(inside(world.buildableLand, world.bounds), "Invalid buildable land");
    if (!report.passed()) return report;
    std::int64_t covered = area(world.buildableLand);
    std::set<WaterRegionId> waterIds;
    for (std::size_t i = 0; i < world.water.size(); ++i) {
        const auto& water = world.water[i];
        check(static_cast<bool>(water.id) && waterIds.insert(water.id).second, "Duplicate/null water ID");
        check(inside(water.boundary, world.bounds) && !overlap(water.boundary, world.buildableLand), "Invalid water geometry");
        if (!report.passed()) return report;
        covered += area(water.boundary);
        for (std::size_t j = i+1; j < world.water.size(); ++j) check(!overlap(water.boundary, world.water[j].boundary), "Overlapping water");
    }
    check(covered == area(world.bounds), "Geography coverage mismatch");
    check(area(world.buildableLand) >= area(world.bounds) * 80 / 100, "Insufficient buildable land");
    check(world.macroZones.size() >= world.config.minMacroZones && world.macroZones.size() <= world.config.maxMacroZones, "Invalid macro-zone count");
    std::map<MacroZoneId, Rect> zones;
    std::int64_t zoneArea = 0;
    std::set<ZoneAdjacency> expectedAdjacency;
    for (std::size_t i = 0; i < world.macroZones.size(); ++i) {
        const auto& zone = world.macroZones[i];
        check(static_cast<bool>(zone.id) && zones.emplace(zone.id, zone.boundary).second, "Duplicate/null macro-zone ID");
        check(inside(zone.boundary, world.buildableLand), "Macro-zone outside land");
        if (!report.passed()) return report;
        zoneArea += area(zone.boundary);
        check(zone.archetype <= ZoneArchetype::Waterfront, "Invalid zone archetype");
        if (zone.archetype == ZoneArchetype::Harbor || zone.archetype == ZoneArchetype::Waterfront)
            check(std::any_of(world.water.begin(), world.water.end(), [&](const auto& water) { return adjacent(zone.boundary, water.boundary); }), "Water-dependent zone lacks water access");
        for (std::size_t j = i+1; j < world.macroZones.size(); ++j) {
            const auto& other = world.macroZones[j]; check(!overlap(zone.boundary, other.boundary), "Overlapping macro-zones");
            if (adjacent(zone.boundary, other.boundary)) expectedAdjacency.insert({std::min(zone.id, other.id), std::max(zone.id, other.id)});
        }
    }
    check(zoneArea == area(world.buildableLand), "Macro-zones do not cover land");
    const std::set<ZoneAdjacency> supplied(world.adjacency.begin(), world.adjacency.end());
    check(supplied == expectedAdjacency && supplied.size() == world.adjacency.size(), "Invalid macro-zone adjacency graph");
    std::map<RoadNodeId, RoadNode> nodes;
    std::set<std::pair<std::int32_t, std::int32_t>> positions;
    for (const auto& node : world.roads.nodes) {
        check(static_cast<bool>(node.id) && nodes.emplace(node.id, node).second, "Duplicate/null road node ID");
        check(positions.insert({node.x, node.z}).second, "Coincident road nodes");
        check(contains(world.buildableLand, node.x, node.z), "Road node outside land");
    }
    check(!nodes.empty(), "Missing road network");
    std::map<RoadNodeId, std::vector<RoadNodeId>> neighbors;
    struct Segment { RoadNode a, b; };
    std::vector<Segment> horizontal, vertical;
    std::set<RoadEdgeId> edgeIds;
    std::set<std::pair<RoadNodeId, RoadNodeId>> connections;
    for (const auto& edge : world.roads.edges) {
        check(static_cast<bool>(edge.id) && edgeIds.insert(edge.id).second, "Duplicate/null road edge ID");
        check(edge.category <= RoadCategory::Service, "Invalid road category");
        check(edge.a != edge.b && connections.insert({std::min(edge.a, edge.b), std::max(edge.a, edge.b)}).second, "Duplicate/zero-length edge");
        if (!nodes.contains(edge.a) || !nodes.contains(edge.b)) { check(false, "Dangling road edge"); continue; }
        const auto a = nodes.at(edge.a), b = nodes.at(edge.b);
        check((a.x == b.x) != (a.z == b.z), "M1 road must be nonzero and axis-aligned");
        if (a.z == b.z && a.x != b.x) horizontal.push_back({a, b});
        if (a.x == b.x && a.z != b.z) vertical.push_back({a, b});
        // An intermediate node would mean an unsplit intersection or collinear overlap.
        for (const auto& node : world.roads.nodes) if (node.id != a.id && node.id != b.id) {
            const bool between = a.x == b.x ? node.x == a.x && node.z > std::min(a.z,b.z) && node.z < std::max(a.z,b.z)
                : node.z == a.z && node.x > std::min(a.x,b.x) && node.x < std::max(a.x,b.x);
            check(!between, "Unsplit road intersection");
        }
        neighbors[a.id].push_back(b.id); neighbors[b.id].push_back(a.id);
    }
    for (const auto& h : horizontal) for (const auto& v : vertical) {
        const bool crossing = v.a.x > std::min(h.a.x, h.b.x) && v.a.x < std::max(h.a.x, h.b.x) &&
            h.a.z > std::min(v.a.z, v.b.z) && h.a.z < std::max(v.a.z, v.b.z);
        check(!crossing, "Road crossing lacks intersection node");
    }
    std::set<RoadNodeId> visited; std::vector<RoadNodeId> pending;
    if (!nodes.empty()) { pending.push_back(nodes.begin()->first); visited.insert(nodes.begin()->first); }
    while (!pending.empty()) {
        const auto id = pending.back(); pending.pop_back();
        for (const auto next : neighbors[id]) if (visited.insert(next).second) pending.push_back(next);
    }
    check(visited.size() == nodes.size(), "Disconnected road network");
    std::set<DistrictId> districtIds; std::map<MacroZoneId, std::int64_t> districtArea;
    for (std::size_t i = 0; i < world.districts.size(); ++i) {
        const auto& district = world.districts[i];
        check(static_cast<bool>(district.id) && districtIds.insert(district.id).second, "Duplicate/null district ID");
        const auto zone = zones.find(district.macroZone);
        check(zone != zones.end() && inside(district.boundary, zone->second), "Invalid district parent/bounds");
        if (!report.passed()) return report;
        districtArea[district.macroZone] += area(district.boundary);
        check(district.population > 0 && district.density >= 20 && district.density <= 100 && district.wealth >= 10 && district.wealth <= 100,
              "Invalid district baseline");
        check(district.archetype <= ZoneArchetype::Waterfront, "Invalid district archetype");
        bool access = false;
        for (const auto& edge : world.roads.edges) if (nodes.contains(edge.a) && nodes.contains(edge.b)) {
            const auto a = nodes.at(edge.a), b = nodes.at(edge.b); const auto r = district.boundary;
            if (a.x == b.x) access |= a.x >= r.x && a.x <= right(r) && std::max(a.z,b.z) >= r.z && std::min(a.z,b.z) <= bottom(r);
            else access |= a.z >= r.z && a.z <= bottom(r) && std::max(a.x,b.x) >= r.x && std::min(a.x,b.x) <= right(r);
        }
        check(access, "Unreachable district");
        for (std::size_t j = i+1; j < world.districts.size(); ++j)
            check(!overlap(district.boundary, world.districts[j].boundary), "Overlapping districts");
    }
    for (const auto& [id, boundary] : zones) check(districtArea[id] == area(boundary), "Districts do not cover macro-zone");
    if (world.metadata.generatorVersion.value == 1)
        check(world.parcels.empty() && world.buildings.empty() && world.territories.empty(), "M1 cannot contain M2 records");
    if (world.metadata.generatorVersion.value < 3)
        check(world.factions.empty() && world.majorNpcs.empty() && world.territoryAssignments.empty() && world.factionRelationships.empty() && world.npcRelationships.empty(),
              "Legacy generator cannot contain M3 records");
    if (report.passed() && world.metadata.generatorVersion.value >= 2) {
        const auto urban = validateUrbanWorld(world);
        report.errors.insert(report.errors.end(), urban.errors.begin(), urban.errors.end());
    }
    if (report.passed() && world.metadata.generatorVersion.value == 3) {
        const auto actors=validateActors(world); report.errors.insert(report.errors.end(),actors.errors.begin(),actors.errors.end());
    }
    return report;
}
std::uint64_t baseWorldHash(const GeneratedWorld& world) {
    core::StateHash hash;
    hash.string(world.metadata.generatorVersion.value == 1 ? "FAULTLINE.base.M1.v1" : world.metadata.generatorVersion.value == 2 ? "FAULTLINE.base.M2.v2" : "FAULTLINE.base.M3.v3");
    const auto number = [&](auto value) { hash.u64(static_cast<std::uint64_t>(value)); };
    const auto rect = [&](Rect r) { number(r.x); number(r.z); number(r.width); number(r.depth); };
    number(world.metadata.seed.value); number(world.metadata.generatorVersion.value);
    const auto& c = world.config; number(c.width); number(c.depth); number(c.originX); number(c.originZ);
    number(c.minMacroZones); number(c.maxMacroZones); number(c.waterPercent);
    rect(world.bounds); rect(world.buildableLand);
    number(world.water.size()); for (const auto& w : world.water) { number(w.id.value()); rect(w.boundary); }
    number(world.macroZones.size()); for (const auto& z : world.macroZones) { number(z.id.value()); rect(z.boundary); number(z.archetype); }
    number(world.adjacency.size()); for (auto edge : world.adjacency) { number(edge.a.value()); number(edge.b.value()); }
    number(world.roads.nodes.size()); for (const auto& n : world.roads.nodes) { number(n.id.value()); number(n.x); number(n.z); }
    number(world.roads.edges.size()); for (auto e : world.roads.edges) { number(e.id.value()); number(e.a.value()); number(e.b.value()); number(e.category); }
    number(world.districts.size()); for (const auto& d : world.districts) {
        number(d.id.value()); number(d.macroZone.value()); rect(d.boundary); number(d.archetype);
        number(d.population); number(d.density); number(d.wealth);
    }
    if (world.metadata.generatorVersion.value >= 2) appendUrbanHash(hash, world);
    if (world.metadata.generatorVersion.value == 3) appendActorsHash(hash, world);
    return hash.value();
}
}
