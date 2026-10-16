#include "doctest.h"
#include "world/generation/world_generator.hpp"
#include "core/rng/deterministic_rng.hpp"
#include <algorithm>
using namespace faultline;
using namespace faultline::world;

TEST_CASE("worldgen M1 reproduces complete canonical world") {
    const GenerationContext context{{12345}, {1}, {}};
    const auto a = generateWorld(context), b = generateWorld(context);
    CHECK(baseWorldHash(a) == 0x2938f0ca42162bc9ULL);
    CHECK(validateGeneratedWorld(a).passed()); CHECK(baseWorldHash(a) == baseWorldHash(b));
    CHECK(baseWorldHash(a) != baseWorldHash(generateWorld({{54321}, {1}, {}})));
    CHECK(a.macroZones.size() >= 5); CHECK(a.macroZones.size() <= 10);
}
TEST_CASE("worldgen config supports micro worlds nonzero origin water and dry land") {
    for (const auto dimensions : {512, 8000, 16000}) {
        WorldGenerationConfig config; config.width = dimensions; config.depth = dimensions;
        config.originX = -100; config.originZ = 200;
        for (const auto water : {0U, 100U}) {
            config.waterPercent = water;
            const auto world = generateWorld({{42}, {1}, config}); CHECK(validateGeneratedWorld(world).passed());
            CHECK(world.water.size() == (water == 0 ? 0 : 1));
            CHECK(world.bounds.x == -100); CHECK(world.bounds.z == 200);
        }
    }
    CHECK_THROWS((void)generateWorld({{1}, {4}, {}}));
    WorldGenerationConfig bad; bad.width = 0; CHECK_THROWS((void)generateWorld({{1}, {1}, bad}));
}
TEST_CASE("validator rejects corrupted topology geometry IDs and reachability") {
    const auto base = generateWorld({{12345}, {1}, {}});
    auto bad = base; bad.roads.edges[0].a = RoadNodeId{999999}; CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.roads.edges.clear(); CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.macroZones[1].id = bad.macroZones[0].id; CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.districts[0].boundary = bad.districts[1].boundary; CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.districts[0].macroZone = MacroZoneId{999}; CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.adjacency.clear(); CHECK_FALSE(validateGeneratedWorld(bad).passed());
    bad = base; bad.buildableLand.width = -1; CHECK_FALSE(validateGeneratedWorld(bad).passed());
}
TEST_CASE("unrelated npc stream consumption leaves generated roads unchanged") {
    const GenerationContext context{{12345}, {1}, {}};
    const auto before = baseWorldHash(generateWorld(context));
    core::DeterministicRng npc{context.stageSeed("npc")};
    for (int i = 0; i < 10000; ++i) (void)npc.nextU64();
    CHECK(baseWorldHash(generateWorld(context)) == before);
    CHECK(context.stageSeed("roads") != context.stageSeed("npc"));
}
TEST_CASE("validator detects roads that cross without a graph intersection") {
    auto world = generateWorld({{12345}, {1}, {}});
    const auto x = world.buildableLand.x + world.buildableLand.width / 2;
    const auto z = world.buildableLand.z + world.buildableLand.depth / 2;
    world.roads.nodes = {{RoadNodeId{1}, x-10, z}, {RoadNodeId{2}, x+10, z},
        {RoadNodeId{3}, x, z-10}, {RoadNodeId{4}, x, z+10},
        {RoadNodeId{5}, x-10, z-10}, {RoadNodeId{6}, x+10, z+10}};
    world.roads.edges = {{RoadEdgeId{1}, RoadNodeId{1}, RoadNodeId{2}, RoadCategory::Local},
        {RoadEdgeId{2}, RoadNodeId{3}, RoadNodeId{4}, RoadCategory::Local},
        {RoadEdgeId{3}, RoadNodeId{1}, RoadNodeId{5}, RoadCategory::Local},
        {RoadEdgeId{4}, RoadNodeId{5}, RoadNodeId{3}, RoadCategory::Local},
        {RoadEdgeId{5}, RoadNodeId{2}, RoadNodeId{6}, RoadCategory::Local},
        {RoadEdgeId{6}, RoadNodeId{6}, RoadNodeId{4}, RoadCategory::Local}};
    const auto report = validateGeneratedWorld(world);
    CHECK(std::find(report.errors.begin(), report.errors.end(), "Road crossing lacks intersection node") != report.errors.end());
}
