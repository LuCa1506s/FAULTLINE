#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/market/market_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/runtime/game.hpp"
#include "world/spatial/streaming.hpp"
#include "persistence/schema_v6.hpp"
#include <fstream>
#include "save_compatibility.hpp"
using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
constexpr FactionId a{1},b{2};
constexpr DistrictId district{1};
constexpr TerritoryId territory{1};
GameState fixture() {
    world::WorldState w;
    (void)w.districts.create(); (void)w.territories.create({district,a,4});
    world::FactionState f; f.name="A";f.treasury=100000;f.reserveMembers=40;f.homeTerritory=territory;
    (void)w.factions.create(f);f.name="B";(void)w.factions.create(f);
    for (auto faction : {a,b}) {
        world::NpcState n;n.faction=faction;n.identity.name="Abstract dealer";n.location={district,territory,{}};
        n.roles.add(world::NpcRole::FactionMember);n.roles.add(world::NpcRole::Dealer);(void)w.npcs.create(n);
    }
    GameState s{{WorldSeed{42},GeneratorVersion{3}},core::SimulationClock{},RuntimeRandomState{WorldSeed{42}},std::move(w)};
    s.factionSimulation.nextEvaluationTick=neverTick;
    for (const auto& [id,n] : s.world.npcs.entries()) { (void)n; s.world.npcs.get(id).semantic.nextUpdateTick=neverTick; }
    rebuildNpcSchedule(s);
    auto& m=s.marketSimulation.districts.at(district);m.baselineDemand=1200;
    for (auto& [id,d] : s.marketSimulation.dealers) { (void)id; d.capacity=1000; }
    return s;
}
void hour(GameState& s) { Simulation sim;sim.advanceHeadless(s,s.marketSimulation.nextUpdateTick+1-s.clock.tick()); }
std::uint64_t income(const GameState& s,FactionId faction) {
    std::uint64_t value=0;for (const auto& [id,m] : s.marketSimulation.districts) { (void)id;
        if (m.factions.contains(faction)) value+=m.factions.at(faction).creditedIncome;
    }return value;
}
std::size_t payload(const SaveBytes& bytes,std::uint32_t tag) {
    detail::Reader r{bytes};r.offset=48;
    while (r.remaining()) { std::uint32_t id{},n{};r(id,n);if (id==tag) return r.offset;r.need(n);r.offset+=n; }
    throw std::logic_error("Missing test section");
}
void u32(SaveBytes& bytes,std::size_t p,std::uint32_t v) {
    for (unsigned i=0;i<4;++i) bytes.at(p+i)=static_cast<std::uint8_t>(v>>(8*i));
}
}
TEST_CASE("market runs coarsely and fixed ticks match headless with no random draws") {
    auto s=fixture(),fixed=s;const auto random=s.randomness;Simulation sim;
    sim.advanceHeadless(s,gameHourTicks);CHECK(s.marketSimulation.updates==0);
    sim.advanceHeadless(s,gameHourTicks+1);
    for (std::uint64_t i=0;i<2*gameHourTicks+1;++i) sim.tick(fixed,{fixed.clock.tick(),{},{}});
    CHECK(s.marketSimulation.updates==2);CHECK(stateHash(s)==stateHash(fixed));CHECK(saveGameState(s)==saveGameState(fixed));
    for (std::size_t i=0;i<random.streams.size();++i) CHECK(random.streams[i].state()==s.randomness.streams[i].state());
    const auto hash=stateHash(s);updateMarkets(s);CHECK(stateHash(s)==hash);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("rival dealer retains meaningful market share in physically controlled territory") {
    auto s=fixture();hour(s);const auto& m=s.marketSimulation.districts.at(district);
    CHECK(controlsTerritory(s,a,territory));CHECK(m.supply==1650);
    CHECK(m.factions.at(a).supply==1000);CHECK(m.factions.at(b).supply==650);
    CHECK(m.factions.at(b).shareBps>3000);CHECK(m.factions.at(a).shareBps+m.factions.at(b).shareBps==10000);
    CHECK(income(s,a)>0);CHECK(income(s,b)>0);CHECK(s.world.factions.get(a).treasury==100000+static_cast<std::int64_t>(income(s,a)));
}
TEST_CASE("dealer death arrest injury missing disabled and role loss remove actual contribution") {
    for (auto lifecycle : {world::NpcLifecycle::Dead,world::NpcLifecycle::Arrested,world::NpcLifecycle::Injured,world::NpcLifecycle::Missing}) {
        auto s=fixture();hour(s);const auto before=s.marketSimulation.districts.at(district);const auto money=income(s,a);
        NpcEventBuffer e;setNpcLifecycle(s,NpcId{1},lifecycle,e);hour(s);const auto& after=s.marketSimulation.districts.at(district);
        CHECK(after.supply<before.supply);CHECK(after.factions.at(a).shareBps==0);CHECK(after.factions.at(b).shareBps==10000);
        CHECK(after.price>before.price);CHECK(income(s,a)==money);CHECK(s.marketSimulation.dealers.size()==2);CHECK_NOTHROW(validateState(s));
    }
    for (int mode=0;mode<3;++mode) {
        auto s=fixture();hour(s);
        if (mode==0) s.marketSimulation.dealers.at(NpcId{1}).enabled=false;
        if (mode==1) s.world.npcs.get(NpcId{1}).roles.bits=0;
        if (mode==2) s.world.npcs.get(NpcId{1}).faction=b;
        hour(s);CHECK(s.marketSimulation.districts.at(district).factions.at(a).supply==0);
    }
}
TEST_CASE("active dealer body health and abstract save observe the same economic loss") {
    auto s=fixture();hour(s);const auto body=world::materializeNpc(s.world,NpcId{1});s.world.health.get(body).current=0;
    auto loaded=loadGameState(saveGameState(s));
    s.clock=core::SimulationClock{s.marketSimulation.nextUpdateTick};loaded.clock=s.clock;
    updateMarkets(s);updateMarkets(loaded);CHECK(stateHash(s)==stateHash(loaded));
    CHECK(s.marketSimulation.districts.at(district).factions.at(a).supply==0);
}
TEST_CASE("contested and rival control reduce capacity without assigning market ownership") {
    auto s=fixture();hour(s);const auto secure=s.marketSimulation.districts.at(district).supply;
    auto& t=s.territorySimulation.territories.at(territory);t.entrenchment={{a,150},{b,220}};
    TerritoryEventBuffer e;reevaluateTerritoryControl(s,territory,e);REQUIRE(territoryControl(s,territory).state==TerritoryControlState::Contested);
    hour(s);CHECK(s.marketSimulation.districts.at(district).supply==1000);CHECK(s.marketSimulation.districts.at(district).supply<secure);
    t.entrenchment={{b,1000}};reevaluateTerritoryControl(s,territory,e);hour(s);
    CHECK(controlsTerritory(s,b,territory));CHECK(s.marketSimulation.districts.at(district).factions.at(a).supply==650);
    CHECK(s.marketSimulation.districts.at(district).factions.at(b).supply==1000);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("disruption has bounded decaying memory and survives save continuation") {
    auto s=fixture();hour(s);const auto before=s.marketSimulation.districts.at(district).supply;
    disruptDistrictMarket(s,district,7500);disruptDistrictMarket(s,district,7500);CHECK(s.marketSimulation.districts.at(district).disruptionBps==10000);
    auto loaded=loadGameState(saveGameState(s));hour(s);hour(loaded);
    CHECK(s.marketSimulation.districts.at(district).supply==0);CHECK(s.marketSimulation.districts.at(district).disruptionBps==9750);
    CHECK(stateHash(s)==stateHash(loaded));
    for (int i=0;i<40;++i) { hour(s);hour(loaded); }
    CHECK(s.marketSimulation.districts.at(district).disruptionBps==0);CHECK(s.marketSimulation.districts.at(district).supply==before);
    CHECK(saveGameState(s)==saveGameState(loaded));CHECK_THROWS(disruptDistrictMarket(s,district,10001));
}
TEST_CASE("zero demand and eliminated dealer networks stop income without respawn or ownership changes") {
    auto s=fixture();hour(s);auto& m=s.marketSimulation.districts.at(district);m.demandScaleBps=0;
    const auto initial=income(s,a)+income(s,b);hour(s);CHECK(m.demand==0);CHECK(income(s,a)+income(s,b)==initial);
    m.demandScaleBps=10000;
    for (auto& [id,d] : s.marketSimulation.dealers) { (void)id;d.enabled=false; }
    for (int i=0;i<100;++i) hour(s);
    CHECK(m.supply==0);CHECK(m.price<=m.maximumPrice);CHECK(income(s,a)+income(s,b)==initial);
    CHECK(s.marketSimulation.dealers.size()==2);CHECK(controlsTerritory(s,a,territory));CHECK_NOTHROW(validateState(s));
}
TEST_CASE("income fractional remainders persist and no update or load pays twice") {
    auto s=fixture();s.marketSimulation.rules.intervalTicks=7;s.marketSimulation.nextUpdateTick=7;
    s.marketSimulation.rules.priceResponseDivisor=13;s.marketSimulation.rules.disruptionRecovery=11;
    hour(s);REQUIRE(income(s,a)==0);REQUIRE(s.marketSimulation.districts.at(district).factions.at(a).incomeRemainder>0);
    const auto bytes=saveGameState(s);auto loaded=loadGameState(bytes);CHECK(saveGameState(loaded)==bytes);
    for (int i=0;i<1000;++i) { hour(s);hour(loaded); }
    CHECK(income(s,a)>0);CHECK(stateHash(s)==stateHash(loaded));CHECK(saveGameState(s)==saveGameState(loaded));
    CHECK(s.world.factions.get(a).treasury==100000+static_cast<std::int64_t>(income(s,a)));
}
TEST_CASE("new registration requires real semantic dealers and does not duplicate membership") {
    auto s=fixture();const auto before=factionResources(s,a).totalManpower;
    CHECK_THROWS(registerMarketDealer(s,NpcId{1},territory,100));CHECK_THROWS(registerMarketDealer(s,NpcId{999},territory,100));
    world::NpcState n;n.faction=a;n.location={district,territory,{}};const auto id=s.world.npcs.create(n);initializeNpc(s,id);
    CHECK_THROWS(registerMarketDealer(s,id,territory,100));s.world.npcs.get(id).roles.add(world::NpcRole::Dealer);
    CHECK_THROWS(registerMarketDealer(s,id,territory,10001));registerMarketDealer(s,id,territory,100);
    hour(s);CHECK(s.marketSimulation.districts.at(district).factions.at(a).supply==1100);
    CHECK(factionResources(s,a).totalManpower==before+1); // One real named actor, no extra aggregate members.
}
TEST_CASE("bounded prices approach extremes smoothly and money cannot overflow") {
    auto s=fixture();auto& m=s.marketSimulation.districts.at(district);m.baselineDemand=marketUnitLimit;
    auto& treasury=s.world.factions.get(a).treasury;treasury=std::numeric_limits<std::int64_t>::max()-1;
    for (int i=0;i<100;++i) { const auto old=m.price;hour(s);CHECK(m.price<=m.maximumPrice);CHECK(m.price>=old);CHECK(m.price-old<=25); }
    CHECK(treasury==std::numeric_limits<std::int64_t>::max());CHECK(income(s,a)==1);
    m.demandScaleBps=0;
    for (int i=0;i<100;++i) { hour(s);CHECK(m.price>=m.minimumPrice); }
    CHECK_NOTHROW(validateState(s));
}
TEST_CASE("market runtime hash includes rules assignments modifiers and accounting; invalid saves reject") {
    auto s=fixture();hour(s);const auto hash=stateHash(s);
    auto changed=s;++changed.marketSimulation.rules.priceResponseDivisor;CHECK(stateHash(changed)!=hash);
    changed=s;++changed.marketSimulation.nextUpdateTick;CHECK(stateHash(changed)!=hash);
    changed=s;changed.marketSimulation.dealers.at(NpcId{1}).enabled=false;CHECK(stateHash(changed)!=hash);
    changed=s;++changed.marketSimulation.districts.at(district).disruptionBps;CHECK(stateHash(changed)!=hash);
    changed=s;++changed.marketSimulation.districts.at(district).factions.at(a).incomeRemainder;CHECK(stateHash(changed)!=hash);
    for (int kind=0;kind<9;++kind) {
        changed=s;auto& market=changed.marketSimulation.districts.at(district);
        if (kind==0) market.price=0;
        if (kind==1) market.supply=marketUnitLimit+1;
        if (kind==2) market.factions.at(a).shareBps=10001;
        if (kind==3) changed.marketSimulation.dealers.at(NpcId{1}).territory=TerritoryId{999};
        if (kind==4) changed.marketSimulation.rules.intervalTicks=0;
        if (kind==5) market.factions.at(a).incomeRemainder=marketIncomeDenominator;
        if (kind==6) changed.marketSimulation.nextUpdateTick=s.clock.tick()-1;
        if (kind==7) market.factions.at(a).sold=marketUnitLimit;
        if (kind==8) changed.marketSimulation.districts.erase(district);
        CHECK_THROWS_AS((void)saveGameState(changed),SaveError);
    }
    const auto good=saveGameState(s);auto bytes=good;u32(bytes,payload(bytes,14)+8,0);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=good;u32(bytes,payload(bytes,14)+32,100001);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    bytes=good;u32(bytes,payload(bytes,14)+36,999);CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    for (auto p=payload(good,14);p<good.size();++p) CHECK_THROWS_AS((void)loadGameState(std::span{good}.first(p)),SaveError);
}
TEST_CASE("frozen real v5 migration preserves every old section without retroactive income") {
    std::ifstream f{std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v5_phase11.flsave",std::ios::binary};
    REQUIRE(f.is_open());const SaveBytes bytes{std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{}};
    REQUIRE(readSaveHeader(bytes).saveFormatVersion.value==5);auto s=loadGameState(bytes);
    CHECK(s.clock.tick()==30*gameDayTicks);CHECK(s.marketSimulation.updates==0);CHECK(s.marketSimulation.dealers.size()==5);
    for (const auto& [id,faction] : s.world.factions.entries()) { (void)faction;CHECK(income(s,id)==0); }
    const auto migrated=saveGameState(s);REQUIRE(migrated.size()>bytes.size());
    CHECK(test::preservesHistoricalPayloads(bytes,migrated));
    CHECK(s.marketSimulation.nextUpdateTick==s.clock.tick()+gameHourTicks);
    auto resumed=loadGameState(migrated);Simulation sim;sim.advanceHeadless(s,365*gameDayTicks);sim.advanceHeadless(resumed,365*gameDayTicks);
    CHECK(saveGameState(s)==saveGameState(resumed));CHECK(stateHash(s)==stateHash(resumed));CHECK_NOTHROW(validateState(s));
}
TEST_CASE("standard districts differ and all five existing faction dealers participate without changing base identity") {
    GameState s{WorldSeed{12345}};CHECK(world::baseWorldHash(*s.world.base)==0x888c5dbd10c15886ULL);
    REQUIRE(s.marketSimulation.dealers.size()==5);std::set<std::uint32_t> demands,sensitivities;std::set<FactionId> participants;
    for (const auto& [id,m] : s.marketSimulation.districts) { (void)id;CHECK(m.demand>0);CHECK(m.price<m.maximumPrice);
        demands.insert(m.baselineDemand);sensitivities.insert(m.sensitivity);
        for (const auto& [f,e] : m.factions) if (e.supply) participants.insert(f);
    }
    CHECK(demands.size()>1);CHECK(sensitivities.size()>1);CHECK(participants.size()==5);
    const auto original=s.world;Simulation sim;sim.advanceHeadless(s,30*gameDayTicks);
    for (const auto& [id,f] : s.world.factions.entries()) {
        CHECK(income(s,id)>0);std::uint64_t spent=0;
        for (const auto& [opId,op] : s.factionSimulation.operations.entries()) { (void)opId;if (op.faction==id) spent+=static_cast<std::uint64_t>(op.commitment.paidCost); }
        CHECK(f.treasury==original.factions.get(id).treasury+static_cast<std::int64_t>(income(s,id))-static_cast<std::int64_t>(spent));
    }
    CHECK(world::baseWorldHash(*s.world.base)==0x888c5dbd10c15886ULL);
}
TEST_CASE("five year headless markets stay bounded with daily validation and interrupted continuation") {
    world::WorldGenerationConfig c;c.width=512;c.depth=512;c.waterPercent=0;
    for (std::uint64_t seed=1;seed<=5;++seed) {
        GameState s{WorldSeed{seed},c};Simulation sim;
        for (int day=0;day<5*365;++day) {
            if (day==100) disruptDistrictMarket(s,s.marketSimulation.districts.begin()->first,9000);
            sim.advanceHeadless(s,gameDayTicks);validateState(s);
            if (day==365) s=loadGameState(saveGameState(s));
            for (const auto& [id,m] : s.marketSimulation.districts) { (void)id;CHECK(m.price>=m.minimumPrice);CHECK(m.price<=m.maximumPrice); }
        }
        CHECK(s.marketSimulation.updates==5*365*24-1);CHECK(s.world.entities.entries().empty());
        for (const auto& [id,f] : s.world.factions.entries()) { CHECK(f.treasury>=0);CHECK(income(s,id)>0); }
    }
}

TEST_CASE("market scheduling safely exhausts the clock and invalid late updates fail") {
    auto s=fixture();s.clock=core::SimulationClock{core::SimulationClock::maxTick-10};initializeMarketSimulation(s);
    CHECK(s.marketSimulation.nextUpdateTick==neverTick);CHECK_NOTHROW(validateMarketSimulation(s));
    updateMarkets(s);CHECK(s.marketSimulation.updates==0);
    s=fixture();s.clock=core::SimulationClock{gameHourTicks+1};CHECK_THROWS(updateMarkets(s));
}

TEST_CASE("district supply saturates safely and bounded demand sales conserve all participation units") {
    auto s=fixture();auto& m=s.marketSimulation.districts.at(district);m.baselineDemand=marketUnitLimit;
    for (int i=0;i<110;++i) {
        world::NpcState n;n.faction=a;n.location={district,territory,{}};n.roles.add(world::NpcRole::Dealer);
        const auto id=s.world.npcs.create(n);initializeNpc(s,id);s.world.npcs.get(id).semantic.nextUpdateTick=neverTick;
        registerMarketDealer(s,id,territory,10000);
    }
    rebuildNpcSchedule(s);hour(s);CHECK(m.supply==marketUnitLimit);CHECK_NOTHROW(validateState(s));
    for (std::uint32_t demand=0;demand<=50;++demand) {
        m.baselineDemand=demand;hour(s);CHECK_NOTHROW(validateMarketSimulation(s));
        for (const auto& [f,e] : m.factions) { (void)f;CHECK(e.sold<=e.supply); }
    }
}
