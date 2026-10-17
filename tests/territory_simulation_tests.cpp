#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "persistence/schema_v4.hpp"
#include "legacy_save_v1.hpp"
#include <fstream>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
constexpr FactionId a{1},b{2};
GameState fixture() {
    world::WorldGenerationConfig config; config.width=512; config.depth=512; config.waterPercent=0;
    GameState s{WorldSeed{12345},config,GeneratorVersion{2}};
    world::FactionState f; f.name="A"; f.treasury=10000000; f.reserveMembers=1000; f.cohesion=8000;
    (void)s.world.factions.create(f); f.name="B"; (void)s.world.factions.create(f);
    const auto x=s.factionSimulation.adjacency.begin()->first;
    const auto y=s.factionSimulation.adjacency.at(x).front();
    s.world.factions.get(a).homeTerritory=x; s.world.factions.get(b).homeTerritory=y;
    s.world.territories.get(x).owner=a; s.world.territories.get(y).owner=b;
    auto& c=s.factionSimulation.config; c.preparationTicks=1; c.activeDurationTicks=10; c.defensePreparationTicks=1;
    c.attackCooldownTicks=1; c.maxForce=32; s.factionSimulation.nextEvaluationTick=neverTick;
    initializeTerritorySimulation(s); validateState(s); return s;
}
TerritoryId target(const GameState& s) { return s.world.factions.get(a).homeTerritory; }
OperationId attack(GameState& s,std::uint32_t force=4) {
    FactionEventBuffer events; const auto r=launchFactionOperation(s,b,{OperationType::AttackTerritory,target(s),{},force,10000},events);
    if (!r) throw std::logic_error("Fixture attack rejected: "+std::to_string(static_cast<unsigned>(r.rejection)));
    return r.operation;
}
void finish(GameState& s,OperationId id) {
    const auto end=s.factionSimulation.operations.get(id).resolutionTick+1; Simulation sim; sim.advanceHeadless(s,end-s.clock.tick());
}
std::size_t payload(const SaveBytes& bytes,std::uint32_t wanted) {
    detail::Reader r{bytes};r.offset=48;
    while (r.remaining()) { std::uint32_t tag{},length{};r(tag,length);if(tag==wanted)return r.offset;r.need(length);r.offset+=length; }
    throw std::logic_error("Missing test section");
}
void u64(SaveBytes& bytes,std::size_t pos,std::uint64_t v) {
    for (std::size_t i=0;i<8;++i) bytes.at(pos+i)=static_cast<std::uint8_t>((v>>(i*8))&255U);
}
SaveBytes historical(const char* name) {
    std::ifstream in{std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures"/name,std::ios::binary};
    if (!in) throw std::logic_error("Historical save missing");
    return {std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};
}
}
TEST_CASE("control thresholds and both hysteresis margins have exact integer boundaries") {
    const TerritoryRules r;
    for (std::uint32_t v : {99U,100U,101U}) {
        const auto c=evaluateTerritoryControl(r,{{a,v}});
        CHECK(c.state==(v<100 ? TerritoryControlState::Uncontrolled : TerritoryControlState::Controlled));
        CHECK(c.controller==(v<100 ? FactionId{} : a));
    }
    for (std::uint32_t second : {59U,60U,61U}) {
        const auto c=evaluateTerritoryControl(r,{{a,100},{b,second}});
        CHECK(c.state==(second<60 ? TerritoryControlState::Controlled : TerritoryControlState::Contested));
    }
    for (std::uint32_t gap : {79U,80U,81U})
        CHECK(evaluateTerritoryControl(r,{{a,100+gap},{b,100}}).state==(gap<80 ? TerritoryControlState::Contested : TerritoryControlState::Controlled));
    for (std::uint32_t gap : {59U,60U,61U})
        CHECK(evaluateTerritoryControl(r,{{a,100+gap},{b,100}},a).state==(gap<60 ? TerritoryControlState::Contested : TerritoryControlState::Controlled));
    CHECK(evaluateTerritoryControl(r,{{a,100},{b,100}},a).state==TerritoryControlState::Contested);
    CHECK(evaluateTerritoryControl(r,{}).controller==FactionId{});
}
TEST_CASE("hysteresis prevents immediate reacquisition after small boundary changes") {
    auto s=fixture(); const auto t=target(s); auto& p=s.territorySimulation.territories.at(t).entrenchment;
    TerritoryEventBuffer events; p={{a,159},{b,100}}; reevaluateTerritoryControl(s,t,events);
    CHECK(territoryControl(s,t).state==TerritoryControlState::Contested); CHECK_FALSE(s.world.territories.get(t).owner);
    p[a]=160; reevaluateTerritoryControl(s,t,events); CHECK(territoryControl(s,t).state==TerritoryControlState::Contested);
    p[a]=179; reevaluateTerritoryControl(s,t,events); CHECK(territoryControl(s,t).state==TerritoryControlState::Contested);
    p[a]=180; reevaluateTerritoryControl(s,t,events); CHECK(controlsTerritory(s,a,t));
    p[a]=160; reevaluateTerritoryControl(s,t,events); CHECK(controlsTerritory(s,a,t));
    CHECK(events.values().size()==2); CHECK_NOTHROW(validateState(s));
}
TEST_CASE("all generated initial ownership maps survive runtime initialization") {
    for (std::uint64_t seed=1;seed<=50;++seed) {
        world::WorldGenerationConfig c;c.width=512;c.depth=512;c.waterPercent=0;
        GameState s{WorldSeed{seed},c};
        for (const auto& assignment : s.world.base->territoryAssignments) {
            CHECK(territoryControl(s,assignment.territory).controller==assignment.owner);
            CHECK(s.world.territories.get(assignment.territory).owner==assignment.owner);
            CHECK(territoryControl(s,assignment.territory).state==(assignment.owner ? TerritoryControlState::Controlled : TerritoryControlState::Uncontrolled));
        }
        CHECK(s.territorySimulation.statistics.transitions==0);
    }
}
TEST_CASE("one successful attack applies causal pressure once without automatically flipping control") {
    auto s=fixture();const auto t=target(s);const auto before=territoryPresence(s,t).at(a);const auto id=attack(s);
    finish(s,id);CHECK(s.factionSimulation.operations.get(id).status==OperationStatus::Resolved);
    CHECK(territoryPresence(s,t).at(a)<before);CHECK(territoryPresence(s,t).at(b)>0);CHECK(controlsTerritory(s,a,t));
    CHECK(s.territorySimulation.appliedOperations.contains(id));
    const auto hash=stateHash(s);FactionOperationEvent event{};event.kind=OperationEventKind::Resolved;event.operation=id;TerritoryEventBuffer events;
    applyTerritoryConsequences(s,std::span{&event,1},events);CHECK(stateHash(s)==hash);CHECK(events.empty());CHECK_NOTHROW(validateState(s));
}
TEST_CASE("repeated resource-backed attacks pass through contest and can change control") {
    auto s=fixture();const auto t=target(s);bool contested=false;unsigned attacks=0;
    while (!controlsTerritory(s,b,t) && attacks<12) {
        const auto id=attack(s);finish(s,id);++attacks;
        if (territoryControl(s,t).state==TerritoryControlState::Contested) contested=true;
        CHECK_NOTHROW(validateState(s));
    }
    CHECK(attacks>1);CHECK(contested);CHECK(controlsTerritory(s,b,t));CHECK(s.world.factions.get(b).treasury<10000000);
    CHECK(s.territorySimulation.statistics.contestedTransitions>0);
    const auto bytes=saveGameState(s);auto loaded=loadGameState(bytes);
    CHECK(territoryControl(loaded,t)==territoryControl(s,t));CHECK(territoryPresence(loaded,t)==territoryPresence(s,t));
    CHECK(saveGameState(loaded)==bytes);CHECK(stateHash(loaded)==stateHash(s));
}
TEST_CASE("strong defense can resolve contested presence in favor of the defender") {
    auto s=fixture();const auto t=target(s);TerritoryEventBuffer changes;
    s.territorySimulation.territories.at(t).entrenchment={{a,150},{b,140}};reevaluateTerritoryControl(s,t,changes);
    REQUIRE(territoryControl(s,t).state==TerritoryControlState::Contested);
    const auto id=attack(s);FactionEventBuffer events;
    const auto d=launchFactionOperation(s,a,{OperationType::DefendTerritory,t,id,32,10000},events);REQUIRE(d);
    finish(s,id);CHECK(s.factionSimulation.operations.get(id).status==OperationStatus::Failed);
    CHECK(s.factionSimulation.operations.get(d.operation).status==OperationStatus::Resolved);
    CHECK(controlsTerritory(s,a,t));CHECK(territoryPresence(s,t).at(a)>150);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("simultaneous attack and defense effects do not emit transient control changes") {
    auto s=fixture();const auto t=target(s);s.territorySimulation.territories.at(t).entrenchment[a]=210;
    TerritoryEventBuffer changes;reevaluateTerritoryControl(s,t,changes);changes.clear();
    const auto id=attack(s,12);FactionEventBuffer events;
    REQUIRE(launchFactionOperation(s,a,{OperationType::DefendTerritory,t,id,4,10000},events));
    const auto end=s.factionSimulation.operations.get(id).resolutionTick+1;
    Simulation sim;std::size_t transitions=0;
    sim.advanceHeadless(s,end-s.clock.tick(),[&](const auto&){transitions=sim.territoryEvents().values().size();});
    CHECK(s.factionSimulation.operations.get(id).status==OperationStatus::Resolved);
    CHECK(controlsTerritory(s,a,t));CHECK(transitions==0);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("failed attacks do not create arbitrary defender entrenchment") {
    auto s=fixture();const auto t=target(s);s.world.territories.get(t).residentMembers=10000;
    const auto before=s.territorySimulation.territories.at(t).entrenchment.at(a);const auto id=attack(s);finish(s,id);
    CHECK(s.factionSimulation.operations.get(id).status==OperationStatus::Failed);
    CHECK(s.territorySimulation.territories.at(t).entrenchment.at(a)==before);CHECK(controlsTerritory(s,a,t));
}
TEST_CASE("neutral expansion costs resources and distant targets cannot teleport control") {
    auto s=fixture();const auto t=target(s);s.world.territories.get(t).owner={};initializeTerritorySimulation(s);
    REQUIRE(territoryControl(s,t).state==TerritoryControlState::Uncontrolled);
    unsigned attempts=0;while (!controlsTerritory(s,b,t) && attempts<8) { const auto id=attack(s);finish(s,id);++attempts; }
    CHECK(attempts>1);CHECK(controlsTerritory(s,b,t));CHECK(s.world.factions.get(b).treasury<10000000);
    TerritoryId distant;for (const auto& [id,rt] : s.world.territories.entries()) { (void)rt;
        if (id!=t && !controlsTerritory(s,b,id)) {
            bool adjacent=false;for(auto neighbor:s.factionSimulation.adjacency.at(id)) adjacent |= controlsTerritory(s,b,neighbor);
            if(!adjacent){distant=id;break;}
        }
    }
    REQUIRE(distant);const auto hash=stateHash(s);FactionEventBuffer events;
    CHECK(launchFactionOperation(s,b,{OperationType::AttackTerritory,distant,{},4,9999},events).rejection==OperationRejection::NotAdjacent);
    CHECK(stateHash(s)==hash);
}
TEST_CASE("garrison affiliation remains independent of control changes and manpower is conserved") {
    auto s=fixture();const auto t=target(s);s.world.territories.get(t).residentMembers=5;
    const auto aBefore=factionResources(s,a).totalManpower,bBefore=factionResources(s,b).totalManpower;
    s.territorySimulation.territories.at(t).entrenchment[b]=1000;TerritoryEventBuffer events;reevaluateTerritoryControl(s,t,events);
    REQUIRE(controlsTerritory(s,b,t));CHECK(s.territorySimulation.territories.at(t).garrisonFaction==a);
    CHECK(territoryGarrison(s,t,b)==0);CHECK(territoryGarrison(s,t,a)==5);
    CHECK(factionResources(s,a).totalManpower==aBefore);CHECK(factionResources(s,b).totalManpower==bBefore);
    CHECK_NOTHROW(validateState(s));CHECK(stateHash(loadGameState(saveGameState(s)))==stateHash(s));
}
TEST_CASE("AI reads changed control and zero-territory factions do not teleport or collapse") {
    auto s=fixture();const auto t=target(s);TerritoryEventBuffer events;
    s.territorySimulation.territories.at(t).entrenchment={{b,500}};reevaluateTerritoryControl(s,t,events);
    CHECK(controlledTerritoryCount(s,a)==0);CHECK(s.world.factions.exists(a));CHECK(factionCandidates(s,a).empty());
    for (const auto& candidate : factionCandidates(s,b)) CHECK(candidate.target!=t);
    FactionEventBuffer opEvents;
    CHECK(launchFactionOperation(s,b,{OperationType::AttackTerritory,t,{},4,9999},opEvents).rejection==OperationRejection::OwnTerritory);
}
TEST_CASE("loss of a launching region cancels stale pending attacks and releases commitments") {
    auto s=fixture();const auto id=attack(s);const auto source=s.world.factions.get(b).homeTerritory;
    s.territorySimulation.territories.at(source).entrenchment={{a,500}};TerritoryEventBuffer changes;reevaluateTerritoryControl(s,source,changes);
    FactionEventBuffer events;reconcileFactionOperations(s,events);
    CHECK(s.factionSimulation.operations.get(id).status==OperationStatus::Cancelled);
    CHECK(factionResources(s,b).committedManpower==0);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("control events occur once per logical transition and contain previous and new control") {
    auto s=fixture();const auto t=target(s);TerritoryEventBuffer events;
    auto& values=s.territorySimulation.territories.at(t).entrenchment;values={{a,150},{b,140}};
    reevaluateTerritoryControl(s,t,events);reevaluateTerritoryControl(s,t,events);REQUIRE(events.values().size()==1);
    CHECK(events.values().front().previous.controller==a);CHECK(events.values().front().current.state==TerritoryControlState::Contested);
    values.clear();reevaluateTerritoryControl(s,t,events);REQUIRE(events.values().size()==2);
    CHECK(events.values().back().previous.state==TerritoryControlState::Contested);CHECK(events.values().back().current.state==TerritoryControlState::Uncontrolled);
}
TEST_CASE("save during contest preserves hysteresis and deterministic future acquisition") {
    auto original=fixture();const auto t=target(original);
    for (unsigned i=0;i<2;++i) {const auto id=attack(original);finish(original,id);}
    REQUIRE(territoryControl(original,t).state==TerritoryControlState::Contested);
    auto loaded=loadGameState(saveGameState(original));CHECK(stateHash(original)==stateHash(loaded));
    while(!controlsTerritory(original,b,t)) {
        const auto x=attack(original),y=attack(loaded);CHECK(x==y);finish(original,x);finish(loaded,y);
        CHECK(stateHash(original)==stateHash(loaded));
        REQUIRE(original.factionSimulation.operations.entries().size()<12);
    }
    CHECK(saveGameState(original)==saveGameState(loaded));
}
TEST_CASE("v1 v2 v3 historical saves migrate without RNG consumption or replaying past results") {
    for (const char* name : {"save_v1_reference.flsave","save_v2_phase9.flsave","save_v3_phase10.flsave"}) {
        const auto bytes=historical(name);auto x=loadGameState(bytes),y=loadGameState(bytes);
        CHECK(stateHash(x)==stateHash(y));CHECK(saveGameState(x)==saveGameState(y));
        const auto migrated=saveGameState(x);
        REQUIRE(migrated.size()>bytes.size());
        CHECK(std::equal(bytes.begin()+48,bytes.end(),migrated.begin()+48));
        CHECK(x.territorySimulation.statistics.transitions==0);
        for (const auto& [id,t] : x.world.territories.entries()) CHECK(territoryControl(x,id).controller==t.owner);
        for (const auto& [id,op] : x.factionSimulation.operations.entries())
            CHECK(x.territorySimulation.appliedOperations.contains(id)==(op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed));
        detail::Reader r{bytes};r.offset=payload(bytes,1)+24;
        for (const auto& rng : x.randomness.streams) {core::DeterministicRng::State words{};r(words);CHECK(rng.state()==words);}
        Simulation sim;sim.advanceHeadless(x,gameDayTicks);y=loadGameState(saveGameState(y));sim.advanceHeadless(y,gameDayTicks);
        CHECK(stateHash(x)==stateHash(y));CHECK_NOTHROW(validateState(x));
    }
}
TEST_CASE("malformed presence controller latch config and consumption records fail explicitly") {
    const auto good=fixture();const auto t=target(good);auto bad=good;
    bad.territorySimulation.territories.at(t).entrenchment[ FactionId{999} ]=100;CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.territories.at(t).entrenchment[a]=10001;CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.territories.at(t).entrenchment[a]=0;CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.territories.at(t).incumbent=b;CHECK_THROWS(validateState(bad));
    bad=good;bad.world.territories.get(t).owner=b;CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.appliedOperations.insert(OperationId{999});CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.rules.holdMargin=bad.territorySimulation.rules.gainMargin;CHECK_THROWS(validateState(bad));
    bad=good;bad.territorySimulation.controlCache.clear();CHECK(stateHash(bad)==stateHash(good));CHECK_THROWS(validateState(bad));
    rebuildTerritoryControlCache(bad);CHECK_NOTHROW(validateState(bad));
    auto bytes=saveGameState(good);const auto p=payload(bytes,13);
    u64(bytes,p+40+32+4,999);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=saveGameState(good);u64(bytes,p+40+32+4+8,999);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=saveGameState(good);std::fill_n(bytes.begin()+static_cast<std::ptrdiff_t>(p+40+32+4+8+8+8+4+8),4,0xFF);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
}
TEST_CASE("initial maps remain stable for the canonical seed and all generator versions") {
    for(std::uint32_t version=1;version<=3;++version) {
        GameState s{WorldSeed{12345},{},GeneratorVersion{version}};
        for(const auto& assignment:s.world.base->territoryAssignments) CHECK(territoryControl(s,assignment.territory).controller==assignment.owner);
        CHECK_NOTHROW(validateState(s));
    }
}

TEST_CASE("a residual foreign garrison cannot hide the new controller from a third attacker") {
    auto s=fixture();const auto t=target(s);s.world.territories.get(t).residentMembers=5;
    s.territorySimulation.territories.at(t).entrenchment[b]=1000;TerritoryEventBuffer events;
    reevaluateTerritoryControl(s,t,events);REQUIRE(controlsTerritory(s,b,t));
    world::FactionState f;f.name="Third faction";f.treasury=10000;f.reserveMembers=30;
    const auto third=s.world.factions.create(f);
    CHECK(territoryDefender(s,t,third)==b);CHECK(territoryGarrison(s,t,b)==0);CHECK(territoryGarrison(s,t,a)==5);
    CHECK_NOTHROW(validateState(s));
}
TEST_CASE("decoder rejects duplicate faction entries and zero/nonbounded presence") {
    auto s=fixture();const auto t=target(s);s.territorySimulation.territories.at(t).entrenchment[b]=1;
    TerritoryEventBuffer events;reevaluateTerritoryControl(s,t,events);const auto valid=saveGameState(s);
    auto bytes=valid;const auto pos=payload(bytes,13);
    u64(bytes,pos+40+32+4+8+8+8+4+12,a.value());
    CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=valid;std::fill_n(bytes.begin()+static_cast<std::ptrdiff_t>(pos+40+32+4+8+8+8+4+8),4,0);
    CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
}

TEST_CASE("FactionSystem resolution alone never writes control or operational foothold") {
    auto s=fixture();const auto t=target(s);const auto id=attack(s);const auto op=s.factionSimulation.operations.get(id);
    const auto before=territoryPresence(s,t);FactionEventBuffer events;
    s.clock=core::SimulationClock{op.activeTick};updateFactionOperations(s,events);events.clear();
    s.clock=core::SimulationClock{op.resolutionTick};updateFactionOperations(s,events);
    REQUIRE(s.factionSimulation.operations.get(id).status==OperationStatus::Resolved);
    CHECK(s.world.territories.get(t).owner==a);CHECK(territoryPresence(s,t)==before);
    TerritoryEventBuffer changes;applyTerritoryConsequences(s,events.values(),changes);
    CHECK(territoryPresence(s,t).contains(b));CHECK_NOTHROW(validateState(s));
}

TEST_CASE("simultaneous outcome order and duplicate delivery preserve canonical state and events") {
    auto original=fixture();const auto t=target(original);
    // A weakened but still controlled foothold makes this paid battle cross a
    // control boundary, so event comparison cannot pass with two empty buffers.
    original.territorySimulation.territories.at(t).entrenchment[a]=150;
    const auto id=attack(original,12);FactionEventBuffer effects;
    REQUIRE(launchFactionOperation(original,a,{OperationType::DefendTerritory,t,id,4,10000},effects));
    const auto op=original.factionSimulation.operations.get(id);
    original.clock=core::SimulationClock{op.activeTick};updateFactionOperations(original,effects);effects.clear();
    original.clock=core::SimulationClock{op.resolutionTick};updateFactionOperations(original,effects);
    REQUIRE(effects.values().size()==2);
    auto reordered=original;
    const auto& outcomes=effects.values();
    const std::vector<FactionOperationEvent> duplicates{outcomes[1],outcomes[0],outcomes[1],outcomes[0]};
    TerritoryEventBuffer forwardEvents,reorderedEvents;
    applyTerritoryConsequences(original,outcomes,forwardEvents);
    applyTerritoryConsequences(reordered,duplicates,reorderedEvents);
    CHECK_NOTHROW(validateState(original));CHECK_NOTHROW(validateState(reordered));
    CHECK(stateHash(original)==stateHash(reordered));
    CHECK(saveGameState(original)==saveGameState(reordered));
    REQUIRE(forwardEvents.values().size()==reorderedEvents.values().size());
    REQUIRE(forwardEvents.values().size()==1);
    const auto& expected=forwardEvents.values().front();const auto& actual=reorderedEvents.values().front();
    CHECK(actual.territory==t);CHECK(actual.territory==expected.territory);
    CHECK(actual.previous==expected.previous);CHECK(actual.current==expected.current);CHECK(actual.tick==expected.tick);
    auto loaded=loadGameState(saveGameState(reordered));const auto hash=stateHash(loaded);TerritoryEventBuffer replay;
    applyTerritoryConsequences(loaded,duplicates,replay);
    CHECK(replay.empty());CHECK(stateHash(loaded)==hash);
}

TEST_CASE("custom territory rules survive contest save load and future paid pressure") {
    auto original=fixture();const auto t=target(original);
    original.territorySimulation.rules={20000,150,90,120,75,30,180,150,15,7};
    initializeTerritorySimulation(original);
    const auto initial=stateHash(original);
    auto defaults=original;defaults.territorySimulation.rules=TerritoryRules{};
    CHECK(stateHash(defaults)!=initial);
    bool contested=false;
    for (unsigned attempts=0;attempts<20 && !controlsTerritory(original,b,t);++attempts) {
        auto loaded=loadGameState(saveGameState(original));
        CHECK(loaded.territorySimulation.rules==original.territorySimulation.rules);
        CHECK(stateHash(loaded)==stateHash(original));
        const auto x=attack(original),y=attack(loaded);CHECK(x==y);
        finish(original,x);finish(loaded,y);
        CHECK(stateHash(original)==stateHash(loaded));CHECK(saveGameState(original)==saveGameState(loaded));
        contested |= territoryControl(original,t).state==TerritoryControlState::Contested;
        CHECK_NOTHROW(validateState(original));
    }
    CHECK(contested);CHECK(controlsTerritory(original,b,t));
}

TEST_CASE("local resistance contests real presence but cannot own or create territory presence") {
    const TerritoryRules r;
    CHECK(evaluateTerritoryControl(r,{},a,10000).state==TerritoryControlState::Uncontrolled);
    CHECK(evaluateTerritoryControl(r,{{a,99}},a,10000).state==TerritoryControlState::Uncontrolled);
    for (std::uint32_t resistance : {140U,141U,10000U}) {
        const auto c=evaluateTerritoryControl(r,{{a,200}},a,resistance);
        CHECK(c.state==(resistance==140 ? TerritoryControlState::Controlled : TerritoryControlState::Contested));
        CHECK(c.controller==(resistance==140 ? a : FactionId{}));
    }
    CHECK(evaluateTerritoryControl(r,{{a,200}},{},120).controller==a);
    CHECK(evaluateTerritoryControl(r,{{a,200}},{},121).state==TerritoryControlState::Contested);
}
TEST_CASE("battle casualties create bounded local opposition and change control without rival footholds") {
    auto s=fixture();const auto t=target(s);
    // A failed costly attack earns no foothold. Resistance alone crosses the
    // incumbent's boundary beyond the battle's separate garrison losses.
    s.world.territories.get(t).residentMembers=10;
    auto& runtime=s.territorySimulation.territories.at(t);
    const auto presence=territoryPresence(s,t).at(a);
    runtime.localResistance=presence-s.territorySimulation.rules.holdMargin-20;
    REQUIRE(controlsTerritory(s,a,t));
    const auto id=attack(s);finish(s,id);const auto& op=s.factionSimulation.operations.get(id);
    REQUIRE(op.status==OperationStatus::Failed);
    const auto casualties=op.result.ownLosses+op.result.opposingLosses;
    REQUIRE(casualties>0);
    CHECK(runtime.localResistance==presence-s.territorySimulation.rules.holdMargin-20+casualties*5);
    CHECK(territoryPresence(s,t).at(a)==presence-op.result.garrisonLosses*s.territorySimulation.rules.garrisonUnit);
    CHECK(evaluateTerritoryControl(s.territorySimulation.rules,territoryPresence(s,t),a,runtime.localResistance-casualties*5).controller==a);
    CHECK_FALSE(territoryPresence(s,t).contains(b));
    CHECK(territoryControl(s,t).state==TerritoryControlState::Contested);
    auto calm=s;calm.territorySimulation.territories.at(t).localResistance=0;
    TerritoryEventBuffer changes;reevaluateTerritoryControl(calm,t,changes);
    CHECK(controlsTerritory(calm,a,t));CHECK(stateHash(s)!=stateHash(calm));
    CHECK_NOTHROW(validateState(s));
    // Saturation is deterministic and cannot overflow even at the configured cap.
    runtime.localResistance=s.territorySimulation.rules.maximumPresence;
    const auto next=attack(s);finish(s,next);
    CHECK(runtime.localResistance==s.territorySimulation.rules.maximumPresence);
    CHECK_NOTHROW(validateState(s));
}
TEST_CASE("successful resource backed defense relieves resistance once and survives continuation") {
    auto original=fixture();const auto t=target(original);
    auto& r=original.territorySimulation.rules;r.resistanceCasualtyUnit=7;r.resistanceReliefUnit=11;
    original.territorySimulation.territories.at(t).localResistance=200;
    TerritoryEventBuffer changes;reevaluateTerritoryControl(original,t,changes);
    REQUIRE(territoryControl(original,t).state==TerritoryControlState::Contested);
    const auto id=attack(original);FactionEventBuffer events;
    const auto defense=launchFactionOperation(original,a,{OperationType::DefendTerritory,t,id,32,10000},events);REQUIRE(defense);
    auto loaded=loadGameState(saveGameState(original));
    CHECK(loaded.territorySimulation.rules==r);
    CHECK(loaded.territorySimulation.territories.at(t).localResistance==200);
    finish(original,id);finish(loaded,id);
    const auto& attackResult=original.factionSimulation.operations.get(id).result;
    const auto& d=original.factionSimulation.operations.get(defense.operation);
    REQUIRE(d.status==OperationStatus::Resolved);
    const auto raised=200+(attackResult.ownLosses+attackResult.opposingLosses)*7;
    const auto relief=(d.commitment.manpower-d.result.ownLosses)*11;
    CHECK(original.territorySimulation.territories.at(t).localResistance==(relief>=raised ? 0U : raised-relief));
    CHECK(controlsTerritory(original,a,t));CHECK(stateHash(original)==stateHash(loaded));
    CHECK(saveGameState(original)==saveGameState(loaded));
    const auto hash=stateHash(loaded);FactionOperationEvent replay{};replay.kind=OperationEventKind::Resolved;replay.operation=defense.operation;
    applyTerritoryConsequences(loaded,std::span{&replay,1},changes);CHECK(stateHash(loaded)==hash);
    // Continue through another real attack after nonzero resistance has round-tripped.
    original.territorySimulation.territories.at(t).localResistance=30;
    reevaluateTerritoryControl(original,t,changes);loaded=loadGameState(saveGameState(original));
    const auto x=attack(original),y=attack(loaded);CHECK(x==y);finish(original,x);finish(loaded,y);
    CHECK(stateHash(original)==stateHash(loaded));CHECK(saveGameState(original)==saveGameState(loaded));
    CHECK_NOTHROW(validateState(loaded));
}
TEST_CASE("real v4 territory save migrates with zero resistance and preserves all prior authority") {
    const auto bytes=historical("save_v4_phase11.flsave");REQUIRE(readSaveHeader(bytes).saveFormatVersion.value==4);
    auto s=loadGameState(bytes);CHECK(s.clock.tick()==30*gameDayTicks);
    CHECK(s.factionSimulation.operations.entries().size()==508);CHECK(s.factionSimulation.pendingOperations.size()==4);
    CHECK(s.territorySimulation.statistics.transitions==50);
    for (const auto& [id,t] : s.territorySimulation.territories) { (void)id; CHECK(t.localResistance==0); }
    const auto migrated=saveGameState(s);CHECK(readSaveHeader(migrated).saveFormatVersion.value==5);
    const auto oldPayload=payload(bytes,13),newPayload=payload(migrated,13);
    REQUIRE(oldPayload==newPayload);
    // Sections 1..12 including every RNG word, and V4's exact territory payload.
    CHECK(std::equal(bytes.begin()+48,bytes.begin()+static_cast<std::ptrdiff_t>(oldPayload-8),migrated.begin()+48));
    CHECK(std::equal(bytes.begin()+static_cast<std::ptrdiff_t>(oldPayload),bytes.end(),migrated.begin()+static_cast<std::ptrdiff_t>(newPayload)));
    auto resumed=loadGameState(migrated);Simulation sim;sim.advanceHeadless(s,gameDayTicks);sim.advanceHeadless(resumed,gameDayTicks);
    CHECK(stateHash(s)==stateHash(resumed));CHECK(saveGameState(s)==saveGameState(resumed));CHECK_NOTHROW(validateState(s));
}
TEST_CASE("resistance and its rules are hashed and malformed save extensions are rejected") {
    auto s=fixture();const auto t=target(s);const auto hash=stateHash(s);
    s.territorySimulation.territories.at(t).localResistance=1;CHECK(stateHash(s)!=hash);
    auto changed=s;++changed.territorySimulation.rules.resistanceCasualtyUnit;CHECK(stateHash(changed)!=stateHash(s));
    changed=s;++changed.territorySimulation.rules.resistanceReliefUnit;CHECK(stateHash(changed)!=stateHash(s));
    changed=s;changed.territorySimulation.territories.at(t).localResistance=10001;CHECK_THROWS(validateState(changed));
    changed=s;changed.territorySimulation.rules.resistanceReliefUnit=1001;CHECK_THROWS(validateState(changed));
    changed=s;changed.territorySimulation.rules.resistanceCasualtyUnit=1001;CHECK_THROWS(validateState(changed));
    auto bytes=saveGameState(s);
    const auto extension=bytes.size()-8-s.territorySimulation.territories.size()*4;
    std::fill_n(bytes.begin()+static_cast<std::ptrdiff_t>(extension),4,0xFF);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=saveGameState(s);std::fill_n(bytes.end()-4,4,0xFF);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=saveGameState(s);bytes.pop_back();CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
}
