#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "persistence/schema_v2.hpp"
#include "legacy_save_v1.hpp"
#include <type_traits>
#include <fstream>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
GameState scenario() {
    world::WorldGenerationConfig config; config.width=512; config.depth=512; config.waterPercent=0;
    GameState state{WorldSeed{12345},config,GeneratorVersion{2}};
    auto& s=state.factionSimulation; auto& w=state.world;
    s.config.evaluationIntervalTicks=16; s.config.preparationTicks=4; s.config.activeDurationTicks=12;
    s.config.defensePreparationTicks=2; s.config.attackCooldownTicks=1; s.config.maxForce=32; s.nextEvaluationTick=neverTick;
    world::FactionState faction; faction.treasury=100000; faction.reserveMembers=30; faction.cohesion=8000;
    faction.personality={8000,8000,8000,8000}; faction.name="A"; const auto a=w.factions.create(faction);
    faction.name="B"; const auto b=w.factions.create(faction);
    auto center=std::find_if(s.adjacency.begin(),s.adjacency.end(),[](const auto& pair){return pair.second.size()>=2;});
    if (center==s.adjacency.end()) throw std::logic_error("Scenario requires a connected junction");
    w.factions.get(a).homeTerritory=center->first;
    w.factions.get(b).homeTerritory=center->second.front();
    for (const auto& [id,t] : w.territories.entries()) {
        (void)t; auto& territory=w.territories.get(id); territory.owner=id==center->first ? a : b;
        territory.residentMembers=id==center->first ? 0U : 3U;
    }
    w.factionRelationships={{a,b,1000,8000,3000,1000,0,1},{b,a,1000,8000,3000,1000,0,1}};
    initializeTerritorySimulation(state); validateState(state); return state;
}
TerritoryId target(const GameState& state,std::size_t index=0) {
    return state.factionSimulation.adjacency.at(state.world.factions.get(FactionId{1}).homeTerritory).at(index);
}
OperationId attack(GameState& state,FactionEventBuffer& events,std::uint32_t force=8,std::size_t index=0) {
    const auto result=launchFactionOperation(state,FactionId{1},{OperationType::AttackTerritory,target(state,index),{},force,9999},events);
    if (!result) throw std::logic_error("Scenario attack rejected: "+std::to_string(static_cast<int>(result.rejection)));
    return result.operation;
}
void goTo(GameState& state,std::uint64_t tick) { Simulation sim; sim.advanceHeadless(state,tick-state.clock.tick()); }
void setU64(SaveBytes& bytes,std::size_t offset,std::uint64_t value) {
    for (std::size_t i=0;i<8;++i) bytes.at(offset+i)=static_cast<std::uint8_t>((value>>(8*i))&255U);
}
std::size_t factionSection(const SaveBytes& bytes) {
    persistence::detail::Reader r{bytes}; r.offset=48;
    for (int i=0;i<11;++i) {
        std::uint32_t tag{},size{}; r(tag,size); if (tag==11) return r.offset; r.need(size); r.offset+=size;
    }
    throw std::logic_error("Missing faction section in test");
}
}
static_assert(!std::is_convertible_v<OperationId,FactionId>);
static_assert(!std::is_convertible_v<EntityId,OperationId>);
TEST_CASE("operation IDs are typed monotone and independently allocated") {
    CHECK_FALSE(OperationId{}); CHECK(std::hash<OperationId>{}(OperationId{7})==std::hash<OperationId>{}(OperationId{7}));
    auto state=scenario(); FactionEventBuffer events; const auto a=attack(state,events); cancelFactionOperation(state,a,events);
    goTo(state,1); const auto b=attack(state,events); CHECK(b.value()>a.value()); CHECK(state.world.entities.entries().empty());
}
TEST_CASE("operations reserve real resources and reject manpower or treasury oversubscription") {
    auto state=scenario(); FactionEventBuffer events; const auto before=factionResources(state,FactionId{1});
    const auto money=state.world.factions.get(FactionId{1}).treasury; const auto id=attack(state,events,20);
    auto r=factionResources(state,FactionId{1}); CHECK(r.totalManpower==before.totalManpower); CHECK(r.committedManpower==20);
    CHECK(r.availableManpower==10); CHECK(r.deployableManpower==10); CHECK(state.world.factions.get(FactionId{1}).treasury<money);
    goTo(state,1); const auto hash=stateHash(state);
    const auto rejected=launchFactionOperation(state,FactionId{1},{OperationType::AttackTerritory,target(state,1),{},15,0},events);
    CHECK_FALSE(rejected); CHECK(rejected.rejection==OperationRejection::Manpower); CHECK(stateHash(state)==hash);
    state.world.factions.get(FactionId{1}).treasury=0;
    CHECK(launchFactionOperation(state,FactionId{1},{OperationType::AttackTerritory,target(state,1),{},4,0},events).rejection==OperationRejection::Treasury);
    CHECK(factionResources(state,FactionId{1}).committedManpower==20);
    cancelFactionOperation(state,id,events); CHECK(factionResources(state,FactionId{1}).committedManpower==0);
    CHECK(state.world.factions.get(FactionId{1}).reserveMembers==30); CHECK_NOTHROW(validateState(state));
}
TEST_CASE("attack validation rejects impossible targets and duplicate operations without side effects") {
    auto state=scenario(); FactionEventBuffer events;
    auto candidate=OperationCandidate{OperationType::AttackTerritory,target(state),{},8,0};
    CHECK(launchFactionOperation(state,FactionId{999},candidate,events).rejection==OperationRejection::InvalidFaction);
    candidate.target=TerritoryId{999}; CHECK(launchFactionOperation(state,FactionId{1},candidate,events).rejection==OperationRejection::InvalidTarget);
    candidate.target=state.world.factions.get(FactionId{1}).homeTerritory;
    CHECK(launchFactionOperation(state,FactionId{1},candidate,events).rejection==OperationRejection::OwnTerritory);
    const auto distant=std::find_if(state.world.territories.entries().begin(),state.world.territories.entries().end(),[&](const auto& pair){
        const auto& neighbors=state.factionSimulation.adjacency.at(pair.first);
        return pair.second.owner==FactionId{2} && std::find(neighbors.begin(),neighbors.end(),candidate.target)==neighbors.end();
    });
    REQUIRE(distant!=state.world.territories.entries().end()); candidate.target=distant->first;
    CHECK(launchFactionOperation(state,FactionId{1},candidate,events).rejection==OperationRejection::NotAdjacent);
    (void)attack(state,events); goTo(state,1); candidate.target=target(state);
    const auto hash=stateHash(state); CHECK(launchFactionOperation(state,FactionId{1},candidate,events).rejection==OperationRejection::DuplicateTarget);
    CHECK(stateHash(state)==hash);
}
TEST_CASE("preparing active and terminal transitions preserve resources exactly once") {
    for (const bool victory : {false,true}) {
        auto state=scenario(); FactionEventBuffer events;
        state.world.territories.get(target(state)).residentMembers=victory ? 0U : 10000U;
        const auto before=factionResources(state,FactionId{1}).totalManpower; const auto id=attack(state,events);
        const auto op=state.factionSimulation.operations.get(id);
        CHECK(op.status==OperationStatus::Preparing); CHECK(events.values().back().kind==OperationEventKind::Created);
        goTo(state,op.activeTick); CHECK(state.factionSimulation.operations.get(id).status==OperationStatus::Preparing);
        Simulation sim; sim.tick(state,{state.clock.tick(),{},{}});
        CHECK(state.factionSimulation.operations.get(id).status==OperationStatus::Active);
        CHECK(sim.factionEvents().values().front().kind==OperationEventKind::Started);
        CHECK(factionResources(state,FactionId{1}).committedManpower==8); CHECK_NOTHROW(validateState(state));
        goTo(state,op.resolutionTick+1); const auto& completed=state.factionSimulation.operations.get(id);
        CHECK(completed.status==(victory ? OperationStatus::Resolved : OperationStatus::Failed));
        CHECK(factionResources(state,FactionId{1}).committedManpower==0);
        CHECK(factionResources(state,FactionId{1}).totalManpower==before-completed.result.ownLosses);
        CHECK_THROWS(cancelFactionOperation(state,id,events)); CHECK_NOTHROW(validateState(state));
    }
    for (unsigned from=0;from<5;++from) for (unsigned to=0;to<5;++to) {
        const bool legal=(from==0 && (to==1 || to==4)) || (from==1 && (to==2 || to==3 || to==4));
        CHECK(legalOperationTransition(static_cast<OperationStatus>(from),static_cast<OperationStatus>(to))==legal);
    }
}
TEST_CASE("defense requires one real threat and home defense outranks optional attack") {
    auto state=scenario(); FactionEventBuffer events;
    CHECK(launchFactionOperation(state,FactionId{2},{OperationType::DefendTerritory,target(state),{},8,0},events).rejection==OperationRejection::InvalidThreat);
    const auto a=attack(state,events); const auto candidates=factionCandidates(state,FactionId{2}); REQUIRE_FALSE(candidates.empty());
    CHECK(candidates.front().type==OperationType::DefendTerritory); CHECK(candidates.front().target==state.world.factions.get(FactionId{2}).homeTerritory);
    const auto defense=launchFactionOperation(state,FactionId{2},candidates.front(),events); REQUIRE(defense);
    CHECK(launchFactionOperation(state,FactionId{2},candidates.front(),events).rejection==OperationRejection::InvalidThreat);
    CHECK(factionResources(state,FactionId{2}).committedManpower>0); cancelFactionOperation(state,a,events);
    CHECK(state.factionSimulation.operations.get(defense.operation).status==OperationStatus::Cancelled);
    CHECK(factionResources(state,FactionId{2}).committedManpower==0); CHECK(state.world.factions.get(FactionId{2}).reserveMembers==30);
    CHECK_NOTHROW(validateState(state));
}
TEST_CASE("cancellation of active operations releases survivors once and capacity bounds concurrency") {
    auto state=scenario(); FactionEventBuffer events;
    state.factionSimulation.config.maxConcurrentOperations=1;
    const auto id=attack(state,events); const auto paid=state.world.factions.get(FactionId{1}).treasury;
    goTo(state,5); CHECK(state.factionSimulation.operations.get(id).status==OperationStatus::Active);
    CHECK(launchFactionOperation(state,FactionId{1},{OperationType::AttackTerritory,target(state,1),{},4,0},events).rejection==OperationRejection::Capacity);
    cancelFactionOperation(state,id,events); CHECK(state.world.factions.get(FactionId{1}).reserveMembers==30);
    CHECK(state.world.factions.get(FactionId{1}).treasury==paid); CHECK(factionResources(state,FactionId{1}).activeOperations==0);
    CHECK_THROWS(cancelFactionOperation(state,id,events)); CHECK_NOTHROW(validateState(state));
}
TEST_CASE("active defense participates in aggregate resolution and all survivors release") {
    auto state=scenario(); FactionEventBuffer events; const auto owner=state.world.territories.get(target(state)).owner;
    const auto initial=factionResources(state,FactionId{1}).totalManpower+factionResources(state,FactionId{2}).totalManpower;
    const auto a=attack(state,events); const auto candidates=factionCandidates(state,FactionId{2}); REQUIRE_FALSE(candidates.empty());
    const auto d=launchFactionOperation(state,FactionId{2},candidates.front(),events); REQUIRE(d);
    goTo(state,state.factionSimulation.operations.get(a).resolutionTick+1);
    const auto& attackResult=state.factionSimulation.operations.get(a); const auto& defense=state.factionSimulation.operations.get(d.operation);
    CHECK_FALSE(pending(attackResult.status)); CHECK_FALSE(pending(defense.status)); CHECK(attackResult.status!=defense.status);
    CHECK(factionResources(state,FactionId{1}).committedManpower==0); CHECK(factionResources(state,FactionId{2}).committedManpower==0);
    const auto final=factionResources(state,FactionId{1}).totalManpower+factionResources(state,FactionId{2}).totalManpower;
    CHECK(initial-final==attackResult.result.ownLosses+attackResult.result.opposingLosses);
    CHECK(state.territorySimulation.territories.at(target(state)).garrisonFaction==owner); CHECK_NOTHROW(validateState(state));
}
TEST_CASE("utility depends on hostility and resources not faction RNG draws") {
    auto state=scenario(); const auto a=target(state),b=target(state,1);
    auto third=state.world.factions.get(FactionId{2}); third.homeTerritory=b; const auto c=state.world.factions.create(third);
    state.world.territories.get(b).owner=c; initializeTerritorySimulation(state);
    state.world.factionRelationships={{FactionId{1},FactionId{2},0,10000,0,0,0,1},{FactionId{1},c,10000,0,0,10000,0,1}};
    auto candidates=factionCandidates(state,FactionId{1}); REQUIRE_FALSE(candidates.empty()); CHECK(candidates.front().target!=b);
    const auto hostile=std::find_if(candidates.begin(),candidates.end(),[&](const auto& candidate){return candidate.target==a;});
    const auto friendly=std::find_if(candidates.begin(),candidates.end(),[&](const auto& candidate){return candidate.target==b;});
    REQUIRE(hostile!=candidates.end()); REQUIRE(friendly!=candidates.end()); CHECK(hostile->utility>friendly->utility);
    CHECK(candidates.front().utility>0);
    const auto before=candidates; for (int i=0;i<1000;++i) (void)state.randomness.get(RandomDomain::Factions).nextU64();
    candidates=factionCandidates(state,FactionId{1}); REQUIRE(candidates.size()==before.size());
    for (std::size_t i=0;i<candidates.size();++i) CHECK(candidates[i].target==before[i].target);
    state.world.factions.get(FactionId{1}).treasury=900;
    candidates=factionCandidates(state,FactionId{1}); REQUIRE_FALSE(candidates.empty()); CHECK(candidates.front().manpower==4);
    state.world.factions.get(FactionId{1}).treasury=899; CHECK(factionCandidates(state,FactionId{1}).empty());
}
TEST_CASE("safe coarse advancement exactly matches fixed ticks and scheduler cadence") {
    auto a=scenario(); a.factionSimulation.nextEvaluationTick=16; auto b=a; Simulation fixed,coarse;
    std::uint64_t events=0;
    for (int i=0;i<2000;++i) { fixed.tick(a,{a.clock.tick(),{},{}}); events+=fixed.factionEvents().values().size(); }
    std::uint64_t coarseEvents=0; coarse.advanceHeadless(b,2000,[&](const auto&){++coarseEvents;});
    CHECK(stateHash(a)==stateHash(b)); CHECK(events==coarseEvents); CHECK(saveGameState(a)==saveGameState(b));
    CHECK(a.world.factions.get(FactionId{1}).strategy.evaluations==124); CHECK_NOTHROW(validateState(a));
    const auto entity=b.world.entities.create(); CHECK_THROWS(coarse.advanceHeadless(b,10)); world::destroyEntity(b.world,entity);
    CHECK_THROWS(coarse.advanceHeadless(b,core::SimulationClock::maxTick));
}
TEST_CASE("save mid-operation preserves scheduler allocator RNG and future results") {
    auto a=scenario(); FactionEventBuffer events; const auto id=attack(a,events);
    const auto defenseCandidates=factionCandidates(a,FactionId{2}); REQUIRE_FALSE(defenseCandidates.empty());
    const auto defense=launchFactionOperation(a,FactionId{2},defenseCandidates.front(),events); REQUIRE(defense);
    goTo(a,a.factionSimulation.operations.get(id).activeTick+1); REQUIRE(a.factionSimulation.operations.get(id).status==OperationStatus::Active);
    auto b=loadGameState(saveGameState(a)); CHECK(stateHash(a)==stateHash(b));
    CHECK(b.factionSimulation.operations.get(defense.operation).threat==id);
    CHECK(a.factionSimulation.operations.nextId()==b.factionSimulation.operations.nextId());
    goTo(a,100); goTo(b,100); CHECK(stateHash(a)==stateHash(b)); CHECK(saveGameState(a)==saveGameState(b));
    a.world.factions.get(FactionId{1}).strategy.nextAttackTick=0; b.world.factions.get(FactionId{1}).strategy.nextAttackTick=0;
    CHECK(attack(a,events)==attack(b,events)); CHECK(stateHash(a)==stateHash(b));
}
TEST_CASE("legacy v1 generated worlds migrate without changing identities resources or RNG") {
    for (std::uint32_t version=1;version<=3;++version) {
        world::WorldGenerationConfig config; config.width=512; config.depth=512; config.waterPercent=0;
        GameState old{WorldSeed{12345},config,GeneratorVersion{version}}; old.clock=core::SimulationClock{64};
        for (auto& rng : old.randomness.streams) for (int i=0;i<23;++i) (void)rng.nextU64();
        const auto bytes=test::legacyV1(old); CHECK(readSaveHeader(bytes).saveFormatVersion.value==1);
        auto current=loadGameState(bytes); CHECK(current.metadata.seed==old.metadata.seed); CHECK(current.clock.tick()==64);
        CHECK(world::baseWorldHash(*current.world.base)==world::baseWorldHash(*old.world.base));
        CHECK(current.factionSimulation.operations.nextId()==1); CHECK(current.factionSimulation.operations.entries().empty());
        CHECK(current.factionSimulation.nextEvaluationTick==64+current.factionSimulation.config.evaluationIntervalTicks);
        for (std::size_t i=0;i<7;++i) CHECK(current.randomness.streams[i].state()==old.randomness.streams[i].state());
        for (const auto& [id,f] : old.world.factions.entries()) {
            CHECK(current.world.factions.get(id).name==f.name); CHECK(current.world.factions.get(id).treasury==f.treasury);
            CHECK(current.world.factions.get(id).reserveMembers==f.reserveMembers); CHECK(factionResources(current,id).committedManpower==0);
        }
        CHECK_NOTHROW(validateState(current)); CHECK(readSaveHeader(saveGameState(current)).saveFormatVersion==currentSaveFormatVersion);
    }
}
TEST_CASE("independent frozen binary v1 fixture retains semantic values after migration") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v1_reference.flsave";
    auto state=loadGameStateFile(path); CHECK(state.metadata.seed.value==12345); CHECK(state.metadata.generatorVersion.value==3);
    CHECK(state.clock.tick()==64); CHECK_FALSE(state.world.base); CHECK(state.world.entities.nextId()==4);
    const auto& faction=state.world.factions.get(FactionId{1}); CHECK(faction.name=="Legacy"); CHECK(faction.treasury==123456);
    CHECK(faction.reserveMembers==20); CHECK(faction.cohesion==7000); CHECK(faction.strategy.confidence==5000);
    CHECK(faction.leader==NpcId{1}); CHECK(state.world.npcs.get(NpcId{1}).identity.name=="Legacy Boss");
    CHECK(state.world.npcs.get(NpcId{1}).health.current==75); CHECK(state.world.missions.get(MissionId{1}).issuer==NpcId{1});
    for (const auto& rng : state.randomness.streams) CHECK(rng.state()==core::DeterministicRng::State{1,2,3,4});
    CHECK(state.factionSimulation.nextEvaluationTick==6464); CHECK(state.factionSimulation.operations.entries().empty());
    CHECK(saveGameState(loadGameState(saveGameState(state)))==saveGameState(state));
    std::ifstream input{path,std::ios::binary}; REQUIRE(input.is_open());
    SaveBytes golden; char byte{}; while (input.get(byte)) golden.push_back(static_cast<std::uint8_t>(byte));
    CHECK(test::legacyV1(state)==golden);
}
TEST_CASE("corrupt operations commitments and scheduler fail validation or load") {
    auto state=scenario(); FactionEventBuffer events; const auto id=attack(state,events);
    const auto valid=saveGameState(state); auto bytes=valid; const auto section=factionSection(bytes);
    const auto configBytes=5*8+3*4+4*8;
    setU64(bytes,section+configBytes,core::SimulationClock::maxTick+1); CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=valid; const auto operation=section+configBytes+8+8+4+8;
    setU64(bytes,operation,999); CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    auto bad=state; bad.factionSimulation.operations.get(id).commitment.manpower=1000; CHECK_THROWS_AS((void)saveGameState(bad),SaveError);
    bad=state; bad.factionSimulation.operations.get(id).status=OperationStatus::Resolved; CHECK_THROWS_AS((void)saveGameState(bad),SaveError);
    bad=state; bad.factionSimulation.operations.get(id).resolutionTick=1; CHECK_THROWS(validateState(bad));
    bad=state; bad.world.factions.get(FactionId{1}).strategy.confidence=10001; CHECK_THROWS(validateState(bad));
    bad=state; bad.factionSimulation.pendingOperations.clear(); CHECK_THROWS(validateState(bad)); CHECK(stateHash(state)==stateHash(bad));
    rebuildFactionIndexes(bad); CHECK_NOTHROW(validateState(bad));
}
TEST_CASE("malformed legacy topology is bounded before rebuilding quadratic adjacency") {
    auto malformed=scenario(); const auto record=malformed.world.base->territories.front();
    malformed.world.base->territories.assign(2000,record);
    const auto bytes=test::legacyV1(malformed);
    try { (void)loadGameState(bytes); FAIL("Oversized topology accepted"); }
    catch (const SaveError& error) {
        CHECK(error.code==SaveErrorCode::ValidationFailure);
        CHECK(std::string{error.what()}.find("topology bounds")!=std::string::npos);
    }
}
TEST_CASE("generated worlds act autonomously without diplomacy or major NPC death") {
    GameState a{WorldSeed{12345}},b{WorldSeed{12346}}; const auto original=a.world; const auto initialRandom=a.randomness;
    Simulation sim; sim.advanceHeadless(a,30*gameDayTicks); sim.advanceHeadless(b,30*gameDayTicks);
    CHECK_FALSE(a.factionSimulation.operations.entries().empty()); CHECK(stateHash(a)!=stateHash(b)); CHECK(a.world.entities.entries().empty());
    CHECK(a.territorySimulation.statistics.controlChanges>0);
    for (const auto& [id,n] : original.npcs.entries()) {
        const auto& after=a.world.npcs.get(id);
        CHECK(after.lifecycle==n.lifecycle); CHECK(after.identity==n.identity); CHECK(after.faction==n.faction);
        CHECK(after.roles==n.roles); CHECK(after.major==n.major); CHECK_FALSE(after.activeEntity);
    }
    CHECK(a.npcSimulation.statistics.updates>0); CHECK(!a.factionSimulation.operations.entries().empty());
    CHECK(a.world.factionRelationships==original.factionRelationships);
    for (std::size_t i=0;i<a.randomness.streams.size();++i)
        if (i!=static_cast<std::size_t>(RandomDomain::Factions) && i!=static_cast<std::size_t>(RandomDomain::Npc)) CHECK(a.randomness.streams[i].state()==initialRandom.streams[i].state());
    for (const auto& [id,f] : original.factions.entries()) {
        CHECK(a.world.factions.get(id).treasury<=f.treasury); CHECK(factionResources(a,id).totalManpower<=world::factionManpower(*original.base,id));
        CHECK(factionResources(a,id).totalManpower>0); CHECK(factionResources(a,id).activeOperations<=2);
    }
    CHECK_NOTHROW(validateState(a));
    sim.advanceHeadless(a,335*gameDayTicks); CHECK(a.clock.tick()==365*gameDayTicks); CHECK_NOTHROW(validateState(a));
    const auto bytes=saveGameState(a); CHECK(stateHash(loadGameState(bytes))==stateHash(a));
}
