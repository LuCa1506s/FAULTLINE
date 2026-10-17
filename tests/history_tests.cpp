#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/history/history_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/runtime/game.hpp"
#include "persistence/schema_v7.hpp"
#include <fstream>
using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
GameState small(std::uint64_t seed=12345) {
    world::WorldGenerationConfig c; c.width=512; c.depth=512; c.waterPercent=0;
    return GameState{WorldSeed{seed},c};
}
void begin(GameState& s,FactionId f=FactionId{1}) {
    auto p=historyPrologueCandidate(s,f); p.firstDeliveryCompleted=true; beginStrategicHistory(s,p);
}
void finish(GameState& s) { while (s.history.months<48) advanceStrategicHistory(s); }
std::size_t payload(const SaveBytes& bytes,std::uint32_t tag) {
    detail::Reader r{bytes};r.offset=48;
    while (r.remaining()) { std::uint32_t id{},n{};r(id,n);if (id==tag) return r.offset;r.need(n);r.offset+=n; }
    throw std::logic_error("Missing test section");
}
void put32(SaveBytes& b,std::size_t p,std::uint32_t n) {
    for (unsigned i=0;i<4;++i) b.at(p+i)=static_cast<std::uint8_t>(n>>(8*i));
}
}
TEST_CASE("history cannot replace first delivery or accept invalid discovery evidence") {
    const auto s=small(); const auto bytes=saveGameState(s);
    for (int kind=0;kind<7;++kind) {
        auto candidate=s; auto p=historyPrologueCandidate(s,FactionId{1});
        if (kind!=0) p.firstDeliveryCompleted=true;
        if (kind==1) p.rival=p.origin;
        if (kind==2) p.witness=NpcId{999};
        if (kind==3) p.location=TerritoryId{999};
        if (kind==4) p.discoveryTick=1;
        if (kind==5) p.deliveryTick=1;
        if (kind==6) p.incident=static_cast<DiscoveryIncident>(255);
        CHECK_THROWS(beginStrategicHistory(candidate,p)); CHECK(saveGameState(candidate)==bytes);
    }
    auto candidate=s; begin(candidate); const auto started=saveGameState(candidate);
    CHECK_THROWS(begin(candidate)); CHECK(saveGameState(candidate)==started);
    Simulation sim; CHECK_THROWS(sim.tick(candidate,{candidate.clock.tick(),{},{}}));
    CHECK(saveGameState(candidate)==started);
}
TEST_CASE("all player selected origins and discovery incident candidates are deterministic") {
    std::set<DiscoveryIncident> incidents; std::set<FactionId> rivals;
    for (std::uint64_t seed=1;seed<=20;++seed) {
        auto s=small(seed);const auto hash=stateHash(s);
        for (std::uint64_t origin=1;origin<=5;++origin) {
            auto p=historyPrologueCandidate(s,FactionId{origin});CHECK(p==historyPrologueCandidate(s,FactionId{origin}));
            CHECK_FALSE(p.firstDeliveryCompleted);CHECK(p.rival!=p.origin);CHECK(s.world.npcs.get(p.witness).faction==p.rival);
            incidents.insert(p.incident);rivals.insert(p.rival);
        }
        CHECK(stateHash(s)==hash);
    }
    CHECK(incidents.size()==6);CHECK(rivals.size()==5);
    for (std::uint64_t origin=1;origin<=5;++origin) {
        auto s=small();begin(s,FactionId{origin});finish(s);CHECK_NOTHROW(validatePostTimeskipWorld(s));
    }
}
TEST_CASE("four year history uses exactly 48 coarse steps and preserves time identity and random streams") {
    auto s=small(); const auto initial=s; const auto base=world::baseWorldHash(*s.world.base);
    begin(s);CHECK(s.history.informed.size()==2);
    CHECK(s.history.events.at(0).kind==HistoryEventKind::FirstDelivery);
    CHECK(s.history.events.at(1).kind==HistoryEventKind::RivalDiscovery);
    for (std::uint32_t month=1;month<=48;++month) {
        advanceStrategicHistory(s);CHECK(s.history.months==month);
        CHECK(s.clock.tick()==static_cast<std::uint64_t>(month)*1460/48*gameDayTicks);
        CHECK(s.history.informed.size()==std::min<std::size_t>(5,month+2));
        CHECK_NOTHROW(validateState(s));
    }
    CHECK(s.clock.tick()==1460*gameDayTicks);CHECK(s.marketSimulation.updates==1460);
    CHECK(world::baseWorldHash(*s.world.base)==base);CHECK(s.metadata.generatorVersion==initial.metadata.generatorVersion);
    CHECK(s.world.entities.entries().empty());CHECK(s.factionSimulation.operations.entries().empty());
    CHECK(s.history.events.size()<1000);CHECK(s.history.events.size()<s.marketSimulation.updates);
    for (std::size_t i=0;i<s.randomness.streams.size();++i) CHECK(s.randomness.streams[i].state()==initial.randomness.streams[i].state());
    CHECK_THROWS(advanceStrategicHistory(s));CHECK_NOTHROW(validatePostTimeskipWorld(s));
}
TEST_CASE("history conserves actual manpower and accounts exactly for all treasury changes") {
    GameState s{WorldSeed{12345}};const auto initial=s;begin(s);finish(s);
    REQUIRE(s.marketSimulation.dealers.size()>5);CHECK(s.world.factionRelationships!=initial.world.factionRelationships);
    CHECK(s.world.npcRelationships!=initial.world.npcRelationships);
    bool settlement=false,dispute=false;
    for (const auto& [id,f] : s.world.factions.entries()) {
        CHECK(factionResources(s,id).totalManpower==factionResources(initial,id).totalManpower);
        std::int64_t cost=0,income=0;
        for (const auto& event : s.history.events) if (event.faction==id) cost+=event.cost;
        for (const auto& [d,m] : s.marketSimulation.districts) { (void)d;
            if (m.factions.contains(id)) income+=static_cast<std::int64_t>(m.factions.at(id).creditedIncome);
        }
        CHECK(cost>0);CHECK(income>0);CHECK(f.treasury==initial.world.factions.get(id).treasury+income-cost);
    }
    for (const auto& e : s.history.events) {
        if (e.kind==HistoryEventKind::DealerExpansion) {
            CHECK(e.cost>=600);CHECK(s.world.npcs.get(e.npc).faction==e.faction);
            CHECK(s.world.npcs.get(e.npc).roles.has(world::NpcRole::Dealer));CHECK_FALSE(s.world.npcs.get(e.npc).major);
            CHECK(s.marketSimulation.dealers.at(e.npc).territory==e.territory);
        }
        if (e.kind==HistoryEventKind::TerritorialSettlement) {
            settlement=true;CHECK(s.world.territories.get(e.territory).residentMembers==6);
            CHECK(s.territorySimulation.territories.at(e.territory).garrisonFaction==e.faction);
        }
        if (e.kind==HistoryEventKind::MarketDispute) dispute=true;
    }
    CHECK(settlement);CHECK(dispute);
    for (const auto& [id,n] : initial.world.npcs.entries()) CHECK(s.world.npcs.get(id).lifecycle==n.lifecycle);
}
TEST_CASE("history interrupted saves at discovery month one year and month 47 continue identically") {
    auto s=small();begin(s);auto uninterrupted=s;finish(uninterrupted);
    for (std::uint32_t stop : {0U,1U,12U,47U}) {
        auto partial=s;while (partial.history.months<stop) advanceStrategicHistory(partial);
        const auto bytes=saveGameState(partial);auto restored=loadGameState(bytes);
        CHECK(stateHash(partial)==stateHash(restored));CHECK(saveGameState(restored)==bytes);
        finish(restored);CHECK(saveGameState(restored)==saveGameState(uninterrupted));
    }
    auto resumed=loadGameState(saveGameState(uninterrupted));Simulation sim;
    sim.advanceHeadless(uninterrupted,30*gameDayTicks);sim.advanceHeadless(resumed,30*gameDayTicks);
    CHECK(stateHash(uninterrupted)==stateHash(resumed));CHECK(saveGameState(uninterrupted)==saveGameState(resumed));
    CHECK_NOTHROW(validateState(resumed));
}
TEST_CASE("history respects custom territory presence limits through checkpoint continuation") {
    for (const auto maximum : {120U,125U,10000U}) {
        CAPTURE(maximum);
        auto s=small();
        s.territorySimulation.rules.maximumPresence=maximum;
        s.territorySimulation.rules.homeEntrenchment=maximum==10000 ? 100U : 0U;
        initializeTerritorySimulation(s);
        REQUIRE_NOTHROW(validateState(s));
        const auto initial=s.territorySimulation.territories;
        begin(s);
        auto uninterrupted=s;finish(uninterrupted);
        for (std::uint32_t month=1;month<=48;++month) {
            REQUIRE_NOTHROW(advanceStrategicHistory(s));
            for (const auto& [t,r] : s.territorySimulation.territories) {
                for (const auto& [f,value] : r.entrenchment) {
                    (void)f;CHECK(value<=maximum);
                }
                for (const auto& [f,value] : initial.at(t).entrenchment)
                    CHECK(r.entrenchment.at(f)>=value);
            }
            if (month==1 || month==12 || month==47) {
                const auto checkpoint=saveGameState(s);
                s=loadGameState(checkpoint);
                CHECK(s.territorySimulation.rules.maximumPresence==maximum);
                CHECK(saveGameState(s)==checkpoint);
            }
        }
        CHECK_NOTHROW(validatePostTimeskipWorld(s));
        CHECK(stateHash(s)==stateHash(uninterrupted));
        CHECK(saveGameState(s)==saveGameState(uninterrupted));
        auto resumed=loadGameState(saveGameState(s));Simulation sim;
        sim.advanceHeadless(s,30*gameDayTicks);sim.advanceHeadless(resumed,30*gameDayTicks);
        CHECK_NOTHROW(validateState(resumed));
        CHECK(saveGameState(s)==saveGameState(resumed));
    }
}
TEST_CASE("history version time ordering and dangling references reject malformed persistence") {
    auto s=small();begin(s);advanceStrategicHistory(s);const auto good=saveGameState(s);
    const auto hash=stateHash(s);
    for (int kind=0;kind<10;++kind) {
        auto broken=s;
        if (kind==0) broken.history.version=99;
        if (kind==1) broken.history.months=49;
        if (kind==2) broken.history.informed.push_back(FactionId{999});
        if (kind==3) broken.history.informed[1]=broken.history.informed[0];
        if (kind==4) broken.history.events.back().territory=TerritoryId{999};
        if (kind==5) broken.history.events.back().kind=static_cast<HistoryEventKind>(255);
        if (kind==6) broken.history.events.back().month=48;
        if (kind==7) broken.history.events.back().cost=-1;
        if (kind==8) broken.history.startTick=neverTick;
        if (kind==9) broken.history.events[0].kind=HistoryEventKind::RivalDiscovery;
        CHECK(stateHash(broken)!=hash);CHECK_THROWS_AS((void)saveGameState(broken),SaveError);
    }
    auto bytes=good;put32(bytes,payload(bytes,15),99);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=good;put32(bytes,payload(bytes,15)+4,49);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    for (auto end=payload(good,15);end<good.size();++end) CHECK_THROWS_AS((void)loadGameState(std::span{good}.first(end)),SaveError);
}
TEST_CASE("real v6 saves migrate without changing fourteen historical sections or inventing history") {
    std::ifstream file{std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v6_phase12.flsave",std::ios::binary};
    REQUIRE(file.is_open());const SaveBytes bytes{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    REQUIRE(readSaveHeader(bytes).saveFormatVersion.value==6);auto s=loadGameState(bytes);
    CHECK(s.history==StrategicHistory{});const auto migrated=saveGameState(s);
    CHECK(std::equal(bytes.begin()+48,bytes.end(),migrated.begin()+48));
    auto restored=loadGameState(migrated);Simulation sim;
    sim.advanceHeadless(s,335*gameDayTicks);sim.advanceHeadless(restored,335*gameDayTicks);
    CHECK(saveGameState(s)==saveGameState(restored));CHECK_NOTHROW(validateState(s));
}
TEST_CASE("history handles nonzero start time and rejects exhausted clock transactionally") {
    auto s=small();s.clock=core::SimulationClock{17};begin(s);finish(s);CHECK(s.clock.tick()==17+1460*gameDayTicks);
    s=small();s.clock=core::SimulationClock{core::SimulationClock::maxTick-100};
    s.factionSimulation.nextEvaluationTick=neverTick;s.marketSimulation.nextUpdateTick=neverTick;
    for (const auto& [id,n] : s.world.npcs.entries()) { (void)n;s.world.npcs.get(id).semantic.nextUpdateTick=neverTick; }
    rebuildNpcSchedule(s);const auto bytes=saveGameState(s);CHECK_THROWS(begin(s));CHECK(saveGameState(s)==bytes);
}
