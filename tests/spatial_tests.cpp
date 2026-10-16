#include "doctest.h"
#include "world/spatial/streaming.hpp"
#include "simulation/runtime/game_state.hpp"
#include <limits>
using namespace faultline;
using namespace faultline::world;

TEST_CASE("cell normalization handles negative coordinates and multiple boundaries") {
    const auto p = normalize({{2, -1}, {-257, 3, 256}});
    CHECK(p.cell == CellCoord{-1, 1}); CHECK(p.local == LocalPosition{127, 3, 0});
    const auto negative = fromGlobal({-0.5, 7, -128});
    CHECK(negative.cell == CellCoord{-1, -1}); CHECK(negative.local.x == 127.5F);
    const auto global = toGlobal(negative); CHECK(global.x == -0.5); CHECK(global.z == -128);
    CHECK(fromGlobal({128, 0, 128}, 64).cell == CellCoord{2, 2});
    CHECK_THROWS((void)normalize({}, 0));
    CHECK_THROWS((void)normalize({{std::numeric_limits<std::int32_t>::max(), 0}, {128, 0, 0}}));
    CHECK_THROWS((void)fromGlobal({std::numeric_limits<double>::infinity(), 0, 0}));
}
TEST_CASE("radius queries cross cell borders and use physical height") {
    SpatialGrid grid; std::map<EntityId, WorldPosition> positions{
        {EntityId{3}, fromGlobal({128, 0, 0})}, {EntityId{1}, fromGlobal({127, 0, 0})},
        {EntityId{2}, fromGlobal({127, 100, 0})}};
    for (const auto& [id, p] : positions) grid.upsert(id, p);
    const auto resolve = [&](EntityId id) { return positions.at(id); };
    CHECK(grid.queryRadius(fromGlobal({127, 0, 0}), 1, resolve) == std::vector<EntityId>{EntityId{1}, EntityId{3}});
    positions.at(EntityId{3}) = fromGlobal({-500, 0, 0}); grid.upsert(EntityId{3}, positions.at(EntityId{3}));
    CHECK(grid.queryRadius(fromGlobal({127, 0, 0}), 1, resolve) == std::vector<EntityId>{EntityId{1}});
    grid.remove(EntityId{1}); CHECK(grid.queryRadius(fromGlobal({127, 0, 0}), 1, resolve).empty());
    CHECK_THROWS((void)grid.queryRadius({}, -1, resolve));
}
TEST_CASE("streaming keeps persistent NPC identity position health and hysteresis") {
    WorldState world; WorldStreamingSystem streaming;
    const auto npc = world.npcs.create(); world.npcs.get(npc).position = fromGlobal({3000, 0, 0});
    streaming.commit(world, {}); CHECK_FALSE(static_cast<bool>(world.npcs.get(npc).activeEntity));
    streaming.commit(world, fromGlobal({3000, 0, 0}));
    const auto first = world.npcs.get(npc).activeEntity; REQUIRE(static_cast<bool>(first));
    world.transforms.get(first).position = fromGlobal({3010, 0, 0}); world.health.get(first).current = 67;
    streaming.commit(world, fromGlobal({3590, 0, 0})); CHECK(world.npcs.get(npc).activeEntity == first);
    streaming.commit(world, {}); CHECK_FALSE(world.entities.exists(first));
    CHECK(world.npcs.get(npc).health.current == 67);
    CHECK(toGlobal(world.npcs.get(npc).position).x == 3010);
    streaming.commit(world, fromGlobal({3000, 0, 0}));
    const auto second = world.npcs.get(npc).activeEntity;
    CHECK(second != first); CHECK(world.npcLinks.get(second).npc == npc);
    CHECK(world.health.get(second).current == 67); CHECK_NOTHROW(validateWorld(world));
    world.npcs.get(npc).lifecycle = NpcLifecycle::Dead;
    streaming.commit(world, {}); streaming.commit(world, fromGlobal({3000, 0, 0}));
    CHECK_FALSE(static_cast<bool>(world.npcs.get(npc).activeEntity)); CHECK(world.npcs.exists(npc));
}
TEST_CASE("spatial cache rebuild has no effect on state hash") {
    GameState state{WorldSeed{42}}; const auto npc = state.world.npcs.create();
    (void)materializeNpc(state.world, npc); const auto before = stateHash(state);
    state.world.spatial.clear(); rebuildSpatialIndex(state.world);
    CHECK(stateHash(state) == before);
    WorldStreamingSystem system; CHECK(system.desiredLevel(false, 513) == SimulationLevel::Abstract);
    CHECK(system.desiredLevel(true, 513) == SimulationLevel::Active);
    CHECK_THROWS(WorldStreamingSystem(StreamingConfig{512, 511}));
}
TEST_CASE("spatial radius queries agree with a brute force oracle") {
    core::DeterministicRng rng{12345}; SpatialGrid grid{64};
    std::map<EntityId, WorldPosition> positions;
    for (std::uint64_t i = 1; i <= 200; ++i) {
        const auto position = fromGlobal({static_cast<double>(rng.uniform(2000)) - 1000,
            static_cast<double>(rng.uniform(100)), static_cast<double>(rng.uniform(2000)) - 1000}, 64);
        positions.emplace(EntityId{i}, position); grid.upsert(EntityId{i}, position);
    }
    for (int i = 0; i < 50; ++i) {
        const auto center = fromGlobal({static_cast<double>(rng.uniform(2000)) - 1000, 0,
            static_cast<double>(rng.uniform(2000)) - 1000}, 64);
        const double radius = static_cast<double>(rng.uniform(500));
        std::vector<EntityId> expected;
        for (const auto& [id, position] : positions)
            if (distanceSquared(position, center, 64) <= radius * radius) expected.push_back(id);
        CHECK(grid.queryRadius(center, radius, [&](EntityId id) { return positions.at(id); }) == expected);
    }
}
