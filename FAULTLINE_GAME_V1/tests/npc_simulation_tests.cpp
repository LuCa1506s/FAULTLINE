#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "world/spatial/streaming.hpp"
#include "persistence/schema_v3.hpp"
#include "legacy_save_v1.hpp"
#include <fstream>
#include "save_compatibility.hpp"
#include <type_traits>

using namespace faultline;
using namespace faultline::world;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
GameState small() {
    WorldGenerationConfig c; c.width=512; c.depth=512; c.waterPercent=0;
    GameState state{WorldSeed{12345},c}; state.factionSimulation.nextEvaluationTick=neverTick;
    return state;
}
NpcId civilian(GameState& state) {
    NpcState n; n.identity.name="Test civilian"; n.location=state.world.npcs.entries().begin()->second.location;
    n.position=state.world.npcs.entries().begin()->second.position;
    const auto id=state.world.npcs.create(n); initializeNpc(state,id); return id;
}
WorldLocation destination(const GameState& state,NpcId id) {
    for (const auto& t : state.world.base->territories)
        if (t.id!=state.world.npcs.get(id).location.territory) return {t.district,t.id,{}};
    throw std::logic_error("Travel test needs two territories");
}
void advanceTo(GameState& state,std::uint64_t end) { Simulation sim; sim.advanceHeadless(state,end-state.clock.tick()); }
OperationId launch(GameState& state,FactionId faction) {
    FactionEventBuffer events;
    for (const auto& [target,t] : state.world.territories.entries()) {
        (void)t;
        const auto result=launchFactionOperation(state,faction,{OperationType::AttackTerritory,target,{},4,10000},events);
        if (result) return result.operation;
    }
    throw std::logic_error("No adjacent attack available in fixture");
}
SaveBytes fileBytes(const char* name) {
    std::ifstream f{std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests"/"fixtures"/name,std::ios::binary};
    if (!f) throw std::runtime_error("Missing historical save fixture");
    return {std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{}};
}
std::size_t section(const SaveBytes& bytes,std::uint32_t tag) {
    detail::Reader r{bytes}; r.offset=48;
    while (r.remaining()) { std::uint32_t id{},length{}; r(id,length); if (id==tag) return r.offset; r.need(length); r.offset+=length; }
    throw std::logic_error("Section missing");
}
void setU64(SaveBytes& bytes,std::size_t offset,std::uint64_t value) {
    for (std::size_t i=0;i<8;++i) bytes.at(offset+i)=static_cast<std::uint8_t>((value>>(8*i))&255U);
}
}
static_assert(!std::is_convertible_v<EntityId,NpcId>);
static_assert(!std::is_copy_constructible_v<EphemeralActor>);
TEST_CASE("semantic scheduler uses integer actor deadlines and shares the fixed tick pipeline") {
    auto coarse=small(),fixed=coarse; Simulation sim;
    const auto initial=coarse.randomness;
    advanceTo(coarse,799); CHECK(coarse.npcSimulation.statistics.updates==0);
    advanceTo(coarse,2401);
    for (int i=0;i<2401;++i) sim.tick(fixed,{fixed.clock.tick(),{},{}});
    CHECK(coarse.npcSimulation.statistics.updates==coarse.world.npcs.entries().size()*3);
    CHECK(stateHash(coarse)==stateHash(fixed)); CHECK(saveGameState(coarse)==saveGameState(fixed));
    for (std::size_t i=0;i<initial.streams.size();++i)
        if (i!=static_cast<std::size_t>(RandomDomain::Npc)) CHECK(initial.streams[i].state()==coarse.randomness.streams[i].state());
    CHECK(coarse.world.entities.entries().empty()); CHECK_NOTHROW(validateState(coarse));
}
TEST_CASE("travel has valid endpoints and completes at its exact deadline without physical entities") {
    auto state=small(); const auto id=civilian(state); NpcEventBuffer events;
    const auto origin=state.world.npcs.get(id).location, dest=destination(state,id);
    startNpcTravel(state,id,dest,events); const auto arrival=state.world.npcs.get(id).semantic.travel->arrivalTick;
    CHECK(arrival>0); CHECK(arrival==npcTravelDuration(state,origin,dest));
    CHECK_THROWS(startNpcTravel(state,id,dest,events)); CHECK_THROWS(materializeNpc(state.world,id));
    advanceTo(state,arrival); CHECK(state.world.npcs.get(id).semantic.travel.has_value());
    advanceTo(state,arrival+1); CHECK_FALSE(state.world.npcs.get(id).semantic.travel);
    CHECK(state.world.npcs.get(id).location==dest); CHECK(state.npcSimulation.statistics.travelsCompleted==1);
    CHECK_NOTHROW(validateState(state));
}
TEST_CASE("save halfway through travel preserves hash canonical bytes and future arrival") {
    auto a=small(); const auto id=civilian(a); NpcEventBuffer events;
    startNpcTravel(a,id,destination(a,id),events); const auto arrival=a.world.npcs.get(id).semantic.travel->arrivalTick;
    advanceTo(a,arrival/2); const auto bytes=saveGameState(a); auto b=loadGameState(bytes);
    CHECK(stateHash(a)==stateHash(b)); CHECK(saveGameState(b)==bytes);
    CHECK(a.world.npcs.get(id).semantic==b.world.npcs.get(id).semantic);
    advanceTo(a,arrival+1601); advanceTo(b,arrival+1601);
    CHECK(stateHash(a)==stateHash(b)); CHECK(saveGameState(a)==saveGameState(b));
}
TEST_CASE("travel validation and explicit cancellation never teleport the actor") {
    auto state=small(); const auto id=civilian(state); const auto original=state.world.npcs.get(id); NpcEventBuffer events;
    CHECK_THROWS(startNpcTravel(state,id,original.location,events));
    CHECK_THROWS(startNpcTravel(state,id,{DistrictId{999},{},{}},events));
    const auto body=materializeNpc(state.world,id); CHECK_THROWS(startNpcTravel(state,id,destination(state,id),events));
    destroyEntity(state.world,body); startNpcTravel(state,id,destination(state,id),events);
    cancelNpcTravel(state,id,events); CHECK(state.world.npcs.get(id).location==original.location);
    CHECK(state.world.npcs.get(id).position==original.position); CHECK(state.npcSimulation.statistics.travelsCancelled==1);
    CHECK_THROWS(cancelNpcTravel(state,id,events)); CHECK_NOTHROW(validateState(state));
}
TEST_CASE("restrictive lifecycle interrupts travel and prevents ordinary activity or assignments") {
    for (auto lifecycle : {NpcLifecycle::Injured,NpcLifecycle::Arrested,NpcLifecycle::Missing,NpcLifecycle::Dead}) {
        auto state=small(); const auto id=civilian(state); NpcEventBuffer events;
        startNpcTravel(state,id,destination(state,id),events); setNpcLifecycle(state,id,lifecycle,events);
        const auto restricted=state.world.npcs.get(id);
        advanceTo(state,3*gameHourTicks); CHECK(state.world.npcs.get(id)==restricted);
        CHECK_THROWS(startNpcTravel(state,id,destination(state,id),events)); CHECK_NOTHROW(validateState(state));
        if (lifecycle==NpcLifecycle::Dead) CHECK_THROWS(setNpcLifecycle(state,id,NpcLifecycle::Alive,events));
        auto loaded=loadGameState(saveGameState(state)); CHECK(loaded.world.npcs.get(id)==restricted);
    }
    auto state=small(); NpcEventBuffer events; const auto leader=state.world.factions.entries().begin()->second.leader;
    CHECK_THROWS(setNpcLifecycle(state,leader,NpcLifecycle::Dead,events));
}
TEST_CASE("leaders stay at base and dealers work or hide according to courage and faction confidence") {
    auto state=small(); const auto faction=state.world.factions.entries().begin()->first;
    const auto leader=state.world.factions.get(faction).leader; const auto home=state.world.npcs.get(leader).location;
    NpcId dealer;
    for (const auto& [id,n] : state.world.npcs.entries()) if (n.faction==faction && n.roles.has(NpcRole::Dealer)) dealer=id;
    REQUIRE(dealer); state.world.npcs.get(dealer).traits.courage=1000; state.world.factions.get(faction).strategy.confidence=1000;
    advanceTo(state,801); CHECK(state.world.npcs.get(dealer).semantic.activity==NpcActivity::Hiding);
    state.world.factions.get(faction).strategy.confidence=5000; advanceTo(state,1601);
    CHECK(state.world.npcs.get(dealer).semantic.activity==NpcActivity::Working);
    CHECK(state.world.npcs.get(leader).location==home); CHECK_FALSE(state.world.npcs.get(leader).semantic.travel);
}
TEST_CASE("personality gates optional support without adding force or committing named manpower twice") {
    auto state=small(); const auto faction=state.world.factions.entries().begin()->first;
    const auto leader=state.world.factions.get(faction).leader;
    const auto op=launch(state,faction); const auto resources=factionResources(state,faction);
    NpcId willing,reluctant;
    for (bool accept : {false,true}) {
        NpcState n=state.world.npcs.get(leader); n.roles={}; n.roles.add(NpcRole::FactionMember);
        n.major=false; n.authority=AuthorityTier::Member; n.activeEntity={}; n.semantic={};
        n.traits={5000,static_cast<std::uint16_t>(accept?9000:1000),5000,static_cast<std::uint16_t>(accept?9000:1000),5000,static_cast<std::uint16_t>(accept?9000:1000)};
        n.capabilities.leadership=5000; const auto id=state.world.npcs.create(n); initializeNpc(state,id);
        if (accept) willing=id; else reluctant=id;
    }
    const auto before=factionResources(state,faction); advanceTo(state,801);
    CHECK(state.world.npcs.get(willing).semantic.operation==op); CHECK_FALSE(state.world.npcs.get(reluctant).semantic.operation);
    CHECK(state.world.npcs.get(leader).semantic.operation==op); CHECK_FALSE(state.world.npcs.get(leader).semantic.travel);
    CHECK(factionResources(state,faction).totalManpower==before.totalManpower);
    CHECK(factionResources(state,faction).committedManpower==resources.committedManpower);
    CHECK(state.factionSimulation.operations.get(op).commitment.manpower==4);
    CHECK_NOTHROW(validateState(state));
    auto loaded=loadGameState(saveGameState(state));
    CHECK(loaded.world.npcs.get(willing).semantic.operation==op);
    const auto deadline=state.factionSimulation.operations.get(op).resolutionTick;
    advanceTo(loaded,deadline+1); advanceTo(state,deadline+1);
    CHECK(stateHash(loaded)==stateHash(state)); CHECK(saveGameState(loaded)==saveGameState(state));
    CHECK_FALSE(state.world.npcs.get(willing).semantic.operation); CHECK_FALSE(state.world.npcs.get(leader).semantic.operation);
    for (const auto& [id,n] : state.world.npcs.entries()) { (void)id; CHECK(n.lifecycle==NpcLifecycle::Alive); }
    CHECK(state.npcSimulation.statistics.relationshipChanges>0); CHECK_NOTHROW(validateState(state));
}
TEST_CASE("operation cancellation releases semantic assignments and cancels dependent travel") {
    auto state=small(); const auto faction=state.world.factions.entries().begin()->first;
    const auto op=launch(state,faction); advanceTo(state,801);
    FactionEventBuffer factionEvents; NpcEventBuffer npcEvents;
    cancelFactionOperation(state,op,factionEvents); updateNpcs(state,npcEvents,true);
    for (const auto& [id,n] : state.world.npcs.entries()) { (void)id; CHECK(n.semantic.operation!=op); }
    CHECK_NOTHROW(validateState(state));
}
TEST_CASE("relationship events own directed sparse bounded updates and reject invalid references") {
    auto state=small(); NpcEventBuffer events; const auto from=NpcId{1},to=NpcId{2};
    const auto count=state.world.npcRelationships.size();
    for (int i=0;i<500;++i) RelationshipSystem::apply(state,{RelationshipEventKind::Helped,from,to},events);
    CHECK(state.world.npcRelationships.size()<=count+1);
    const auto lookup=[&]() -> const NpcRelationship& {
        return *std::find_if(state.world.npcRelationships.begin(),state.world.npcRelationships.end(),[&](const auto& r){return r.from==from && r.to==to;});
    };
    CHECK(lookup().trust==10000); CHECK(lookup().debt==10000); CHECK(lookup().personalLoyalty==10000);
    for (int i=0;i<500;++i) RelationshipSystem::apply(state,{RelationshipEventKind::Threatened,from,to},events);
    CHECK(lookup().trust==0); CHECK(lookup().fear==10000); CHECK(lookup().hostility==10000);
    RelationshipSystem::apply(state,{RelationshipEventKind::Abandoned,from,to},events);
    CHECK_THROWS(RelationshipSystem::apply(state,{RelationshipEventKind::Helped,from,from},events));
    CHECK_THROWS(RelationshipSystem::apply(state,{RelationshipEventKind::Helped,from,NpcId{999}},events));
    auto reversed=state; std::reverse(reversed.world.npcRelationships.begin(),reversed.world.npcRelationships.end());
    CHECK(saveGameState(reversed)==saveGameState(state)); CHECK(stateHash(loadGameState(saveGameState(state)))==stateHash(state));
}
TEST_CASE("headless promotion preserves meaningful identity and allocator continuity through save load") {
    auto a=small(); const auto highWater=a.world.npcs.nextId(); NpcEventBuffer events; EphemeralActor actor;
    actor.state=NpcState{}; actor.state.identity.name="Promoted civilian";
    actor.state.location=a.world.npcs.entries().begin()->second.location;
    actor.state.traits.courage=6500; actor.state.semantic.activity=NpcActivity::Working;
    const auto id=promoteNpc(a,actor,events);
    CHECK(id.value()==highWater); CHECK_FALSE(a.world.npcs.get(id).major); CHECK(a.world.npcs.get(id).identity.name=="Promoted civilian");
    CHECK(a.world.npcs.get(id).traits.courage==6500); CHECK(a.world.npcs.get(id).semantic.activity==NpcActivity::Working);
    CHECK_THROWS((void)promoteNpc(a,actor,events));
    auto b=loadGameState(saveGameState(a)); CHECK(a.world.npcs.get(id)==b.world.npcs.get(id));
    EphemeralActor nextA,nextB; nextA.state=actor.state; nextB.state=actor.state;
    const auto idA=promoteNpc(a,nextA,events),idB=promoteNpc(b,nextB,events);
    CHECK(idA==idB); CHECK(idA>id); CHECK(stateHash(a)==stateHash(b)); CHECK(a.npcSimulation.statistics.promotions==2);
    for (const auto& generated : a.world.base->majorNpcs) CHECK(generated.id<id);
}
TEST_CASE("promotion links an existing body in place and abstracts only the save snapshot") {
    auto state=small(); NpcEventBuffer events; EphemeralActor actor;
    actor.state.identity.name="Existing pedestrian"; actor.state.location=state.world.npcs.entries().begin()->second.location;
    const auto entity=state.world.entities.create(); actor.state.activeEntity=entity;
    state.world.transforms.set(state.world.entities,entity,{fromGlobal({12,0,17})}); state.world.health.set(state.world.entities,entity,{80,100});
    const auto nextEntity=state.world.entities.nextId(); const auto id=promoteNpc(state,actor,events);
    CHECK(state.world.npcs.get(id).activeEntity==entity); CHECK(state.world.entities.exists(entity));
    CHECK(state.world.entities.nextId()==nextEntity); CHECK(state.world.npcLinks.get(entity).npc==id);
    CHECK(state.world.npcs.get(id).health.current==80); CHECK(state.world.npcs.get(id).position==fromGlobal({12,0,17}));
    const auto loaded=loadGameState(saveGameState(state)); CHECK_FALSE(loaded.world.npcs.get(id).activeEntity);
    CHECK(loaded.world.npcs.get(id).identity==actor.state.identity); CHECK(stateHash(state)==stateHash(loaded));
    EphemeralActor duplicate; duplicate.state=actor.state; CHECK_THROWS((void)promoteNpc(state,duplicate,events));
}
TEST_CASE("aggregate faction promotion transfers one reserve person without duplicating manpower") {
    auto state=small(); NpcEventBuffer events; EphemeralActor actor;
    const auto faction=state.world.factions.entries().begin()->first;
    actor.state.identity.name="Named member"; actor.state.faction=faction;
    actor.state.location=state.world.npcs.get(state.world.factions.get(faction).leader).location;
    const auto resources=factionResources(state,faction); const auto reserve=state.world.factions.get(faction).reserveMembers;
    const auto hash=stateHash(state); CHECK_THROWS((void)promoteNpc(state,actor,events)); CHECK(stateHash(state)==hash);
    actor.fromFactionReserve=true; const auto id=promoteNpc(state,actor,events);
    CHECK(state.world.npcs.get(id).roles.has(NpcRole::FactionMember)); CHECK(factionResources(state,faction).totalManpower==resources.totalManpower);
    CHECK(state.world.factions.get(faction).reserveMembers==reserve-1); CHECK(factionResources(state,faction).committedManpower==0);
    auto b=loadGameState(saveGameState(state)); CHECK(factionResources(b,faction).totalManpower==resources.totalManpower);
}
TEST_CASE("invalid promotion is transactional and exhausted ID allocators never reuse IDs") {
    auto state=small(); NpcEventBuffer events; EphemeralActor actor; actor.state.identity.name="Invalid";
    const auto before=stateHash(state); CHECK_THROWS((void)promoteNpc(state,actor,events)); CHECK(stateHash(state)==before);
    actor.state.location=state.world.npcs.entries().begin()->second.location; actor.state.traits.courage=10001;
    CHECK_THROWS((void)promoteNpc(state,actor,events)); CHECK(stateHash(state)==before); CHECK_FALSE(actor.promotedTo);
    actor.state.traits.courage=5000; state.world.npcs=NpcStore::restore(0,state.world.npcs.entries());
    const auto exhausted=stateHash(state); CHECK_THROWS((void)promoteNpc(state,actor,events)); CHECK(stateHash(state)==exhausted);
}
TEST_CASE("v1 migration initializes semantic defaults without consuming RNG or changing generated identity") {
    for (std::uint32_t version=1;version<=3;++version) {
        WorldGenerationConfig c; c.width=512; c.depth=512; c.waterPercent=0;
        GameState original{WorldSeed{42},c,GeneratorVersion{version}}; original.clock=core::SimulationClock{400};
        auto migrated=loadGameState(test::legacyV1(original));
        CHECK(world::baseWorldHash(*original.world.base)==world::baseWorldHash(*migrated.world.base));
        for (std::size_t i=0;i<original.randomness.streams.size();++i) CHECK(original.randomness.streams[i].state()==migrated.randomness.streams[i].state());
        for (const auto& [id,n] : migrated.world.npcs.entries()) {
            CHECK(n.semantic.activity==NpcActivity::Idle); CHECK_FALSE(n.semantic.travel); CHECK_FALSE(n.semantic.operation);
            CHECK(n.semantic.home==n.location); CHECK(n.semantic.nextUpdateTick==1200); CHECK(n.identity==original.world.npcs.get(id).identity);
        }
        CHECK(migrated.npcSimulation.statistics==NpcStatistics{}); CHECK_NOTHROW(validateState(migrated));
    }
}
TEST_CASE("real Phase 9 v2 save with active operations migrates and continues deterministically") {
    const auto bytes=fileBytes("save_v2_phase9.flsave"); REQUIRE(readSaveHeader(bytes).saveFormatVersion.value==2);
    auto a=loadGameState(bytes),b=loadGameState(bytes);
    CHECK(a.clock.tick()==30*gameDayTicks); CHECK(a.factionSimulation.operations.entries().size()==468);
    CHECK(a.factionSimulation.pendingOperations.size()==8); CHECK(a.factionSimulation.operations.nextId()==469);
    detail::Reader oldRuntime{bytes}; oldRuntime.offset=section(bytes,1);
    std::uint64_t tick{},gameMs{}; float cell{}; std::uint32_t streams{}; oldRuntime(tick,gameMs,cell,streams);
    REQUIRE(streams==a.randomness.streams.size());
    for (const auto& rng : a.randomness.streams) { core::DeterministicRng::State words{}; oldRuntime(words); CHECK(rng.state()==words); }
    CHECK(a.world.npcs.nextId()==23); CHECK(baseWorldHash(*a.world.base)==0x888c5dbd10c15886ULL);
    CHECK(a.world.factions.get(FactionId{1}).treasury==131801); CHECK(a.world.factions.get(FactionId{1}).strategy.confidence==10000);
    for (const auto& [id,n] : a.world.npcs.entries()) {
        (void)id; CHECK(n.semantic.activity==NpcActivity::Idle); CHECK_FALSE(n.semantic.travel); CHECK_FALSE(n.semantic.operation);
        CHECK(n.semantic.home==n.location); CHECK(n.semantic.nextUpdateTick==30*gameDayTicks+800);
    }
    CHECK(saveGameState(a)==saveGameState(b)); CHECK(readSaveHeader(saveGameState(a)).saveFormatVersion==currentSaveFormatVersion);
    const auto migratedBytes=saveGameState(a);
    REQUIRE(migratedBytes.size()>bytes.size());
    // All eleven old sections are preserved byte for byte, independently of the
    // current encoder's new header/section dispatcher and migration defaults.
    CHECK(test::preservesHistoricalPayloads(bytes,migratedBytes));
    advanceTo(a,31*gameDayTicks); b=loadGameState(saveGameState(b)); advanceTo(b,31*gameDayTicks);
    CHECK(stateHash(a)==stateHash(b)); CHECK(a.npcSimulation.statistics.participations>0); CHECK_NOTHROW(validateState(a));
}
TEST_CASE("runtime validation rejects invalid activity travel scheduler membership and relationship data") {
    const auto good=small(); const auto id=good.world.npcs.entries().rbegin()->first;
    auto bad=good; bad.world.npcs.get(id).semantic.activity=NpcActivity::Travelling; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcs.get(id).semantic.nextUpdateTick=core::SimulationClock::maxTick+1; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcs.get(id).faction={}; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcs.get(id).location={}; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcs.get(id).semantic.operation=OperationId{999}; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcRelationships.front().trust=10001; CHECK_THROWS(validateState(bad));
    bad=good; bad.world.npcRelationships.push_back(bad.world.npcRelationships.front()); CHECK_THROWS(validateState(bad));
    bad=good; NpcEventBuffer events; startNpcTravel(bad,id,destination(bad,id),events);
    bad.world.npcs.get(id).semantic.travel->arrivalTick=0; CHECK_THROWS(validateState(bad));
    auto bytes=saveGameState(good); const auto pos=section(bytes,12);
    bytes.at(pos+20+56+4+8)=255; CHECK_THROWS_AS((void)loadGameState(bytes),SaveError); // first activity
    bytes=saveGameState(good); setU64(bytes,pos+20+56+4,999); CHECK_THROWS_AS((void)loadGameState(bytes),SaveError); // unknown actor key
    bytes=saveGameState(good); setU64(bytes,pos,0); CHECK_THROWS_AS((void)loadGameState(bytes),SaveError); // invalid interval
}
TEST_CASE("semantic state hashes include travel scheduling activity statistics and promotions") {
    const auto a=small(); const auto id=a.world.npcs.entries().begin()->first;
    auto b=a; b.world.npcs.get(id).semantic.nextUpdateTick++; CHECK(stateHash(a)!=stateHash(b));
    b=a; b.world.npcs.get(id).semantic.activity=NpcActivity::Working; CHECK(stateHash(a)!=stateHash(b));
    b=a; b.npcSimulation.statistics.promotions++; CHECK(stateHash(a)!=stateHash(b));
    b=a; NpcEventBuffer events; startNpcTravel(b,id,destination(b,id),events); CHECK(stateHash(a)!=stateHash(b));
    CHECK(baseWorldHash(*a.world.base)==baseWorldHash(*b.world.base));
}
TEST_CASE("clock limit migration has no overflow or busy-loop semantic scheduler") {
    auto state=small(); state.clock=core::SimulationClock{core::SimulationClock::maxTick-10}; initializeNpcSimulation(state);
    CHECK(nextNpcDueTick(state)==neverTick); CHECK_NOTHROW(validateNpcSimulation(state));
    const auto id=state.world.npcs.entries().begin()->first; NpcEventBuffer events;
    CHECK_THROWS(startNpcTravel(state,id,destination(state,id),events));
}

TEST_CASE("legacy position-only NPCs remain valid without inventing geography during migration") {
    WorldState world; NpcState n; n.identity.name="Position-only legacy actor"; n.position=fromGlobal({12,0,-80});
    const auto npc=world.npcs.create(n);
    GameState state{{WorldSeed{42},GeneratorVersion{1}},core::SimulationClock{17},RuntimeRandomState{WorldSeed{42}},std::move(world)};
    auto loaded=loadGameState(test::legacyV1(state));
    CHECK(loaded.world.npcs.get(npc).location==WorldLocation{}); CHECK(loaded.world.npcs.get(npc).position==n.position);
    advanceTo(loaded,2000); CHECK_NOTHROW(validateState(loaded)); CHECK(loaded.npcSimulation.statistics.updates==2);
    NpcEventBuffer events; CHECK_THROWS(startNpcTravel(loaded,npc,{},events));
}
TEST_CASE("runtime faction support and relationship updates leave frozen worldgen baselines untouched") {
    GameState state{WorldSeed{12345}}; const auto base=baseWorldHash(*state.world.base);
    advanceTo(state,30*gameDayTicks);
    CHECK(!state.factionSimulation.operations.entries().empty()); CHECK(state.npcSimulation.statistics.participations>0);
    CHECK(state.npcSimulation.statistics.travelsStarted>0); CHECK(state.npcSimulation.statistics.relationshipChanges>0);
    CHECK(baseWorldHash(*state.world.base)==base); CHECK(base==0x888c5dbd10c15886ULL);
    CHECK(state.npcSimulation.statistics.travelsStarted==state.npcSimulation.statistics.travelsCompleted+
        state.npcSimulation.statistics.travelsCancelled+static_cast<std::uint64_t>(std::count_if(state.world.npcs.entries().begin(),state.world.npcs.entries().end(),
            [](const auto& entry){return entry.second.semantic.travel.has_value();})));
    const auto bytes=saveGameState(state); auto loaded=loadGameState(bytes); CHECK(stateHash(state)==stateHash(loaded)); CHECK(saveGameState(loaded)==bytes);
    advanceTo(state,31*gameDayTicks); advanceTo(loaded,31*gameDayTicks); CHECK(stateHash(state)==stateHash(loaded));
}
