#include "doctest.h"
#include "world/state/world_state.hpp"
#include "simulation/runtime/game_state.hpp"
using namespace faultline;
using namespace faultline::world;

TEST_CASE("entity IDs never recycle and components require live entities") {
    EntityRegistry registry; ComponentStore<int> components;
    const auto first = registry.create(); components.set(registry, first, 42);
    CHECK(components.get(first) == 42); components.remove(first); registry.destroy(first);
    CHECK_FALSE(registry.exists(first)); CHECK(registry.create().value() > first.value());
    CHECK_THROWS(components.set(registry, first, 7)); CHECK_THROWS(registry.destroy(first));
}
TEST_CASE("domain stores allocate separately and validate semantic references") {
    WorldState world;
    const auto district = world.districts.create();
    const auto territory = world.territories.create({district});
    const auto faction = world.factions.create({FactionLifecycle::Active, {}, territory});
    const auto npc = world.npcs.create({NpcLifecycle::Alive, faction, {district,{},{}}, {}, {}, {}});
    world.factions.get(faction).leader = npc;
    const auto mission = world.missions.create({MissionResolution::Available, npc});
    CHECK(world.missions.get(mission).issuer == npc);
    CHECK_NOTHROW(validateWorld(world));
    world.npcs.get(npc).faction = FactionId{999}; CHECK_THROWS(validateWorld(world));
    world.npcs.get(npc).faction = faction;
    world.npcs.get(npc).lifecycle = NpcLifecycle::Dead; CHECK_THROWS(validateWorld(world));
    world.factions.get(faction).leader = {}; CHECK_NOTHROW(validateWorld(world));
    CHECK(world.npcs.exists(npc));
}
TEST_CASE("duplicate materialization is detected and destruction removes links") {
    WorldState world; const auto npc = world.npcs.create();
    const auto a = world.entities.create(), b = world.entities.create();
    world.npcs.get(npc).activeEntity = a; world.npcLinks.set(world.entities, a, {npc});
    world.transforms.set(world.entities, a, {}); world.health.set(world.entities, a, {});
    world.characterBodies.set(world.entities, a, {});
    CHECK_NOTHROW(validateWorld(world));
    world.npcLinks.set(world.entities, b, {npc}); CHECK_THROWS(validateWorld(world));
    world.npcLinks.remove(b); destroyEntity(world, a);
    CHECK_FALSE(static_cast<bool>(world.npcs.get(npc).activeEntity));
    CHECK_FALSE(world.npcLinks.contains(a)); CHECK_NOTHROW(validateWorld(world));
}
TEST_CASE("world hash includes semantic records and allocation high water marks") {
    GameState a{WorldSeed{42}}, b = a;
    const auto entity = b.world.entities.create(); destroyEntity(b.world, entity);
    CHECK(stateHash(a) != stateHash(b));
    b = a; const auto npc = b.world.npcs.create(); CHECK(stateHash(a) != stateHash(b));
    a = b; b.world.npcs.get(npc).lifecycle = NpcLifecycle::Dead; CHECK(stateHash(a) != stateHash(b));
}
