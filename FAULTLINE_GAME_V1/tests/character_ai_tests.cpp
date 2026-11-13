#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/npc/character_ai.hpp"
#include "simulation/npc/npc_system.hpp"
#include "persistence/save.hpp"
#include "world/spatial/character_movement.hpp"
#include <cmath>
#include <set>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

namespace {
GameState small(std::uint64_t seed = 12345) {
    world::WorldGenerationConfig config;
    config.width = 512; config.depth = 512; config.waterPercent = 0;
    auto state = GameState{WorldSeed{seed}, config};
    state.factionSimulation.nextEvaluationTick = neverTick;
    state.marketSimulation.nextUpdateTick = neverTick;
    return state;
}

NpcId activateFirst(GameState& state, Simulation& sim) {
    const auto npc = state.world.npcs.entries().begin()->first;
    createPlayerEntity(state, state.world.npcs.get(npc).position);
    sim.tick(state, {state.clock.tick(), {}, {}});
    REQUIRE(state.world.npcs.get(npc).activeEntity);
    return npc;
}

world::WorldLocation territoryTarget(const GameState& state, NpcId npc) {
    const auto current = state.world.npcs.get(npc).location.territory;
    for (const auto& territory : state.world.base->territories)
        if (territory.id != current) return {territory.district, territory.id, {}};
    return {state.world.npcs.get(npc).location.district, current, {}};
}

void makeFlee(GameState& state, NpcId npc) {
    auto& value = state.world.npcs.get(npc);
    value.semantic.activity = world::NpcActivity::Hiding;
    value.semantic.home = territoryTarget(state, npc);
}

void setPlayerPosition(GameState& state, world::WorldPosition position) {
    auto& transform = state.world.transforms.get(state.player.controlledEntity);
    transform.position = world::normalize(position, state.world.spatial.cellSize());
    state.world.spatial.upsert(state.player.controlledEntity, transform.position);
}
}

TEST_CASE("materialized NPC has one linked physical character body") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim);
    const auto entity = state.world.npcs.get(npc).activeEntity;
    CHECK(state.world.entities.exists(entity));
    CHECK(state.world.npcLinks.get(entity).npc == npc);
    CHECK(state.world.transforms.contains(entity));
    CHECK(state.world.health.contains(entity));
    CHECK(state.world.characterBodies.contains(entity));
    CHECK(state.world.characterBodies.get(entity).movementSpeed == npcMovementSpeedMetersPerSec);
    CHECK(state.world.characterBodies.get(entity).stanceHeight == npcStanceHeightMeters);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("abstract NPC receives no physical update and idle active NPC is stable") {
    auto state = small(); CharacterAiEventBuffer events;
    const auto abstractNpc = state.world.npcs.entries().begin()->first;
    updateCharacterAi(state, events);
    CHECK(events.empty());
    CHECK_FALSE(state.world.npcs.get(abstractNpc).activeEntity);
    Simulation sim; const auto npc = activateFirst(state, sim);
    auto& value = state.world.npcs.get(npc); value.semantic.activity = world::NpcActivity::Idle;
    const auto entity = value.activeEntity;
    const auto before = state.world.transforms.get(entity).position;
    updateCharacterAi(state, events);
    CHECK(state.world.transforms.get(entity).position == before);
    CHECK(state.world.characterBodies.get(entity).yaw == 0.0F);
}

TEST_CASE("flee movement uses exactly one 50ms normalized physical step") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim);
    state.world.base->buildings.clear(); state.world.base->water.clear();
    makeFlee(state, npc);
    const auto entity = state.world.npcs.get(npc).activeEntity;
    const auto target = decideCharacterIntent(state, npc).target;
    const auto targetGlobal = world::toGlobal(target);
    state.world.transforms.get(entity).position = world::fromGlobal({targetGlobal.x - 10, 0, targetGlobal.z - 10});
    state.world.spatial.upsert(entity, state.world.transforms.get(entity).position);
    const auto before = world::toGlobal(state.world.transforms.get(entity).position);
    CharacterAiEventBuffer events; updateCharacterAi(state, events);
    const auto after = world::toGlobal(state.world.transforms.get(entity).position);
    CHECK(std::hypot(after.x-before.x, after.z-before.z) ==
          doctest::Approx(npcMovementSpeedMetersPerSec * 0.05).epsilon(0.0001));
    CHECK(events.values().front().intent.behavior == CharacterAiBehavior::Flee);
    CHECK(events.values().front().moved);
    CHECK(state.world.characterBodies.get(entity).yaw == doctest::Approx(std::atan2(1.0, 1.0)));
}

TEST_CASE("working, follow and operation assignments map to bounded physical intents") {
    auto state = small(); Simulation sim; const auto observer = activateFirst(state, sim);
    auto& npc = state.world.npcs.get(observer);
    npc.semantic.activity = world::NpcActivity::Working;
    CHECK(decideCharacterIntent(state, observer).behavior == CharacterAiBehavior::MoveTo);

    NpcId leader;
    if (npc.faction) leader = state.world.factions.get(npc.faction).leader;
    if (leader && leader != observer && state.world.npcs.get(leader).activeEntity) {
        auto leaderPosition = state.world.transforms.get(state.world.npcs.get(observer).activeEntity).position;
        leaderPosition.local.x += 2.0F;
        state.world.transforms.get(state.world.npcs.get(leader).activeEntity).position = world::normalize(leaderPosition);
        state.world.spatial.upsert(state.world.npcs.get(leader).activeEntity, leaderPosition);
        npc.semantic.activity = world::NpcActivity::Meeting;
        const auto intent = decideCharacterIntent(state, observer);
        CHECK(intent.behavior == CharacterAiBehavior::Follow);
        CHECK(intent.targetNpc == leader);
    }

    const auto target = state.world.territories.entries().begin()->first;
    auto operation = FactionOperation{};
    operation.faction = npc.faction; operation.target = target; operation.status = OperationStatus::Active;
    operation.type = OperationType::AttackTerritory;
    npc.semantic.operation = state.factionSimulation.operations.create(operation);
    CHECK(decideCharacterIntent(state, observer).behavior == CharacterAiBehavior::AttackIntent);
    state.factionSimulation.operations.get(npc.semantic.operation).type = OperationType::DefendTerritory;
    CHECK(decideCharacterIntent(state, observer).behavior == CharacterAiBehavior::Protect);
}

TEST_CASE("local perception is radius bounded stable and excludes observer") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim);
    const auto perceived = perceiveCharacters(state, npc, 1000.0);
    CHECK(std::is_sorted(perceived.begin(), perceived.end()));
    CHECK(std::find(perceived.begin(), perceived.end(), state.world.npcs.get(npc).activeEntity) == perceived.end());
    CHECK(std::find(perceived.begin(), perceived.end(), state.player.controlledEntity) != perceived.end());
    auto playerPosition = state.world.transforms.get(state.player.controlledEntity).position;
    playerPosition.local.x += 1.0F;
    setPlayerPosition(state, playerPosition);
    CHECK(perceiveCharacters(state, npc, 0.01).empty());
}

TEST_CASE("Character AI never moves the player and updates residents in NpcId order") {
    auto state = small(); Simulation sim; (void)activateFirst(state, sim);
    const auto playerBefore = state.world.transforms.get(state.player.controlledEntity).position;
    for (const auto& resident : state.player.residents) makeFlee(state, resident.npc);
    CharacterAiEventBuffer events; updateCharacterAi(state, events);
    CHECK(state.world.transforms.get(state.player.controlledEntity).position == playerBefore);
    NpcId previous;
    for (const auto& event : events.values()) {
        CHECK(event.intent.npc > previous); previous = event.intent.npc;
        CHECK(event.intent.entity != state.player.controlledEntity);
    }
}

TEST_CASE("static collision clips NPC movement through building footprint") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim);
    REQUIRE_FALSE(state.world.base->buildings.empty());
    const auto building = state.world.base->buildings.front();
    auto& value = state.world.npcs.get(npc);
    value.location = {building.district, {}, building.id};
    value.semantic.activity = world::NpcActivity::Working;
    const auto entity = value.activeEntity;
    const double z = static_cast<double>(building.transform.centerZ2) / 2.0;
    state.world.transforms.get(entity).position = world::fromGlobal({building.footprint.x - 1.0, 0, z});
    state.world.characterBodies.get(entity).movementSpeed = 100.0F;
    state.world.spatial.upsert(entity, state.world.transforms.get(entity).position);
    CharacterAiEventBuffer events; updateCharacterAi(state, events);
    const auto after = world::toGlobal(state.world.transforms.get(entity).position);
    CHECK(after.x <= static_cast<double>(building.footprint.x) - world::characterCollisionRadius + 0.0001);
}

TEST_CASE("physical intent normalizes positive negative and multiple cell crossings") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim);
    const auto entity = state.world.npcs.get(npc).activeEntity;
    state.world.base.reset();
    state.world.characterBodies.get(entity).movementSpeed = 3000.0F; // 150 m fixed step.
    CharacterAiIntent intent{npc,entity,CharacterAiBehavior::MoveTo,true,world::fromGlobal({300,0,0}),{}};
    state.world.transforms.get(entity).position=world::fromGlobal({-0.05,0,0});
    auto event=executeCharacterIntent(state,intent);
    CHECK(event.moved); CHECK(state.world.transforms.get(entity).position.cell.x==1);
    intent.target=world::fromGlobal({-300,0,0});
    event=executeCharacterIntent(state,intent);
    CHECK(event.moved); CHECK(state.world.transforms.get(entity).position.cell.x==-1);
    for (int i=0;i<8;++i) (void)executeCharacterIntent(state,intent);
    CHECK(state.world.transforms.get(entity).position.cell.x<0);
    CHECK(world::normalize(state.world.transforms.get(entity).position)==state.world.transforms.get(entity).position);
}

TEST_CASE("active abstraction rematerialization preserves NpcId progress and leases") {
    auto state = small(); Simulation sim; const auto npc = activateFirst(state, sim); makeFlee(state, npc);
    for (int i=0;i<20;++i) sim.tick(state,{state.clock.tick(),{}, {}});
    const auto firstEntity = state.world.npcs.get(npc).activeEntity;
    const auto movedPosition = state.world.transforms.get(firstEntity).position;
    auto distant = movedPosition; distant.local.x += 800.0F; setPlayerPosition(state, distant);
    sim.tick(state,{state.clock.tick(),{}, {}});
    CHECK_FALSE(state.world.npcs.get(npc).activeEntity);
    CHECK(state.world.npcs.get(npc).position == movedPosition);
    CHECK_FALSE(state.world.entities.exists(firstEntity));
    const auto bytes = saveGameState(state); auto restored = loadGameState(bytes);
    setPlayerPosition(restored, restored.world.npcs.get(npc).position);
    Simulation resumed; resumed.tick(restored,{restored.clock.tick(),{}, {}});
    const auto secondEntity = restored.world.npcs.get(npc).activeEntity;
    CHECK(secondEntity); CHECK(secondEntity != firstEntity);
    CHECK(restored.world.npcLinks.get(secondEntity).npc == npc);
    CHECK(restored.world.characterBodies.contains(secondEntity));
    CHECK(std::count_if(restored.world.npcLinks.entries().begin(), restored.world.npcLinks.entries().end(),
        [&](const auto& entry){ return entry.second.npc == npc; }) == 1);
    CHECK_NOTHROW(validateState(restored));
}

TEST_CASE("save load continued Character AI is identical every tick") {
    auto original = small(); Simulation uninterrupted; const auto npc = activateFirst(original, uninterrupted);
    makeFlee(original, npc);
    for (int i=0;i<37;++i) uninterrupted.tick(original,{original.clock.tick(),{}, {}});
    const auto bytes = saveGameState(original); auto restored = loadGameState(bytes);
    CHECK(stateHash(original) == stateHash(restored));
    CHECK(saveGameState(restored) == bytes);
    Simulation resumed;
    for (int i=0;i<1200;++i) {
        uninterrupted.tick(original,{original.clock.tick(),{}, {}});
        resumed.tick(restored,{restored.clock.tick(),{}, {}});
        CAPTURE(i); CAPTURE(original.clock.tick());
        REQUIRE(stateHash(original) == stateHash(restored));
        REQUIRE(original.world.entities.nextId() == restored.world.entities.nextId());
        REQUIRE(original.player.residents == restored.player.residents);
        for (const auto& resident : original.player.residents) {
            const auto other = restored.world.npcs.get(resident.npc).activeEntity;
            REQUIRE(other == resident.entity);
            REQUIRE(original.world.transforms.get(resident.entity).position == restored.world.transforms.get(other).position);
            REQUIRE(original.world.characterBodies.get(resident.entity) == restored.world.characterBodies.get(other));
        }
    }
    CHECK(saveGameState(original) == saveGameState(restored));
}

TEST_CASE("long multi-NPC physical execution remains finite normalized and unique") {
    auto a = small(77), b = a; Simulation sa, sb;
    const auto focus = a.world.npcs.entries().begin()->second.position;
    createPlayerEntity(a, focus); createPlayerEntity(b, focus);
    for (int tick=0;tick<5000;++tick) {
        sa.tick(a,{a.clock.tick(),{}, {}}); sb.tick(b,{b.clock.tick(),{}, {}});
        if (tick==0) {
            for (const auto& resident : a.player.residents) makeFlee(a,resident.npc);
            for (const auto& resident : b.player.residents) makeFlee(b,resident.npc);
        }
        CAPTURE(tick);
        REQUIRE(stateHash(a)==stateHash(b));
        std::set<EntityId> bodies;
        for (const auto& resident : a.player.residents) {
            REQUIRE(bodies.insert(resident.entity).second);
            const auto position=a.world.transforms.get(resident.entity).position;
            REQUIRE(world::normalize(position)==position);
            const auto global=world::toGlobal(position);
            REQUIRE(std::isfinite(global.x)); REQUIRE(std::isfinite(global.z));
        }
    }
    CHECK_NOTHROW(validateState(a));
}
