#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/story/story_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/market/market_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/runtime/game.hpp"
#include "persistence/schema_v8.hpp"
#include <fstream>
using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;
namespace {
GameState prepared(std::uint64_t seed=12345) {
    world::WorldGenerationConfig c; c.width=512;c.depth=512;c.waterPercent=0;
    GameState s{WorldSeed{seed},c};auto p=historyPrologueCandidate(s,FactionId{1});p.firstDeliveryCompleted=true;
    beginStrategicHistory(s,p);while (s.history.months<48) advanceStrategicHistory(s);
    return s;
}
GameState ready() { auto s=prepared();initializeStoryGraph(s);return s; }
StoryNodeId node(const GameState& s,StoryKind kind,FactionId faction=FactionId{1}) {
    for (const auto& [id,n] : s.story.nodes.entries()) if (n.kind==kind && n.faction==faction) return id;
    throw std::logic_error("Test node absent");
}
std::size_t payload(const SaveBytes& b) {
    detail::Reader r{b};r.offset=48;
    while (r.remaining()) { std::uint32_t id{},n{};r(id,n);if (id==16) return r.offset;r.need(n);r.offset+=n; }
    throw std::logic_error("Missing story section");
}
}
TEST_CASE("story deterministic projection preserves world history allocators and unrelated random streams") {
    for (std::uint64_t seed=1;seed<=5;++seed) {
        auto s=prepared(seed);const auto before=saveGameState(s);auto other=s;
        initializeStoryGraph(s);initializeStoryGraph(other);
        CHECK(stateHash(s)==stateHash(other));CHECK(saveGameState(s)==saveGameState(other));
        CHECK_NOTHROW(validateState(s));CHECK(s.story.nodes.entries().size()==70);
        for (const auto& [id,n] : s.story.nodes.entries()) {
            CHECK(s.world.factions.exists(n.faction));CHECK(s.world.npcs.exists(n.contact));
            CHECK(n.historyRecord>0);CHECK(storyAvailability(s,id)<=StoryAvailability::Closed);
        }
        repairStoryGraph(s);CHECK(saveGameState(s)==saveGameState(other));
        s.story={};CHECK(saveGameState(s)==before);
    }
}
TEST_CASE("story refuses invalid worlds partial history and destructive reinitialization") {
    GameState s{WorldSeed{1}};CHECK_THROWS(initializeStoryGraph(s));CHECK(s.story.version==0);
    s=prepared();s.world.factions.get(FactionId{1}).leader=NpcId{999999};CHECK_THROWS(initializeStoryGraph(s));
    s=ready();const auto b=saveGameState(s);CHECK_THROWS(initializeStoryGraph(s));CHECK(saveGameState(s)==b);
}
TEST_CASE("story prerequisites create success failure branches and exclude the incompatible continuation") {
    auto s=ready();const auto root=node(s,StoryKind::Contact),boss=node(s,StoryKind::BossOperation);
    CHECK(storyAvailability(s,root)==StoryAvailability::Eligible);
    CHECK(storyAvailability(s,boss)==StoryAvailability::Locked);
    CHECK_THROWS(resolveStoryNode(s,boss,StoryOutcome::Succeeded));
    resolveStoryNode(s,root,StoryOutcome::Succeeded);
    CHECK(storyAvailability(s,root)==StoryAvailability::Closed);CHECK(storyAvailability(s,boss)==StoryAvailability::Eligible);
    CHECK(storyAvailability(s,node(s,StoryKind::MarketChange))==StoryAvailability::Invalidated);
    CHECK_THROWS(resolveStoryNode(s,root,StoryOutcome::Failed));
    auto refused=ready();resolveStoryNode(refused,node(refused,StoryKind::Contact),StoryOutcome::Failed);
    CHECK(storyAvailability(refused,node(refused,StoryKind::BossOperation))==StoryAvailability::Invalidated);
}
TEST_CASE("story rejects malformed structure cycles missing semantic references and enums") {
    const auto original=ready();
    for (unsigned fault=0;fault<12;++fault) {
        auto s=original;const auto id=node(s,StoryKind::BossOperation);auto& n=s.story.nodes.get(id);
        if (fault==0) n.prerequisites={{id,StoryOutcome::Succeeded}};
        if (fault==1) n.prerequisites={{StoryNodeId{9999},StoryOutcome::Succeeded}};
        if (fault==2) n.prerequisites.push_back(n.prerequisites.front());
        if (fault==3) n.prerequisites.front().outcome=StoryOutcome::Unresolved;
        if (fault==4) n.contact=NpcId{9999};
        if (fault==5) n.faction=FactionId{9999};
        if (fault==6) n.kind=static_cast<StoryKind>(255);
        if (fault==7) n.outcome=static_cast<StoryOutcome>(255);
        if (fault==8) (void)s.story.nodes.create(n);
        if (fault==9) s.story.version=99;
        if (fault==10) n.historyRecord=9999;
        if (fault==11) n.prerequisites.clear();
        CHECK_THROWS(validateStoryGraph(s));CHECK_THROWS_AS((void)saveGameState(s),SaveError);
    }
}
TEST_CASE("dead important dealer repairs to an existing candidate or invalidates without resurrection") {
    auto s=ready();const auto id=node(s,StoryKind::DealerConflict);const auto victim=s.story.nodes.get(id).contact;
    REQUIRE(s.world.npcs.get(victim).major);const auto count=s.world.npcs.entries().size(),next=s.world.npcs.nextId();
    NpcEventBuffer events;setNpcLifecycle(s,victim,world::NpcLifecycle::Dead,events);refreshMarkets(s);
    const auto changed=s;repairStoryGraph(s);
    CHECK(s.world.npcs.get(victim).lifecycle==world::NpcLifecycle::Dead);
    CHECK(s.world.npcs.entries().size()==count);CHECK(s.world.npcs.nextId()==next);
    CHECK(s.story.nodes.get(id).contact!=victim);
    REQUIRE(s.story.nodes.get(id).contact);CHECK(s.world.npcs.exists(s.story.nodes.get(id).contact));
    auto worldOnly=s;worldOnly.story=changed.story;CHECK(saveGameState(worldOnly)==saveGameState(changed));
    for (const auto& [npc,d] : s.marketSimulation.dealers) if (d.faction==FactionId{1} &&
        s.world.npcs.get(npc).lifecycle==world::NpcLifecycle::Alive)
        setNpcLifecycle(s,npc,world::NpcLifecycle::Dead,events);
    refreshMarkets(s);repairStoryGraph(s);CHECK_FALSE(s.story.nodes.get(id).contact);
    CHECK(storyAvailability(s,id)==StoryAvailability::Invalidated);CHECK_NOTHROW(validateState(s));
    CHECK(saveGameState(loadGameState(saveGameState(s)))==saveGameState(s));
}
TEST_CASE("story succession observes actual leadership replacement rather than appointing or reviving") {
    auto s=ready();const auto id=node(s,StoryKind::Succession);const auto original=s.world.factions.get(FactionId{1}).leader;
    NpcId successor;
    for (const auto& [npc,n] : s.world.npcs.entries()) if (n.faction==FactionId{1} && n.major && npc!=original) { successor=npc;break; }
    REQUIRE(successor);
    // Explicit semantic fixture for an external organizational decision, not a new succession system.
    s.world.factions.get(FactionId{1}).leader=successor;
    NpcEventBuffer events;setNpcLifecycle(s,original,world::NpcLifecycle::Dead,events);refreshMarkets(s);repairStoryGraph(s);
    CHECK(storyAvailability(s,id)==StoryAvailability::Eligible);
    CHECK(s.world.factions.get(FactionId{1}).leader==successor);
    CHECK(s.world.npcs.get(original).lifecycle==world::NpcLifecycle::Dead);
}
TEST_CASE("story eligibility follows real territory relations faction resources and market facts") {
    auto s=ready();resolveStoryNode(s,node(s,StoryKind::Contact),StoryOutcome::Succeeded);
    const auto id=node(s,StoryKind::TerritorialConflict);const auto n=s.story.nodes.get(id);
    auto& relation=*std::find_if(s.world.factionRelationships.begin(),s.world.factionRelationships.end(),[&](const auto& r){return r.from==n.faction && r.to==n.other;});
    relation.hostility=7000;CHECK(storyAvailability(s,id)==StoryAvailability::Eligible);
    relation.hostility=0;repairStoryGraph(s);CHECK(storyAvailability(s,id)==StoryAvailability::Invalidated);
    relation.hostility=7000;
    // Fixture changes actual presence, then asks its owning system to derive control.
    s.world.territories.get(n.territory).residentMembers=0;
    s.territorySimulation.territories.at(n.territory).entrenchment.clear();
    TerritoryEventBuffer events;reevaluateTerritoryControl(s,n.territory,events);repairStoryGraph(s);
    CHECK(storyAvailability(s,id)==StoryAvailability::Invalidated);
    s.world.factions.get(n.faction).treasury=0;CHECK(storyAvailability(s,node(s,StoryKind::BossOperation))==StoryAvailability::Invalidated);
}
TEST_CASE("story current persistence canonical bytes and runtime continuation preserve resolved branches") {
    auto s=ready();resolveStoryNode(s,node(s,StoryKind::Contact),StoryOutcome::Succeeded);
    const auto bytes=saveGameState(s);auto loaded=loadGameState(bytes);
    CHECK(stateHash(s)==stateHash(loaded));CHECK(saveGameState(loaded)==bytes);
    Simulation sim;sim.advanceHeadless(s,30*gameDayTicks);sim.advanceHeadless(loaded,30*gameDayTicks);
    CHECK(stateHash(s)==stateHash(loaded));CHECK(saveGameState(s)==saveGameState(loaded));CHECK_NOTHROW(validateState(s));
    CHECK(s.story.nodes.get(node(s,StoryKind::Contact)).outcome==StoryOutcome::Succeeded);
}
TEST_CASE("personal relationship events alter betrayal eligibility without story editing social state") {
    auto s=ready();resolveStoryNode(s,node(s,StoryKind::Contact),StoryOutcome::Succeeded);
    const auto id=node(s,StoryKind::Betrayal);const auto contact=s.story.nodes.get(id).contact;
    REQUIRE(contact);auto& npc=s.world.npcs.get(contact);
    npc.traits.ambition=9000;npc.loyalty=1000;s.world.factions.get(npc.faction).cohesion=8000;
    // Start from an explicit neutral personal fixture, then use the actual social authority.
    const auto leader=s.world.factions.get(npc.faction).leader;
    std::erase_if(s.world.npcRelationships,[&](const auto& r){return r.from==contact && r.to==leader;});
    CHECK(storyAvailability(s,id)==StoryAvailability::Invalidated);
    NpcEventBuffer events;
    for (int i=0;i<60;++i) RelationshipSystem::apply(s,{RelationshipEventKind::Threatened,contact,leader},events);
    repairStoryGraph(s);CHECK(storyAvailability(s,id)==StoryAvailability::Eligible);
    CHECK_NOTHROW(validateState(s));
}
TEST_CASE("real phase13 v7 fixture migrates without touching any historical payload") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v7_phase13.flsave";
    std::ifstream file{path,std::ios::binary};REQUIRE(file.is_open());
    const SaveBytes old{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    REQUIRE(readSaveHeader(old).saveFormatVersion.value==7);auto s=loadGameState(old);
    CHECK(s.story.version==0);CHECK(s.story.nodes.entries().empty());
    const auto current=saveGameState(s);CHECK(std::equal(old.begin()+48,old.end(),current.begin()+48));
    auto loaded=loadGameState(current);Simulation sim;sim.advanceHeadless(s,gameDayTicks);sim.advanceHeadless(loaded,gameDayTicks);
    CHECK(saveGameState(s)==saveGameState(loaded));
}
TEST_CASE("story malformed payload truncation oversized counts allocator and dangling graph rejected") {
    auto s=ready();const auto good=saveGameState(s);const auto start=payload(good);
    for (const auto offset : {start,start+12,start+16,start+24,start+25}) {
        auto bytes=good;bytes.at(offset)=255;CHECK_THROWS_AS((void)loadGameState(bytes),SaveError);
    }
    auto allocator=good;std::fill_n(allocator.begin()+static_cast<std::ptrdiff_t>(start+4),8,0);
    allocator.at(start+4)=1;CHECK_THROWS_AS((void)loadGameState(allocator),SaveError);
    // Full story payload truncation sweep, including nested prerequisite arrays.
    for (auto end=start;end<good.size();++end) CHECK_THROWS_AS((void)loadGameState(std::span{good}.first(end)),SaveError);
}
TEST_CASE("completed history v7 wire projection migrates explicitly without inventing story outcomes") {
    const auto s=prepared();auto bytes=saveGameState(s);
    bytes.resize(payload(bytes)-8);bytes[16]=7;bytes[44]=15;
    auto migrated=loadGameState(bytes);CHECK(migrated.story.version==0);
    CHECK(migrated.history==s.history);CHECK(saveGameState(migrated)==saveGameState(s));
    initializeStoryGraph(migrated);CHECK_NOTHROW(validateState(migrated));
}
