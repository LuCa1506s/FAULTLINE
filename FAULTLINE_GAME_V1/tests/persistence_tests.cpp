#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "persistence/schema_v1.hpp"
#include "simulation/runtime/game.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/market/market_system.hpp"
#include "simulation/missions/mission_system.hpp"
#include "world/spatial/streaming.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>

using namespace faultline;
using namespace faultline::persistence;
namespace {
GameState emptyState() {
    world::WorldState world;
    return {{WorldSeed{42},currentGeneratorVersion},core::SimulationClock{},RuntimeRandomState{WorldSeed{42}},std::move(world)};
}
void expectError(const SaveBytes& bytes, SaveErrorCode expected) {
    try { (void)loadGameState(bytes); FAIL("Malformed save accepted"); }
    catch (const SaveError& error) {
        CHECK(error.code==expected); CHECK_FALSE(error.section.empty()); CHECK_FALSE(std::string{error.what()}.empty());
        CHECK(error.offset<=bytes.size());
    }
}
std::uint32_t u32(const SaveBytes& bytes, std::size_t offset) {
    std::uint32_t v{}; for (std::size_t i=0;i<4;++i) v|=static_cast<std::uint32_t>(bytes.at(offset+i))<<(8*i); return v;
}
void setU32(SaveBytes& bytes, std::size_t offset, std::uint32_t v) {
    for (std::size_t i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::uint8_t>((v>>(8*i))&255U);
}
void setU64(SaveBytes& bytes, std::size_t offset, std::uint64_t v) {
    for (std::size_t i=0;i<8;++i) bytes.at(offset+i)=static_cast<std::uint8_t>((v>>(8*i))&255U);
}
std::size_t payload(const SaveBytes& bytes, std::uint32_t section) {
    std::size_t pos=48; // 16-byte magic + 28-byte header + u32 section count.
    while (pos<bytes.size()) {
        if (u32(bytes,pos)==section) return pos+8;
        pos+=8+u32(bytes,pos+4);
    }
    throw std::logic_error("Test section missing");
}
}
TEST_CASE("save header and little-endian wire vectors") {
    auto state=emptyState(); state.metadata.seed=WorldSeed{0x0102030405060708ULL};
    const auto bytes=saveGameState(state); const auto h=readSaveHeader(bytes);
    CHECK(h.saveFormatVersion==currentSaveFormatVersion); CHECK(h.gameVersion==currentGameVersion);
    CHECK(h.generatorVersion==currentGeneratorVersion); CHECK(h.worldSeed==state.metadata.seed);
    CHECK(std::equal(saveMagic.begin(),saveMagic.end(),bytes.begin()));
    CHECK(bytes[16]==currentSaveFormatVersion.value); CHECK(bytes[17]==0); CHECK(bytes[32]==3);
    for (std::size_t i=0;i<8;++i) CHECK(bytes[36+i]==8-i);
    persistence::detail::Writer w;
    w(std::int32_t{-2},std::uint16_t{0x1234},1.0F,std::string{"A"});
    CHECK(w.bytes==SaveBytes{0xFE,0xFF,0xFF,0xFF,0x34,0x12,0,0,0x80,0x3F,1,0,0,0,0x41});
}
TEST_CASE("full semantic world round-trip preserves all supported generator versions") {
    const std::array<std::uint64_t,3> expected{0x2938f0ca42162bc9ULL,0x5f3a446fe0d2f974ULL,0x888c5dbd10c15886ULL};
    for (std::uint32_t version=1;version<=3;++version) {
        CAPTURE(version);
        GameState original{WorldSeed{12345},{},GeneratorVersion{version}};
        for (int i=0;i<37;++i) original.clock.advance();
        const auto bytes=saveGameState(original); auto restored=loadGameState(bytes);
        CHECK_NOTHROW(validateState(restored)); CHECK(stateHash(original)==stateHash(restored));
        CHECK(saveGameState(restored)==bytes); CHECK(saveGameState(original)==bytes);
        CHECK(world::baseWorldHash(*original.world.base)==expected[version-1]);
        CHECK(world::baseWorldHash(*restored.world.base)==expected[version-1]);
        CHECK(restored.metadata.generatorVersion.value==version);
        CHECK(restored.clock.tick()==37); CHECK(restored.clock.gameTime()==original.clock.gameTime());
    }
}
TEST_CASE("active NPC syncs to abstract semantic snapshot and can materialize again") {
    GameState state{WorldSeed{12345}};
    const auto npc=state.world.npcs.entries().begin()->first;
    const auto body=world::materializeNpc(state.world,npc);
    const auto position=world::fromGlobal({17,3,-21});
    state.world.transforms.get(body).position=position; state.world.health.get(body).current=42;
    const auto before=stateHash(state); const auto bytes=saveGameState(state); auto restored=loadGameState(bytes);
    CHECK(state.world.npcs.get(npc).activeEntity==body); // Snapshot never mutates the running game.
    CHECK_FALSE(restored.world.npcs.get(npc).activeEntity);
    CHECK(restored.world.npcs.get(npc).position==position); CHECK(restored.world.npcs.get(npc).health.current==42);
    CHECK_FALSE(restored.world.entities.exists(body)); CHECK(restored.world.entities.nextId()==state.world.entities.nextId());
    CHECK(stateHash(restored)==before); CHECK(saveGameState(restored)==bytes);
    const auto newBody=world::materializeNpc(restored.world,npc);
    CHECK(newBody.value()>body.value()); CHECK(restored.world.transforms.get(newBody).position==position);
    CHECK(restored.world.health.get(newBody).current==42); CHECK(restored.world.npcs.get(npc).identity==state.world.npcs.get(npc).identity);
}
TEST_CASE("dead records mission status and existing vehicle components persist without caches") {
    auto state=emptyState(); auto& w=state.world;
    world::NpcState dead; dead.lifecycle=world::NpcLifecycle::Dead;
    dead.identity.name="Storico \xC3\xA8"; const auto npc=w.npcs.create(dead);
    const auto mission=w.missions.create({world::MissionResolution::ResolvedByNpc,npc});
    const auto vehicle=w.vehicles.create(); const auto body=w.entities.create();
    w.vehicles.get(vehicle).activeEntity=body;
    w.vehicleLinks.set(w.entities,body,{vehicle}); w.transforms.set(w.entities,body,{world::fromGlobal({-1,2,300},64)});
    w.health.set(w.entities,body,{12,80}); w.spatial=world::SpatialGrid{64};
    CHECK(w.spatial.queryCell({-1,4}).empty());
    const auto bytes=saveGameState(state); auto restored=loadGameState(bytes);
    CHECK(stateHash(state)==stateHash(restored)); CHECK(saveGameState(restored)==bytes);
    CHECK(restored.world.npcs.get(npc)==dead); CHECK(restored.world.missions.get(mission).status==world::MissionResolution::ResolvedByNpc);
    CHECK(restored.world.missions.get(mission).issuer==npc);
    CHECK(restored.world.spatial.queryCell({-1,4})==std::vector<EntityId>{body});
    CHECK(restored.world.health.get(body).current==12); CHECK(restored.world.vehicles.get(vehicle).activeEntity==body);
}
TEST_CASE("every runtime RNG stream preserves its exact future") {
    auto state=emptyState();
    for (std::size_t i=0;i<state.randomness.streams.size();++i)
        for (std::size_t j=0;j<17+i;++j) (void)state.randomness.streams[i].nextU64();
    auto restored=loadGameState(saveGameState(state));
    for (std::size_t i=0;i<state.randomness.streams.size();++i) {
        CHECK(state.randomness.streams[i].state()==restored.randomness.streams[i].state());
        for (int j=0;j<100;++j) CHECK(state.randomness.streams[i].nextU64()==restored.randomness.streams[i].nextU64());
    }
}
TEST_CASE("allocator high-water marks survive gaps and domain exhaustion") {
    auto state=emptyState();
    for (int i=0;i<27;++i) { const auto id=state.world.entities.create(); world::destroyEntity(state.world,id); }
    (void)state.world.npcs.create(); (void)state.world.vehicles.create(); (void)state.world.missions.create();
    const auto district=state.world.districts.create(); (void)state.world.territories.create({district});
    (void)state.world.factions.create();
    simulation::initializeTerritorySimulation(state); simulation::initializeMarketSimulation(state); auto b=loadGameState(saveGameState(state));
    CHECK(state.world.entities.create()==b.world.entities.create()); CHECK(b.world.entities.nextId()==29);
    CHECK(state.world.npcs.create()==b.world.npcs.create()); CHECK(state.world.vehicles.create()==b.world.vehicles.create());
    CHECK(state.world.missions.create()==b.world.missions.create()); CHECK(state.world.districts.create()==b.world.districts.create());
    CHECK(state.world.territories.create({district})==b.world.territories.create({district})); CHECK(state.world.factions.create()==b.world.factions.create());
    auto exhausted=emptyState();
    exhausted.world.entities=world::EntityRegistry::restore(0,{EntityId{std::numeric_limits<std::uint64_t>::max()}});
    auto restored=loadGameState(saveGameState(exhausted)); CHECK_THROWS(restored.world.entities.create());
    CHECK_THROWS(world::EntityRegistry::restore(3,{EntityId{3}}));
    CHECK_THROWS(world::NpcStore::restore(1,{{NpcId{1},{}}}));
}
TEST_CASE("continued headless ticks RNG and object creation match after load") {
    auto a=emptyState(); Simulation simulation;
    const auto step=[&](GameState& state) {
        simulation.tick(state,{state.clock.tick(),{},{}});
        for (auto& rng : state.randomness.streams) (void)rng.nextU64();
        const auto entity=state.world.entities.create();
        if (state.clock.tick()%3!=0) world::destroyEntity(state.world,entity);
        if (state.clock.tick()%17==0) (void)state.world.npcs.create();
    };
    for (int i=0;i<1000;++i) step(a);
    auto b=loadGameState(saveGameState(a));
    for (int i=0;i<1000;++i) { step(a); step(b); }
    CHECK(a.clock.tick()==2000); CHECK(stateHash(a)==stateHash(b)); CHECK(saveGameState(a)==saveGameState(b));
    Game gameA{a},gameB{loadGameState(saveGameState(b))};
    for (int i=0;i<100;++i) { gameA.tick(); gameB.tick(); }
    CHECK(stateHash(gameA.state())==stateHash(gameB.state()));
}
TEST_CASE("unordered semantic relationships serialize in canonical order") {
    GameState state{WorldSeed{12345}}; auto reverse=state;
    std::reverse(reverse.world.factionRelationships.begin(),reverse.world.factionRelationships.end());
    std::reverse(reverse.world.npcRelationships.begin(),reverse.world.npcRelationships.end());
    CHECK(stateHash(state)==stateHash(reverse)); CHECK(saveGameState(state)==saveGameState(reverse));
    auto signedZero=emptyState(); const auto id=signedZero.world.entities.create();
    signedZero.world.transforms.set(signedZero.world.entities,id,{{{0,0},{0,-0.0F,0}}});
    auto positive=signedZero; positive.world.transforms.get(id).position.local.y=0.0F;
    CHECK(saveGameState(positive)==saveGameState(signedZero));
}
TEST_CASE("invalid magic unsupported versions and trailing sections fail explicitly") {
    const auto valid=saveGameState(emptyState()); auto bytes=valid;
    bytes[0]='?'; expectError(bytes,SaveErrorCode::InvalidMagic);
    bytes=valid; setU32(bytes,16,999); expectError(bytes,SaveErrorCode::UnsupportedSaveVersion);
    bytes=valid; setU32(bytes,32,999); expectError(bytes,SaveErrorCode::UnsupportedGeneratorVersion);
    bytes=valid; setU32(bytes,20,999); expectError(bytes,SaveErrorCode::UnsupportedGameVersion);
    bytes=valid; setU32(bytes,44,0); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; setU32(bytes,48,999); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; bytes.push_back(0); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; setU32(bytes,52,std::numeric_limits<std::uint32_t>::max()); expectError(bytes,SaveErrorCode::TruncatedData);
}
TEST_CASE("every truncated prefix fails cleanly including nested world data") {
    const auto valid=saveGameState(emptyState());
    for (std::size_t size=0;size<valid.size();++size) {
        CAPTURE(size); expectError(SaveBytes(valid.begin(),valid.begin()+static_cast<std::ptrdiff_t>(size)),SaveErrorCode::TruncatedData);
    }
    GameState state{WorldSeed{12345}}; const auto full=saveGameState(state);
    for (std::size_t size=1000;size<full.size();size+=7919)
        CHECK_THROWS_AS((void)loadGameState(std::span{full}.first(size)),SaveError);
}
TEST_CASE("invalid lengths clocks RNG and references never silently repair") {
    auto state=emptyState(); (void)state.world.npcs.create();
    const auto valid=saveGameState(state); auto bytes=valid;
    setU32(bytes,payload(bytes,7)+8,std::numeric_limits<std::uint32_t>::max()); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; setU64(bytes,payload(bytes,1)+8,1); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; setU64(bytes,payload(bytes,1),std::numeric_limits<std::uint64_t>::max()); expectError(bytes,SaveErrorCode::ValidationFailure);
    bytes=valid; const auto rng=payload(bytes,1)+24; std::fill_n(bytes.begin()+static_cast<std::ptrdiff_t>(rng),32,0);
    expectError(bytes,SaveErrorCode::ValidationFailure);
    bytes=valid; setU64(bytes,payload(bytes,7)+8+4+8+1,999); expectError(bytes,SaveErrorCode::ValidationFailure); // NPC faction
    bytes=valid; setU64(bytes,payload(bytes,7),1); expectError(bytes,SaveErrorCode::ValidationFailure); // nextNpcId collides
    bytes=valid; setU64(bytes,payload(bytes,7)+12,0); expectError(bytes,SaveErrorCode::InvalidData);
    auto invalid=state; invalid.world.npcs.get(NpcId{1}).faction=FactionId{999};
    CHECK_THROWS_AS((void)saveGameState(invalid),SaveError);
}
TEST_CASE("strict UTF-8 finite floats and string length limits") {
    auto state=emptyState(); world::NpcState npc; npc.identity.name="A"; const auto id=state.world.npcs.create(npc);
    const auto valid=saveGameState(state);
    const auto record=payload(valid,7)+12+8;
    // NPC prefix: lifecycle 1, faction 8, location 24, activeEntity 8,
    // position 20, health 8, then identity.name length 4.
    const auto nameLength=record+69;
    CHECK(u32(valid,nameLength)==1);
    auto bytes=valid; setU32(bytes,nameLength,0xFFFFFFFFU); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; bytes[nameLength+4]=0xC0; expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; setU32(bytes,record+41+8,0x7FC00000U); expectError(bytes,SaveErrorCode::InvalidData);
    state.world.npcs.get(id).identity.name=std::string(65537,'x'); CHECK_THROWS_AS((void)saveGameState(state),SaveError);
    state.world.npcs.get(id).identity.name="\xED\xA0\x80"; CHECK_THROWS_AS((void)saveGameState(state),SaveError);
}
TEST_CASE("loader rejects broken semantic references duplicate records and orphan components") {
    auto state=emptyState();
    const auto district=state.world.districts.create(); const auto territory=state.world.territories.create({district});
    const auto faction=state.world.factions.create(); world::NpcState n; n.faction=faction;
    const auto first=state.world.npcs.create(n); (void)state.world.npcs.create(n);
    state.world.factions.get(faction).leader=first; state.world.factions.get(faction).homeTerritory=territory;
    (void)state.world.missions.create({world::MissionResolution::Available,first});
    const auto entity=state.world.entities.create(); state.world.transforms.set(state.world.entities,entity,{});
    simulation::initializeTerritorySimulation(state); simulation::initializeMarketSimulation(state);
    const auto valid=saveGameState(state); auto bytes=valid;
    setU64(bytes,payload(bytes,6)+12+8+1,999); expectError(bytes,SaveErrorCode::ValidationFailure); // leader
    bytes=valid; setU64(bytes,payload(bytes,5)+12+8+8,999); expectError(bytes,SaveErrorCode::ValidationFailure); // owner
    bytes=valid; setU64(bytes,payload(bytes,9)+12+8+1,999); expectError(bytes,SaveErrorCode::ValidationFailure); // issuer
    bytes=valid; setU64(bytes,payload(bytes,4)+12+8,999); expectError(bytes,SaveErrorCode::ValidationFailure); // macro-zone
    bytes=valid; setU64(bytes,payload(bytes,3)+12+8+4,999); expectError(bytes,SaveErrorCode::ValidationFailure); // transform entity
    // Determine the second record boundary by decoding the first explicit record.
    persistence::detail::Reader r{valid}; r.offset=payload(valid,7)+12;
    NpcId id{}; world::NpcState record; r(id,record);
    bytes=valid; setU64(bytes,r.offset,first.value()); expectError(bytes,SaveErrorCode::InvalidData);
    bytes=valid; bytes[payload(bytes,7)+12+8]=255; expectError(bytes,SaveErrorCode::ValidationFailure); // lifecycle
}
TEST_CASE("configured decoder budgets reject huge declarations before allocation") {
    const auto valid=saveGameState(emptyState()); auto bytes=valid;
    setU32(bytes,payload(bytes,3)+8,100001); expectError(bytes,SaveErrorCode::InvalidData);
    persistence::detail::Writer w; w(std::uint32_t{100000});
    persistence::detail::Reader r{w.bytes};
    std::vector<world::NpcState> records;
    CHECK_THROWS_AS(r(records),SaveError); CHECK(records.empty());
    w.bytes.clear(); w(std::uint32_t{2}); w(false,false);
    persistence::detail::Reader budget{w.bytes}; budget.objectsLeft=1;
    std::vector<std::uint8_t> v; CHECK_THROWS_AS(budget(v),SaveError); CHECK(v.empty());
    persistence::detail::Writer encoder; encoder.objects=300000;
    CHECK_THROWS_AS(encoder(std::vector<std::uint8_t>{1}),SaveError);
}
TEST_CASE("file persistence has explicit IO failures and cleans test artifacts") {
    const auto directory=std::filesystem::path{FAULTLINE_TEST_BINARY_DIR}/"save-test-artifacts";
    REQUIRE_FALSE(std::filesystem::exists(directory)); REQUIRE(std::filesystem::create_directory(directory));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path/"roundtrip.flsave",error);
            std::filesystem::remove(path,error);
        }
    } cleanup{directory};
    const auto path=directory/"roundtrip.flsave"; const auto state=emptyState();
    saveGameStateFile(state,path); auto restored=loadGameStateFile(path);
    CHECK(stateHash(state)==stateHash(restored)); CHECK(std::filesystem::file_size(path)==saveGameState(state).size());
    CHECK_THROWS_AS((void)loadGameStateFile(directory/"missing.flsave"),SaveError);
    CHECK_THROWS_AS(saveGameStateFile(state,directory/"missing"/"save.flsave"),SaveError);
}
TEST_CASE("v8 fixture loads and migrates to current state with later systems initialized") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v8_phase14.flsave";
    GameState state=loadGameStateFile(path);
    CHECK(state.metadata.generatorVersion.value==3);
    CHECK(state.metadata.seed==WorldSeed{12345});
    CHECK(state.missionSimulation.version==simulation::currentMissionSimulationVersion);
    CHECK(state.missionSimulation.instances.entries().empty());
    CHECK(state.prologue.version==simulation::currentPrologueStateVersion);
    CHECK(state.prologue.phase==simulation::ProloguePhase::PreMission);
    const auto beforeHash=stateHash(state);
    const auto bytes=saveGameState(state);
    CHECK(bytes[16]==currentSaveFormatVersion.value);
    auto reloaded=loadGameState(bytes);
    CHECK(stateHash(reloaded)==beforeHash);
    CHECK(saveGameState(reloaded)==bytes);
}
TEST_CASE("MissionSimulation state round-trips through save/load") {
    GameState state{WorldSeed{12345}};
    simulation::initializeMissionSimulation(state);
    const auto& sim=state.missionSimulation;
    CHECK(sim.version==simulation::currentMissionSimulationVersion);
    const auto beforeHash=stateHash(state);
    const auto bytes=saveGameState(state);
    auto restored=loadGameState(bytes);
    CHECK(stateHash(restored)==beforeHash);
    CHECK(saveGameState(restored)==bytes);
    CHECK(restored.missionSimulation.version==sim.version);
    CHECK(restored.missionSimulation.statistics==sim.statistics);
}
