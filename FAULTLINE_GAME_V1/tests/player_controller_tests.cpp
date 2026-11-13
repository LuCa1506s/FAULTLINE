#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "simulation/prologue/prologue_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include "world/spatial/streaming.hpp"
#include "world/spatial/character_movement.hpp"
#include "simulation/player/player_script.hpp"
#include "save_compatibility.hpp"
#include <algorithm>
#include <set>
#include <limits>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

namespace {
GameState freshWorld(std::uint64_t seed = 12345) {
    world::WorldGenerationConfig c; c.width = 512; c.depth = 512; c.waterPercent = 0;
    return GameState{WorldSeed{seed}, c};
}
void startPhysicalSession(GameState& state) {
    runPrologue(state,FactionId{1});
    const auto leader=state.world.factions.get(state.prologue.origin).leader;
    createPlayerEntity(state,state.world.npcs.get(leader).position);
}

}

TEST_CASE("PlayerState defaults to version 1 with null entity") {
    GameState state = freshWorld();
    CHECK(state.player.version == currentPlayerStateVersion);
    CHECK_FALSE(state.player.controlledEntity);
    CHECK(state.player.yaw == 0.0F);
}

TEST_CASE("createPlayerEntity creates body with Transform, Health, CharacterBody") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({100, 0, 200});
    const auto entity = createPlayerEntity(state, spawn);
    REQUIRE(static_cast<bool>(entity));
    CHECK(state.player.controlledEntity == entity);
    CHECK(state.world.entities.exists(entity));
    CHECK(state.world.transforms.contains(entity));
    CHECK(state.world.health.contains(entity));
    CHECK(state.world.characterBodies.contains(entity));
    CHECK(state.world.transforms.get(entity).position == spawn);
    CHECK(state.world.health.get(entity).current == 100);
    CHECK(state.world.health.get(entity).maximum == 100);
    CHECK(state.world.characterBodies.get(entity).movementSpeed == playerMovementSpeedMetersPerSec);
    CHECK(state.world.characterBodies.get(entity).stanceHeight == playerStanceHeightMeters);
    CHECK(state.player.spawnPosition == spawn);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("createPlayerEntity rejects duplicate") {
    GameState state = freshWorld();
    createPlayerEntity(state, world::fromGlobal({0, 0, 0}));
    CHECK_THROWS_AS(createPlayerEntity(state, world::fromGlobal({0, 0, 0})), std::logic_error);
}

TEST_CASE("player movement north increments position by tick distance") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    Simulation sim;
    const auto entity = state.player.controlledEntity;
    const auto before = state.world.transforms.get(entity).position;
    sim.tick(state, {state.clock.tick(), {0, 1, 0, 0, false, false, false, false}, {}});
    CHECK(state.world.transforms.get(entity).position.local.x == before.local.x);
    CHECK(state.world.transforms.get(entity).position.local.z == doctest::Approx(before.local.z + playerTickMovementMeters));
    CHECK(state.player.yaw == doctest::Approx(std::atan2(0.0F, 1.0F)));
}

TEST_CASE("player movement with zero input does not move") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    Simulation sim;
    const auto entity = state.player.controlledEntity;
    const auto before = state.world.transforms.get(entity).position;
    sim.tick(state, {state.clock.tick(), {}, {}});
    CHECK(state.world.transforms.get(entity).position == before);
    CHECK(state.player.yaw == 0.0F);
}

TEST_CASE("diagonal movement is normalized to same speed as cardinal") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    Simulation sim;
    const auto entity = state.player.controlledEntity;
    const auto before = state.world.transforms.get(entity).position;
    sim.tick(state, {state.clock.tick(), {1, 1, 0, 0, false, false, false, false}, {}});
    const auto after = state.world.transforms.get(entity).position;
    const auto dx = after.local.x - before.local.x;
    const auto dz = after.local.z - before.local.z;
    CHECK(dx == doctest::Approx(dz));
    const auto diagDist = std::sqrt(dx * dx + dz * dz);
    CHECK(diagDist == doctest::Approx(playerTickMovementMeters));
    CHECK(diagDist != doctest::Approx(2 * playerTickMovementMeters));
}

TEST_CASE("player movement east-west does not change z coordinate") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    Simulation sim;
    const auto entity = state.player.controlledEntity;
    const auto before = state.world.transforms.get(entity).position;
    sim.tick(state, {state.clock.tick(), {1, 0, 0, 0, false, false, false, false}, {}});
    const auto after = state.world.transforms.get(entity).position;
    CHECK(after.local.z == doctest::Approx(before.local.z));
    CHECK(after.local.x == doctest::Approx(before.local.x + playerTickMovementMeters));
}

TEST_CASE("player yaw updates from aim vector") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    Simulation sim;
    CHECK(state.player.yaw == 0.0F);
    sim.tick(state, {state.clock.tick(), {0, 0, 1, 0, false, false, false, false}, {}});
    CHECK(state.player.yaw == doctest::Approx(std::atan2(1.0, 0.0)));
    sim.tick(state, {state.clock.tick(), {0, 0, 0, 1, false, false, false, false}, {}});
    CHECK(state.player.yaw == doctest::Approx(std::atan2(0.0, 1.0)));
    sim.tick(state, {state.clock.tick(), {0, 0, -1, 0, false, false, false, false}, {}});
    CHECK(state.player.yaw == doctest::Approx(std::atan2(-1.0, 0.0)));
}

TEST_CASE("player movement crosses cell boundary with normalization") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({127, 0, -200});
    createPlayerEntity(state, spawn);
    Simulation sim;
    const auto entity = state.player.controlledEntity;
    for (int i = 0; i < 20; ++i)
        sim.tick(state, {state.clock.tick(), {1, 0, 0, 0, false, false, false, false}, {}});
    const auto pos = state.world.transforms.get(entity).position;
    const bool crossedBoundary = pos.cell.x == 1;
    CHECK(crossedBoundary);    CHECK(world::normalize(pos, state.world.spatial.cellSize()) == pos);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("headless tick without player entity advances clock deterministically") {
    GameState a{WorldSeed{12345}}, b{WorldSeed{12345}};
    Simulation sim;
    sim.tick(a, {a.clock.tick(), {}, {}});
    sim.tick(b, {b.clock.tick(), {}, {}});
    CHECK(stateHash(a) == stateHash(b));
    CHECK_FALSE(a.player.controlledEntity);
}

TEST_CASE("player save/load round-trip preserves position, health, and yaw") {
    GameState state = freshWorld();
    createPlayerEntity(state, world::fromGlobal({1000, 0, 2000}));
    state.player.yaw = 1.5F;
    state.world.health.get(state.player.controlledEntity).current = 75;
    state.world.characterBodies.get(state.player.controlledEntity).yaw = 1.5F;
    const auto before = stateHash(state);
    const auto bytes = saveGameState(state);
    CHECK(bytes[16] == currentSaveFormatVersion.value);
    CHECK(bytes[16] == currentSaveFormatVersion.value);
    auto restored = loadGameState(bytes);
    CHECK(stateHash(restored) == before);
    CHECK(saveGameState(restored) == bytes);
    CHECK(restored.player.controlledEntity == state.player.controlledEntity);
    CHECK(restored.player.yaw == doctest::Approx(1.5F));
    CHECK(restored.world.health.get(restored.player.controlledEntity).current == 75);
    CHECK(restored.world.characterBodies.get(restored.player.controlledEntity).yaw == doctest::Approx(1.5F));
}

TEST_CASE("v9 fixture loads and migrates to current state with default PlayerState") {
    const auto path = std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v9_phase15.flsave";
    GameState state = loadGameStateFile(path);
    CHECK(state.metadata.generatorVersion.value == 3);
    CHECK(state.metadata.seed == WorldSeed{12345});
    CHECK(state.player.version == currentPlayerStateVersion);
    CHECK_FALSE(state.player.controlledEntity);
    CHECK(state.player.yaw == 0.0F);
    CHECK(state.world.characterBodies.entries().empty());
    const auto beforeHash = stateHash(state);
    const auto bytes = saveGameState(state);
    CHECK(bytes[16] == currentSaveFormatVersion.value);
    CHECK(bytes[16] == currentSaveFormatVersion.value);
    auto reloaded = loadGameState(bytes);
    CHECK(stateHash(reloaded) == beforeHash);
    CHECK(saveGameState(reloaded) == bytes);
}

TEST_CASE("player movement is deterministic across save/load") {
    GameState state = freshWorld();
    createPlayerEntity(state, state.world.npcs.entries().begin()->second.position);
    Simulation sim;
    for (int i = 0; i < 10; ++i)
        sim.tick(state, {state.clock.tick(), {0.7F, 0, 0.7F, 0, false, false, false, false}, {}});
    const auto before = stateHash(state);
    const auto bytes = saveGameState(state);
    auto restored = loadGameState(bytes);
    CHECK(stateHash(restored) == before);
    CHECK(saveGameState(restored) == bytes);
    REQUIRE_FALSE(state.player.residents.empty());
    Simulation resumed;
    for (int i=0;i<1700;++i) {
        const PlayerInputFrame input{0.7F,0,0.7F,0};
        resumed.tick(restored,{restored.clock.tick(),input,{}});
        sim.tick(state,{state.clock.tick(),input,{}});
        CAPTURE(state.clock.tick());
        REQUIRE(stateHash(state)==stateHash(restored));
        REQUIRE(state.world.entities.nextId()==restored.world.entities.nextId());
        REQUIRE(state.world.entities.entries()==restored.world.entities.entries());
        for (std::size_t domain=0;domain<state.randomness.streams.size();++domain)
            REQUIRE(state.randomness.streams[domain].state()==restored.randomness.streams[domain].state());
    }
    CHECK(stateHash(state)==stateHash(restored));
    CHECK(saveGameState(state)==saveGameState(restored));
}

TEST_CASE("physical session after prologue creates player entity at origin faction leader position") {
    GameState state = freshWorld();
    startPhysicalSession(state);
    CHECK(state.prologue.phase == ProloguePhase::Complete);
    REQUIRE(static_cast<bool>(state.player.controlledEntity));
    CHECK(state.world.entities.exists(state.player.controlledEntity));
    CHECK(state.world.transforms.contains(state.player.controlledEntity));
    CHECK(state.world.health.contains(state.player.controlledEntity));
    CHECK(state.world.characterBodies.contains(state.player.controlledEntity));
    const auto& leader = state.world.npcs.get(state.world.factions.get(state.prologue.origin).leader);
    CHECK(state.world.transforms.get(state.player.controlledEntity).position == leader.position);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("physical session after prologue then save/load preserves player entity") {
    GameState state = freshWorld();
    startPhysicalSession(state);
    const auto entity = state.player.controlledEntity;
    const auto hash = stateHash(state);
    const auto bytes = saveGameState(state);
    auto restored = loadGameState(bytes);
    CHECK(stateHash(restored) == hash);
    CHECK(saveGameState(restored) == bytes);
    CHECK(restored.player.controlledEntity == entity);
    CHECK(restored.world.entities.exists(entity));
    CHECK_NOTHROW(validateState(restored));
}

TEST_CASE("player entity survives persistent snapshot") {
    GameState state = freshWorld();
    createPlayerEntity(state, world::fromGlobal({300, 0, 400}));
    state.world.health.get(state.player.controlledEntity).current = 50;
    const auto snapshot = world::persistentSnapshot(state.world);
    CHECK(snapshot.entities.exists(state.player.controlledEntity));
    CHECK(snapshot.transforms.contains(state.player.controlledEntity));
    CHECK(snapshot.health.contains(state.player.controlledEntity));
    CHECK(snapshot.characterBodies.contains(state.player.controlledEntity));
    CHECK(snapshot.npcLinks.entries().empty());
}

TEST_CASE("destroyEntity removes CharacterBody from player entity") {
    GameState state = freshWorld();
    const auto entity = createPlayerEntity(state, world::fromGlobal({0, 0, 0}));
    CHECK(state.world.characterBodies.contains(entity));
    CHECK(state.world.transforms.contains(entity));
    world::destroyEntity(state.world, entity);
    CHECK_FALSE(state.world.characterBodies.contains(entity));
    CHECK_FALSE(state.world.transforms.contains(entity));
    CHECK_FALSE(state.world.entities.exists(entity));
}

TEST_CASE("multiple player ticks accumulate position deterministically") {
    GameState a = freshWorld();
    GameState b = freshWorld();
    createPlayerEntity(a, world::fromGlobal({500, 0, 500}));
    createPlayerEntity(b, world::fromGlobal({500, 0, 500}));
    Simulation sim;
    PlayerInputFrame input{};
    input.moveX = 0.6F; input.moveZ = 0.8F;
    for (int i = 0; i < 50; ++i) {
        sim.tick(a, {a.clock.tick(), input, {}});
        sim.tick(b, {b.clock.tick(), input, {}});
    }
    CHECK(stateHash(a) == stateHash(b));
    CHECK(stateHash(a) != stateHash(freshWorld()));
    const auto posA = a.world.transforms.get(a.player.controlledEntity).position;
    const auto posB = b.world.transforms.get(b.player.controlledEntity).position;
    CHECK(posA == posB);
}

TEST_CASE("player yaw persists through streaming materialization cycle") {
    GameState state = freshWorld();
    const auto spawn = world::fromGlobal({500, 0, 500});
    createPlayerEntity(state, spawn);
    state.player.yaw = 1.0F;
    state.world.characterBodies.get(state.player.controlledEntity).yaw = 1.0F;
    const auto snapshot = world::persistentSnapshot(state.world);
    CHECK(snapshot.entities.exists(state.player.controlledEntity));
    CHECK(snapshot.characterBodies.get(state.player.controlledEntity).yaw == doctest::Approx(1.0F));
}

TEST_CASE("input validation rejects non-finite player input") {
    GameState state = freshWorld();
    createPlayerEntity(state, world::fromGlobal({0, 0, 0}));
    Simulation sim;
    CHECK_THROWS_AS(sim.tick(state, {state.clock.tick(), {std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, false, false, false, false}, {}}), std::logic_error);
}

TEST_CASE("player state hash is sensitive to yaw and position") {
    GameState a = freshWorld();
    GameState b = freshWorld();
    createPlayerEntity(a, world::fromGlobal({100, 0, 200}));
    createPlayerEntity(b, world::fromGlobal({100, 0, 200}));
    CHECK(stateHash(a) == stateHash(b));
    a.player.yaw = 2.0F; b.player.yaw = 2.0F;
    CHECK(stateHash(a) == stateHash(b));
    a.player.yaw = 1.0F;
    CHECK(stateHash(a) != stateHash(b));
}

TEST_CASE("Game::tick applies player movement and preserves determinism") {
    GameState state = freshWorld();
    startPhysicalSession(state);
    Game gameA{state}, gameB{GameState{state}};
    PlayerInputFrame input{};
    input.moveX = 1.0F; input.moveZ = 0.0F;
    for (int i = 0; i < 50; ++i) {
        gameA.submit(input); gameA.tick();
        gameB.submit(input); gameB.tick();
    }
    CHECK(stateHash(gameA.state()) == stateHash(gameB.state()));
    const auto posA = gameA.state().world.transforms.get(gameA.state().player.controlledEntity).position;
    const auto posB = gameB.state().world.transforms.get(gameB.state().player.controlledEntity).position;
    CHECK(posA == posB);
    const bool movedA = (posA.cell.x != 0) || (posA.cell.z != 0);
    CHECK(movedA);
}


TEST_CASE("player entity has valid CharacterBody after prologue") {
    GameState state = freshWorld();
    startPhysicalSession(state);
    const auto entity = state.player.controlledEntity;
    REQUIRE(static_cast<bool>(entity));
    const auto& body = state.world.characterBodies.get(entity);
    CHECK(std::isfinite(body.yaw));
    CHECK(body.movementSpeed == playerMovementSpeedMetersPerSec);
    CHECK(body.stanceHeight == playerStanceHeightMeters);
    CHECK(body.yaw == doctest::Approx(state.player.yaw));
}

TEST_CASE("cross-process prologue with player state is deterministic") {
    GameState a = freshWorld(12345);
    startPhysicalSession(a);
    const auto bytes = saveGameState(a);
    const auto hash = stateHash(a);
    GameState b = loadGameState(bytes);
    CHECK(stateHash(b) == hash);
    CHECK(saveGameState(b) == bytes);
    CHECK(b.player.controlledEntity == a.player.controlledEntity);
}

TEST_CASE("physical timestep is 50ms not accelerated game time") {
    CHECK(playerTickMovementMeters==doctest::Approx(0.25));
    auto s=freshWorld();const auto e=createPlayerEntity(s,world::fromGlobal({500,0,500}));Simulation sim;
    PlayerInputFrame input;input.moveX=0.5F;
    const auto before=world::toGlobal(s.world.transforms.get(e).position);
    sim.tick(s,{s.clock.tick(),input,{}});
    CHECK(world::toGlobal(s.world.transforms.get(e).position).x-before.x==doctest::Approx(0.125));
}
TEST_CASE("signed diagonal cell crossings and long script remain normalized") {
    auto a=freshWorld();const auto e=createPlayerEntity(a,world::fromGlobal({-0.05,0,-0.05}));
    Simulation sim;
    PlayerInputFrame input; input.moveX=1;input.moveZ=1;
    sim.tick(a,{a.clock.tick(),input,{}});
    CHECK(a.world.transforms.get(e).position.cell==world::CellCoord{0,0});
    input.moveX=-1;input.moveZ=-1;
    sim.tick(a,{a.clock.tick(),input,{}});
    CHECK(a.world.transforms.get(e).position.cell==world::CellCoord{-1,-1});
    for (std::uint64_t t=0;t<16000;++t) {
        sim.tick(a,{a.clock.tick(),playerVerificationInput(t),{}});
        REQUIRE(world::normalize(a.world.transforms.get(e).position)==a.world.transforms.get(e).position);
        REQUIRE(a.world.entities.exists(e));
    }
    CHECK(a.world.characterBodies.entries().size()==1);
    auto b=loadGameState(saveGameState(a));CHECK(stateHash(a)==stateHash(b));
    CHECK_NOTHROW(validateState(a));
}
TEST_CASE("kinematic swept footprint collision slides and prevents tunneling") {
    world::WorldState w;w.base=world::GeneratedWorld{};
    w.base->bounds={-100,-100,200,200};
    world::BuildingState b; b.footprint={10,10,10,10};w.base->buildings.push_back(b);
    const auto move=[&](double x,double z,float dx,float dz) {
        return world::toGlobal(world::moveCharacter(w,world::fromGlobal({x,0,z}),dx,dz));
    };
    CHECK(move(0,15,50,0).x==doctest::Approx(10-world::characterCollisionRadius));
    CHECK(move(30,15,-50,0).x==doctest::Approx(20+world::characterCollisionRadius));
    const auto slide=move(9.5,12,1,1);
    CHECK(slide.x==doctest::Approx(10-world::characterCollisionRadius));CHECK(slide.z==doctest::Approx(13));
    CHECK(move(15,15,20,0).x==doctest::Approx(35)); // Existing interior actor may exit.
    CHECK(move(99,0,100,0).x==100);
    w.base->water.push_back({WaterRegionId{1},{-20,-20,10,10}});
    CHECK(move(-30,-15,50,0).x==doctest::Approx(-20-world::characterCollisionRadius));
    world::WorldState empty;
    const auto distant=world::moveCharacter(empty,world::fromGlobal({0,0,0}),512.5F,-512.5F);
    CHECK(distant.cell==world::CellCoord{4,-5});CHECK(world::normalize(distant)==distant);
}
TEST_CASE("interaction input emits scoped intent and never completes missions implicitly") {
    auto s=freshWorld();createPlayerEntity(s,world::fromGlobal({0,0,0}));Simulation sim;
    PlayerInputFrame input;input.interactPressed=true;
    sim.tick(s,{s.clock.tick(),input,{}});
    REQUIRE(sim.playerIntents().values().size()==1);
    CHECK(sim.playerIntents().values()[0].actor==s.player.controlledEntity);
    CHECK(sim.playerIntents().values()[0].tick==0);
    CHECK(s.missionSimulation.instances.entries().empty());
    sim.tick(s,{s.clock.tick(),{}, {}});CHECK(sim.playerIntents().empty());
}
TEST_CASE("authentic v10 prologue migrates with no invented player or body") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v10_phase16.flsave";
    std::ifstream file{path,std::ios::binary};REQUIRE(file.is_open());
    const SaveBytes old{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    REQUIRE(readSaveHeader(old).saveFormatVersion.value==10);
    auto s=loadGameState(old);CHECK(s.prologue.phase==ProloguePhase::Complete);
    CHECK(s.history.months==48);CHECK(s.story.version==1);
    CHECK(s.player==PlayerState{});CHECK(s.world.characterBodies.entries().empty());
    const auto bytes=saveGameState(s);CHECK(test::preservesHistoricalPayloads(old,bytes));
    auto b=loadGameState(bytes);CHECK(stateHash(s)==stateHash(b));CHECK(saveGameState(b)==bytes);
}
TEST_CASE("player malformed state and residency are rejected without allocator repair") {
    auto s=freshWorld();createPlayerEntity(s,s.world.npcs.entries().begin()->second.position);Simulation sim;
    sim.tick(s,{s.clock.tick(),{}, {}});REQUIRE_FALSE(s.player.residents.empty());
    for (int fault=0;fault<6;++fault) {
        auto bad=s;
        if (fault==0) bad.player.yaw=1;
        if (fault==1) bad.player.residents.push_back(bad.player.residents.front());
        if (fault==2) bad.player.residents.front().npc=NpcId{999999};
        if (fault==3) bad.player.residents.front().entity=bad.player.controlledEntity;
        if (fault==4) bad.player.version=999;
        if (fault==5) bad.world.characterBodies.get(bad.player.controlledEntity).movementSpeed=0;
        CHECK_THROWS_AS((void)saveGameState(bad),SaveError);
    }
    const auto bytes=saveGameState(s);
    std::size_t start=48;
    while (start<bytes.size()) {
        const auto u32=[&](std::size_t p){return std::uint32_t(bytes[p])|(std::uint32_t(bytes[p+1])<<8)|(std::uint32_t(bytes[p+2])<<16)|(std::uint32_t(bytes[p+3])<<24);};
        if (u32(start)==19) { start+=8;break; } start+=8+u32(start+4);
    }
    REQUIRE(start<bytes.size());
    for (auto n=start;n<bytes.size();++n) CHECK_THROWS_AS((void)loadGameState(std::span{bytes}.first(n)),SaveError);
    auto corrupt=bytes;corrupt[start]=255;CHECK_THROWS_AS((void)loadGameState(corrupt),SaveError);
}

TEST_CASE("streaming hysteresis leases reconstruct the same IDs without allocating") {
    auto a=freshWorld();const auto npc=a.world.npcs.entries().begin()->first;
    const auto origin=a.world.npcs.get(npc).position;
    const auto player=createPlayerEntity(a,origin);Simulation sim;
    sim.tick(a,{a.clock.tick(),{}, {}});
    const auto body=a.world.npcs.get(npc).activeEntity;REQUIRE(body);
    auto outsideActivation=origin;outsideActivation.local.x+=550;
    a.world.transforms.get(player).position=world::normalize(outsideActivation);
    sim.tick(a,{a.clock.tick(),{}, {}});REQUIRE(a.world.npcs.get(npc).activeEntity==body);
    const auto bytes=saveGameState(a);auto b=loadGameState(bytes);
    CHECK_FALSE(b.world.npcs.get(npc).activeEntity);
    const auto highWater=b.world.entities.nextId();
    restorePlayerResidency(b);
    CHECK(b.world.entities.nextId()==highWater);CHECK(b.world.npcs.get(npc).activeEntity==body);
    CHECK(stateHash(a)==stateHash(b));CHECK(saveGameState(b)==bytes);
    Simulation resumed;sim.tick(a,{a.clock.tick(),{}, {}});resumed.tick(b,{b.clock.tick(),{}, {}});
    CHECK(stateHash(a)==stateHash(b));CHECK(saveGameState(a)==saveGameState(b));
}
TEST_CASE("aim wins over movement and heading remains in signed pi interval") {
    auto s=freshWorld();const auto e=createPlayerEntity(s,world::fromGlobal({0,0,0}));Simulation sim;
    for (const auto& direction : std::vector<std::pair<float,float>>{{1,0},{-1,0},{0,1},{0,-1},{-1,-1}}) {
        PlayerInputFrame input;input.moveX=1;input.aimX=direction.first;input.aimZ=direction.second;
        sim.tick(s,{s.clock.tick(),input,{}});
        CHECK(s.player.yaw==doctest::Approx(std::atan2(input.aimX,input.aimZ)));
        CHECK(s.player.yaw>=-3.14159265F);CHECK(s.player.yaw<=3.14159265F);
        CHECK(s.player.yaw==s.world.characterBodies.get(e).yaw);
    }
}
