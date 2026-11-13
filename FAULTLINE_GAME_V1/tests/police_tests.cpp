#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/police/police_system.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "persistence/save.hpp"
#include "persistence/schema_v13.hpp"
#include "world/spatial/streaming.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <map>

using namespace faultline;
using namespace faultline::simulation;

namespace {
struct Fixture {
    GameState state{WorldSeed{12345}};
    PoliceEventBuffer events;
    NpcEventBuffer npcEvents;
    std::vector<NpcId> candidates;
    Fixture() {
        for (const auto& [id,npc]:state.world.npcs.entries())
            if (!npc.roles.has(world::NpcRole::FactionLeader)) candidates.push_back(id);
        REQUIRE(candidates.size()>=5);
        createPlayerEntity(state,state.world.npcs.get(candidates[0]).position);
    }
    EntityId activate(NpcId id,world::WorldPosition position) {
        auto& npc=state.world.npcs.get(id); npc.semantic.travel.reset(); npc.position=position;
        const auto entity=world::materializeNpc(state.world,id);
        state.world.transforms.get(entity).position=position; state.world.spatial.upsert(entity,position);
        state.player.residents.push_back({id,entity}); std::sort(state.player.residents.begin(),state.player.residents.end());
        return entity;
    }
    CrimeEventId crimeAt(world::WorldPosition position,std::uint16_t severity=4000) {
        return recordCrime(state,CrimeType::Assault,{CrimeActorKind::Player,{}},{},position,severity,
                           CrimeObservability::Visual,events);
    }
};

world::WorldPosition shifted(const GameState& state,world::WorldPosition p,double x,double z=0) {
    auto global=world::toGlobal(p,state.world.spatial.cellSize()); global.x+=x;global.z+=z;
    return world::fromGlobal(global,state.world.spatial.cellSize());
}

std::map<std::uint32_t,persistence::SaveBytes> sectionPayloads(const persistence::SaveBytes& bytes) {
    persistence::detail::Reader reader{bytes}; reader.offset=48;
    std::map<std::uint32_t,persistence::SaveBytes> result;
    while (reader.remaining()) {
        std::uint32_t id{},length{}; reader(id,length); reader.need(length);
        result.emplace(id,persistence::SaveBytes{bytes.begin()+static_cast<std::ptrdiff_t>(reader.offset),
            bytes.begin()+static_cast<std::ptrdiff_t>(reader.offset+length)});
        reader.offset+=length;
    }
    return result;
}
}

TEST_CASE("police state initializes neutral and old-neutral facts remain absent") {
    GameState state{WorldSeed{7}};
    CHECK(state.police.version==currentPoliceStateVersion);
    CHECK(state.police.wanted.state==WantedState::Clear);
    CHECK(state.police.crimes.empty()); CHECK(state.police.reports.empty());
    CHECK(state.police.knowledge.empty()); CHECK(state.police.responders.empty());
    CHECK(state.police.districtPressureBps.size()==state.world.districts.entries().size());
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("typed crime identity attribution and Combat ingestion are replay safe") {
    Fixture f; const auto target=f.candidates[0]; const auto targetEntity=f.activate(target,shifted(f.state,f.state.player.spawnPosition,2));
    const auto result=resolveCombatIntent(f.state,{{CombatActorKind::Player,f.state.player.controlledEntity,{}},
        {CombatActorKind::Npc,targetEntity,target}},f.npcEvents);
    REQUIRE(result.result==CombatResult::Hit);
    CombatEventBuffer combat; combat.push(result);
    ingestCombatCrimes(f.state,combat,f.events); ingestCombatCrimes(f.state,combat,f.events);
    REQUIRE(f.state.police.crimes.size()==1);
    const auto& crime=f.state.police.crimes.front();
    CHECK(crime.id==CrimeEventId{1}); CHECK(crime.actor.kind==CrimeActorKind::Player);
    CHECK(crime.victim==CrimeActorRef{CrimeActorKind::Npc,target});
    CHECK(f.state.police.statistics.duplicateFacts==1);
}

TEST_CASE("unwitnessed crime never becomes police knowledge") {
    Fixture f; (void)f.crimeAt(f.state.player.spawnPosition); observeNewCrimes(f.state,f.events);
    CHECK(f.state.police.observations.empty()); CHECK(f.state.police.reports.empty());
    auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
    Simulation simulation,other;
    for (int i=0;i<400;++i) {
        simulation.tick(f.state,{f.state.clock.tick(),{}, {}});
        other.tick(restored,{restored.clock.tick(),{}, {}});
        CHECK(stateHash(f.state)==stateHash(restored));
    }
    CHECK(f.state.police.knowledge.empty()); CHECK(f.state.police.wanted.state==WantedState::Clear);
    CHECK(f.state.police.responders.empty());
    CHECK(persistence::saveGameState(f.state)==persistence::saveGameState(restored));
}

TEST_CASE("visual witness schedules exactly one delayed report") {
    Fixture f; const auto witness=f.candidates[1]; const auto position=f.state.player.spawnPosition;
    f.activate(witness,position); const auto id=f.crimeAt(position); observeNewCrimes(f.state,f.events); observeNewCrimes(f.state,f.events);
    REQUIRE(f.state.police.observations.size()==1); REQUIRE(f.state.police.reports.size()==1);
    CHECK(f.state.police.observations[0].crime==id); CHECK(f.state.police.observations[0].witness==witness);
    CHECK(f.state.police.reports[0].completionTick==f.state.police.rules.reportDelayTicks);
    CHECK(f.state.police.reports[0].status==ReportStatus::Pending);
}

TEST_CASE("distance line of sight dead eligibility and adjacent cells are authoritative") {
    SUBCASE("out of range") {
        Fixture f; auto p=f.state.player.spawnPosition; f.activate(f.candidates[1],shifted(f.state,p,100));
        (void)f.crimeAt(p); observeNewCrimes(f.state,f.events); CHECK(f.state.police.reports.empty());
    }
    SUBCASE("building blocks visual crime") {
        Fixture f; const auto found=std::find_if(f.state.world.base->buildings.begin(),f.state.world.base->buildings.end(),
            [](const auto& value){return std::min(value.footprint.width,value.footprint.depth)<30;});
        REQUIRE(found!=f.state.world.base->buildings.end()); const auto& b=*found;
        const bool acrossX=b.footprint.width<=b.footprint.depth;
        const auto a=acrossX ? world::fromGlobal({static_cast<double>(b.footprint.x)-1,0,static_cast<double>(b.footprint.z)+b.footprint.depth/2.0}) :
            world::fromGlobal({static_cast<double>(b.footprint.x)+b.footprint.width/2.0,0,static_cast<double>(b.footprint.z)-1});
        const auto c=acrossX ? world::fromGlobal({static_cast<double>(world::right(b.footprint))+1,0,static_cast<double>(b.footprint.z)+b.footprint.depth/2.0}) :
            world::fromGlobal({static_cast<double>(b.footprint.x)+b.footprint.width/2.0,0,static_cast<double>(world::bottom(b.footprint))+1});
        REQUIRE(std::sqrt(world::distanceSquared(a,c))<f.state.police.rules.visualRangeMeters);
        f.activate(f.candidates[1],a); (void)f.crimeAt(c); observeNewCrimes(f.state,f.events);
        CHECK(f.state.police.reports.empty());
    }
    SUBCASE("dead NPC cannot newly witness") {
        Fixture f; auto id=f.candidates[1]; auto entity=f.activate(id,f.state.player.spawnPosition);
        world::destroyEntity(f.state.world,entity); std::erase_if(f.state.player.residents,[&](const auto& r){return r.npc==id;});
        setNpcLifecycle(f.state,id,world::NpcLifecycle::Dead,f.npcEvents);
        (void)f.crimeAt(f.state.player.spawnPosition); observeNewCrimes(f.state,f.events);
        CHECK(f.state.police.reports.empty());
    }
    SUBCASE("audible event crosses cell boundary") {
        Fixture f; const auto witnessPosition=world::WorldPosition{{0,0},{127.5F,0,10}};
        const auto crimePosition=world::WorldPosition{{1,0},{0.5F,0,10}};
        f.activate(f.candidates[1],witnessPosition);
        (void)recordCrime(f.state,CrimeType::WeaponDischarge,{CrimeActorKind::Player,{}},{},crimePosition,1500,
            CrimeObservability::VisualAndAudible,f.events);
        observeNewCrimes(f.state,f.events); CHECK(f.state.police.reports.size()==1);
    }
}

TEST_CASE("report deadline is exact and witness death interrupts it") {
    SUBCASE("deadline minus one and deadline") {
        Fixture f; f.activate(f.candidates[1],f.state.player.spawnPosition); (void)f.crimeAt(f.state.player.spawnPosition);
        observeNewCrimes(f.state,f.events); const auto deadline=f.state.police.reports[0].completionTick;
        f.state.clock=core::SimulationClock{deadline-1}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.reports[0].status==ReportStatus::Pending);
        f.state.clock=core::SimulationClock{deadline}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.reports[0].status==ReportStatus::Delivered);
        CHECK(f.state.police.knowledge.size()==1); CHECK(f.state.police.wanted.state==WantedState::Reported);
        const auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
        CHECK(restored.police.knowledge==f.state.police.knowledge);
        CHECK(restored.police.wanted==f.state.police.wanted);
        f.state.clock=core::SimulationClock{deadline+1}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.reports[0].status==ReportStatus::Delivered);
    }
    SUBCASE("death before completion") {
        Fixture f; auto witness=f.candidates[1]; auto entity=f.activate(witness,f.state.player.spawnPosition);
        (void)f.crimeAt(f.state.player.spawnPosition); observeNewCrimes(f.state,f.events);
        world::destroyEntity(f.state.world,entity); std::erase_if(f.state.player.residents,[&](const auto& r){return r.npc==witness;});
        setNpcLifecycle(f.state,witness,world::NpcLifecycle::Dead,f.npcEvents);
        f.state.clock=core::SimulationClock{f.state.police.rules.reportDelayTicks}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.reports[0].status==ReportStatus::Interrupted);
        CHECK(f.state.police.knowledge.empty()); CHECK(f.state.police.wanted.state==WantedState::Clear);
    }
}

TEST_CASE("abstraction does not erase an initiated semantic report") {
    Fixture f; const auto witness=f.candidates[1]; const auto entity=f.activate(witness,f.state.player.spawnPosition);
    (void)f.crimeAt(f.state.player.spawnPosition); observeNewCrimes(f.state,f.events);
    world::destroyEntity(f.state.world,entity); std::erase_if(f.state.player.residents,[&](const auto& r){return r.npc==witness;});
    f.state.clock=core::SimulationClock{f.state.police.rules.reportDelayTicks}; updateReportsAndWanted(f.state,f.events);
    CHECK(f.state.police.reports[0].status==ReportStatus::Delivered);
}

TEST_CASE("three witnesses are stable and knowledge is deduplicated") {
    Fixture f; const auto position=f.state.player.spawnPosition;
    for (std::size_t i=1;i<=3;++i) f.activate(f.candidates[i],position);
    (void)f.crimeAt(position); observeNewCrimes(f.state,f.events);
    REQUIRE(f.state.police.reports.size()==3);
    CHECK(f.state.police.reports[0].witness<f.state.police.reports[1].witness);
    CHECK(f.state.police.reports[1].witness<f.state.police.reports[2].witness);
    f.state.clock=core::SimulationClock{f.state.police.rules.reportDelayTicks}; updateReportsAndWanted(f.state,f.events);
    CHECK(f.state.police.knowledge.size()==1); CHECK(f.state.police.knowledge[0].deliveredReports==3);
    CHECK(f.state.police.statistics.wantedTransitions==1);
}

TEST_CASE("direct police observation bypasses civilian report") {
    Fixture f; const auto officer=f.candidates[1]; f.state.world.npcs.get(officer).roles.add(world::NpcRole::PoliceOfficer);
    f.activate(officer,f.state.player.spawnPosition); (void)f.crimeAt(f.state.player.spawnPosition);
    observeNewCrimes(f.state,f.events);
    CHECK(f.state.police.reports.empty()); REQUIRE(f.state.police.knowledge.size()==1);
    CHECK(f.state.police.knowledge[0].directObservation);
    CHECK(f.state.police.wanted.state==WantedState::Located);
}

TEST_CASE("police keeps last known position when hidden player moves") {
    Fixture f; const auto officer=f.candidates[1]; f.state.world.npcs.get(officer).roles.add(world::NpcRole::PoliceOfficer);
    f.activate(officer,f.state.player.spawnPosition); const auto a=f.state.player.spawnPosition;
    (void)f.crimeAt(a); observeNewCrimes(f.state,f.events); REQUIRE(f.state.police.wanted.lastKnownPosition==a);
    const auto b=shifted(f.state,a,200); f.state.world.transforms.get(f.state.player.controlledEntity).position=b;
    f.state.world.spatial.upsert(f.state.player.controlledEntity,b);
    CharacterAiEventBuffer ai; updatePoliceResponse(f.state,ai,f.events);
    CHECK(f.state.police.wanted.lastKnownPosition==a);
    CHECK(f.state.police.wanted.lastKnownPosition!=b);
    const auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
    CHECK(restored.police.wanted.lastKnownPosition==a);
    CHECK(restored.police.wanted.lastKnownPosition!=b);
}

TEST_CASE("Wanted transition table exhaustively rejects every forbidden edge") {
    constexpr std::array states{WantedState::Clear,WantedState::Suspicion,WantedState::Reported,WantedState::Searching,
        WantedState::Located,WantedState::Pursuit,WantedState::EscapedPending,WantedState::Cooldown};
    const auto expected=[](WantedState a,WantedState b){
        if(a==b)return true;
        if(a==WantedState::Clear)return b==WantedState::Suspicion||b==WantedState::Reported||b==WantedState::Located;
        if(a==WantedState::Suspicion)return b==WantedState::Clear||b==WantedState::Reported||b==WantedState::Located;
        if(a==WantedState::Reported)return b==WantedState::Searching||b==WantedState::Located;
        if(a==WantedState::Searching)return b==WantedState::Reported||b==WantedState::Located||b==WantedState::Cooldown;
        if(a==WantedState::Located)return b==WantedState::Pursuit||b==WantedState::EscapedPending;
        if(a==WantedState::Pursuit)return b==WantedState::EscapedPending;
        if(a==WantedState::EscapedPending)return b==WantedState::Located||b==WantedState::Searching||b==WantedState::Cooldown;
        return b==WantedState::Clear||b==WantedState::Reported||b==WantedState::Located;
    };
    for(auto from:states)for(auto to:states)CHECK(isAllowedWantedTransition(from,to)==expected(from,to));
    for (auto value:states) {
        Fixture persisted; persisted.state.police.wanted.state=value;
        if (value!=WantedState::Clear) {
            persisted.state.police.wanted.heatBps=1;
            persisted.state.police.wanted.lastKnownPosition=persisted.state.player.spawnPosition;
            persisted.state.police.wanted.deadlineTick=10;
        }
        const auto restored=persistence::loadGameState(persistence::saveGameState(persisted.state));
        CHECK(restored.police.wanted.state==value);
        CHECK(stateHash(restored)==stateHash(persisted.state));
    }
    Fixture f; f.state.police.wanted.state=WantedState::Pursuit;
    CHECK_THROWS(transitionWanted(f.state,WantedState::Clear,0,f.events));
}

TEST_CASE("every deadline-driven Wanted transition fires at deadline not before") {
    struct Case { WantedState from,to; };
    constexpr std::array cases{Case{WantedState::Suspicion,WantedState::Clear},
        Case{WantedState::Reported,WantedState::Searching},Case{WantedState::Searching,WantedState::Cooldown},
        Case{WantedState::Located,WantedState::Pursuit},Case{WantedState::EscapedPending,WantedState::Searching},
        Case{WantedState::Cooldown,WantedState::Clear}};
    for (const auto value:cases) {
        Fixture f; f.state.police.wanted={value.from,4000,true,f.state.player.spawnPosition,0,100,40};
        f.state.clock=core::SimulationClock{99}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.wanted.state==value.from);
        f.state.clock=core::SimulationClock{100}; updateReportsAndWanted(f.state,f.events);
        CHECK(f.state.police.wanted.state==value.to);
    }
    Fixture pursuit; pursuit.state.police.wanted={WantedState::Pursuit,4000,true,pursuit.state.player.spawnPosition,
        0,0,40};
    const auto location=pursuit.state.world.npcs.get(pursuit.candidates[1]).location;
    const auto officer=createPoliceOfficer(pursuit.state,pursuit.state.player.spawnPosition,location);
    pursuit.activate(officer,pursuit.state.player.spawnPosition);
    pursuit.state.police.responders.push_back({officer,PoliceResponderStatus::Active,0,0,pursuit.state.player.spawnPosition});
    const auto hidden=shifted(pursuit.state,pursuit.state.player.spawnPosition,200);
    pursuit.state.world.transforms.get(pursuit.state.player.controlledEntity).position=hidden;
    pursuit.state.world.spatial.upsert(pursuit.state.player.controlledEntity,hidden);
    CharacterAiEventBuffer ai;
    pursuit.state.clock=core::SimulationClock{pursuit.state.police.rules.lostContactTicks-1};
    updatePoliceResponse(pursuit.state,ai,pursuit.events); CHECK(pursuit.state.police.wanted.state==WantedState::Pursuit);
    pursuit.state.clock=core::SimulationClock{pursuit.state.police.rules.lostContactTicks};
    updatePoliceResponse(pursuit.state,ai,pursuit.events); CHECK(pursuit.state.police.wanted.state==WantedState::EscapedPending);
}

TEST_CASE("Wanted deadline transitions occur at the boundary and survive save load") {
    Fixture f; f.activate(f.candidates[1],f.state.player.spawnPosition); (void)f.crimeAt(f.state.player.spawnPosition);
    observeNewCrimes(f.state,f.events);
    Simulation simulation;
    while(f.state.clock.tick()<f.state.police.rules.reportDelayTicks) simulation.tick(f.state,{f.state.clock.tick(),{}, {}});
    auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
    REQUIRE(stateHash(f.state)==stateHash(restored));
    Simulation other;
    for(int i=0;i<100;++i){simulation.tick(f.state,{f.state.clock.tick(),{}, {}});other.tick(restored,{restored.clock.tick(),{}, {}});CHECK(stateHash(f.state)==stateHash(restored));}
    CHECK(persistence::saveGameState(f.state)==persistence::saveGameState(restored));
}

TEST_CASE("police response reuses Character AI movement and CombatSystem damage") {
    Fixture f; const auto location=f.state.world.npcs.get(f.candidates[1]).location;
    const auto officer=createPoliceOfficer(f.state,shifted(f.state,f.state.player.spawnPosition,5),location);
    const auto entity=world::materializeNpc(f.state.world,officer); f.state.player.residents.push_back({officer,entity});
    std::sort(f.state.player.residents.begin(),f.state.player.residents.end());
    f.state.police.wanted={WantedState::Pursuit,8000,true,f.state.player.spawnPosition,0,0,40};
    f.state.police.responders.push_back({officer,PoliceResponderStatus::Active,0,0,f.state.player.spawnPosition});
    const auto beforePosition=f.state.world.transforms.get(entity).position;
    const auto beforeHealth=f.state.world.health.get(f.state.player.controlledEntity).current;
    CharacterAiEventBuffer ai; updatePoliceResponse(f.state,ai,f.events);
    CHECK(f.state.world.transforms.get(entity).position!=beforePosition);
    CHECK(f.state.world.health.get(f.state.player.controlledEntity).current==beforeHealth);
    CombatEventBuffer combat; updateCombat(f.state,{},ai,combat,f.npcEvents);
    CHECK(f.state.world.health.get(f.state.player.controlledEntity).current<beforeHealth);
}

TEST_CASE("response identity survives snapshot reconstruction without duplicate bodies") {
    Fixture f; const auto location=f.state.world.npcs.get(f.candidates[1]).location;
    const auto officer=createPoliceOfficer(f.state,f.state.player.spawnPosition,location);
    const auto body=world::materializeNpc(f.state.world,officer); f.state.player.residents.push_back({officer,body});
    std::sort(f.state.player.residents.begin(),f.state.player.residents.end());
    f.state.police.responders.push_back({officer,PoliceResponderStatus::Active,0,0,f.state.player.spawnPosition});
    auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
    CHECK_FALSE(restored.world.npcs.get(officer).activeEntity); restorePlayerResidency(restored);
    CHECK(restored.world.npcs.get(officer).activeEntity==body);
    const auto next=restored.world.entities.nextId(); restorePlayerResidency(restored);
    CHECK(restored.world.entities.nextId()==next); CHECK(restored.world.npcs.get(officer).activeEntity==body);
    CHECK(restored.police.responders==f.state.police.responders);
}

TEST_CASE("active hidden-target response continues identically after save load") {
    Fixture f; const auto origin=f.state.player.spawnPosition;
    const auto location=f.state.world.npcs.get(f.candidates[1]).location;
    const auto officer=createPoliceOfficer(f.state,shifted(f.state,origin,5),location);
    f.activate(officer,shifted(f.state,origin,5));
    f.state.police.wanted={WantedState::Pursuit,6000,true,origin,0,0,40};
    f.state.police.responders.push_back({officer,PoliceResponderStatus::Active,0,0,origin});
    const auto hidden=shifted(f.state,origin,200);
    f.state.world.transforms.get(f.state.player.controlledEntity).position=hidden;
    f.state.world.spatial.upsert(f.state.player.controlledEntity,hidden);
    auto restored=persistence::loadGameState(persistence::saveGameState(f.state));
    Simulation first,second;
    for (int i=0;i<80;++i) {
        first.tick(f.state,{f.state.clock.tick(),{}, {}});
        second.tick(restored,{restored.clock.tick(),{}, {}});
        CHECK(stateHash(f.state)==stateHash(restored));
        CHECK(f.state.police.wanted==restored.police.wanted);
        CHECK(f.state.police.responders==restored.police.responders);
        CHECK(f.state.world.entities.nextId()==restored.world.entities.nextId());
    }
    CHECK(persistence::saveGameState(f.state)==persistence::saveGameState(restored));
    CHECK(f.state.police.wanted.lastKnownPosition==origin);
}

TEST_CASE("arrest advances exactly 120 game days and respawns instead of game over") {
    Fixture f; const auto before=f.state.clock.tick(); f.state.police.pendingArrest=true;
    f.state.police.arrestPosition=f.state.player.spawnPosition;
    resolvePlayerArrest(f.state);
    CHECK(f.state.clock.tick()==before+f.state.police.rules.arrestAdvanceTicks);
    CHECK(f.state.player.controlledEntity); CHECK(f.state.world.health.get(f.state.player.controlledEntity).current==100);
    CHECK(f.state.police.statistics.arrests==1); CHECK(f.state.police.wanted.state==WantedState::Clear);
    CHECK_NOTHROW(validateState(f.state));
}

TEST_CASE("Phase 20 state hashes persists and malformed state is rejected") {
    Fixture f; f.activate(f.candidates[1],f.state.player.spawnPosition); (void)f.crimeAt(f.state.player.spawnPosition);
    observeNewCrimes(f.state,f.events);
    const auto hash=stateHash(f.state); const auto bytes=persistence::saveGameState(f.state);
    auto restored=persistence::loadGameState(bytes);
    CHECK(stateHash(restored)==hash); CHECK(persistence::saveGameState(restored)==bytes);
    restored.police.version=99; CHECK_THROWS(validateState(restored)); CHECK_THROWS((void)persistence::saveGameState(restored));
    auto malformed=bytes;
    persistence::detail::Reader reader{malformed}; reader.offset=48;
    bool found=false;
    while (reader.remaining()) {
        std::uint32_t id{},length{}; reader(id,length); reader.need(length);
        if (id==21) { malformed[reader.offset]=99; found=true; break; }
        reader.offset+=length;
    }
    REQUIRE(found); CHECK_THROWS((void)persistence::loadGameState(malformed));
}

TEST_CASE("authentic v12 fixture migrates to neutral Phase 20 without RNG consumption") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v12_phase19.flsave";
    std::ifstream file{path,std::ios::binary}; REQUIRE(file.is_open());
    const persistence::SaveBytes oldBytes{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    auto state=persistence::loadGameState(oldBytes);
    CHECK(state.police.wanted.state==WantedState::Clear); CHECK(state.police.crimes.empty());
    CHECK(state.police.reports.empty()); CHECK(state.police.knowledge.empty());
    RuntimeRandomState expected{state.metadata.seed};
    CHECK(state.randomness.get(RandomDomain::Police).state()==expected.get(RandomDomain::Police).state());
    const auto oldSections=sectionPayloads(oldBytes), migratedSections=sectionPayloads(persistence::saveGameState(state));
    for (const auto& [id,payload]:oldSections) {
        REQUIRE(migratedSections.contains(id)); CHECK(migratedSections.at(id)==payload);
    }
    CHECK(state.metadata.generatorVersion.value==3); CHECK_NOTHROW(validateState(state));
}
