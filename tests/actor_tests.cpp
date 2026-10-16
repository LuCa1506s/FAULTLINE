#include "doctest.h"
#include "world/generation/actor_generation.hpp"
#include "world/generation/actor_names.hpp"
#include "world/spatial/streaming.hpp"
#include "simulation/runtime/game.hpp"
#include <algorithm>
#include <set>

using namespace faultline;
using namespace faultline::world;
namespace {
GeneratedWorld withoutActors(GeneratedWorld world) {
    world.metadata.generatorVersion={2}; world.factions.clear(); world.majorNpcs.clear(); world.territoryAssignments.clear();
    world.factionRelationships.clear(); world.npcRelationships.clear(); return world;
}
}
TEST_CASE("M3 keeps M1 and M2 compatibility and urban geometry") {
    const auto m1=generateWorld({{12345},{1},{}}), m2=generateWorld({{12345},{2},{}}), m3=generateWorld({{12345},{3},{}});
    CHECK(baseWorldHash(m1)==0x2938f0ca42162bc9ULL); CHECK(baseWorldHash(m2)==0x5f3a446fe0d2f974ULL);
    CHECK(baseWorldHash(withoutActors(m3))==baseWorldHash(m2));
    CHECK(m3.parcels==m2.parcels); CHECK(m3.buildings==m2.buildings); CHECK(m3.territories==m2.territories);
}
TEST_CASE("M3 seed 12345 reproduces all actors assignments and relationships") {
    const auto a=generateWorld({{12345},{3},{}}), b=generateWorld({{12345},{3},{}});
    CHECK(a.factions==b.factions); CHECK(a.majorNpcs==b.majorNpcs); CHECK(a.territoryAssignments==b.territoryAssignments);
    CHECK(a.factionRelationships==b.factionRelationships); CHECK(a.npcRelationships==b.npcRelationships);
    CHECK(baseWorldHash(a)==0x888c5dbd10c15886ULL); CHECK(baseWorldHash(a)==baseWorldHash(b));
    CHECK(baseWorldHash(a)!=baseWorldHash(generateWorld({{54321},{3},{}})));
    CHECK(validateGeneratedWorld(a).passed());
}
TEST_CASE("canonical family duplicate varies by seed and is never a fifth family") {
    const auto geometry=generateWorld({{12345},{2},{}}); std::set<FactionFamily> duplicated;
    for (std::uint64_t seed=1; seed<=64; ++seed) {
        const auto result=generateFactions({{seed},{3},{}},geometry); REQUIRE(result.factions.size()==5);
        std::array<unsigned,4> counts{}; std::set<std::string> names; std::set<TerritoryId> homes;
        for (const auto& f : result.factions) {CHECK(names.insert(f.state.name).second); CHECK(homes.insert(f.state.homeTerritory).second); ++counts.at(static_cast<std::size_t>(f.state.family));}
        for (std::size_t i=0; i<counts.size(); ++i) if (counts[i]==2) duplicated.insert(static_cast<FactionFamily>(i));
        std::sort(counts.begin(),counts.end()); CHECK(counts==std::array<unsigned,4>{1,1,1,2});
    }
    CHECK(duplicated.size()==4);
}
TEST_CASE("all generated factions have real connected presence and viable leaders") {
    const auto world=generateWorld({{12345},{3},{}}); REQUIRE(world.factions.size()==5);
    std::size_t majorCount=0;
    for (const auto& f : world.factions) {
        CHECK(validateFactionViability(world,f.id).passed());
        std::uint64_t aggregate=f.state.reserveMembers; std::uint64_t named=0;
        for (const auto& a : world.territoryAssignments) if (a.owner==f.id) {CHECK(a.residentMembers>0); aggregate+=a.residentMembers;}
        for (const auto& n : world.majorNpcs) if (n.state.faction==f.id) {
            ++named; CHECK(n.state.major); CHECK(n.state.lifecycle==NpcLifecycle::Alive); CHECK_FALSE(static_cast<bool>(n.state.activeEntity));
            CHECK(n.state.roles.has(NpcRole::FactionMember)); CHECK(n.state.location.territory==f.state.homeTerritory);
        }
        CHECK(named>=3); CHECK(named<=5); majorCount+=static_cast<std::size_t>(named);
        CHECK(aggregate>=70); CHECK(aggregate<=110); CHECK(factionManpower(world,f.id)==aggregate+named);
    }
    CHECK(majorCount==world.majorNpcs.size());
    CHECK(world.factionRelationships.size()==20); CHECK(world.npcRelationships.size()<world.majorNpcs.size()*world.majorNpcs.size()/2);
    bool asymmetric=false;
    for (const auto& r : world.factionRelationships) {
        CHECK(r.historicalGrievance==0); CHECK(r.from!=r.to);
        const auto reverse=std::find_if(world.factionRelationships.begin(),world.factionRelationships.end(),[&](const auto& q){return r.from==q.to && r.to==q.from;});
        REQUIRE(reverse!=world.factionRelationships.end()); asymmetric |= r.fear!=reverse->fear;
    }
    CHECK(asymmetric);
}
TEST_CASE("changing NPC generation cannot mutate earlier generation stages") {
    const auto world=generateWorld({{12345},{3},{}}); const auto before=baseWorldHash(withoutActors(world));
    const auto a=generateMajorNpcs({{12345},{3},{}},world,world.factions);
    const auto b=generateMajorNpcs({{54321},{3},{}},world,world.factions);
    CHECK(a.npcs==world.majorNpcs); CHECK(a.npcs!=b.npcs);
    CHECK(baseWorldHash(withoutActors(world))==before);
    const auto again=generateFactions({{12345},{3},{}},world);
    CHECK(again.assignments==world.territoryAssignments);
    const auto relations=generateInitialRelationships({{12345},{3},{}},world,world.factions,world.territoryAssignments,world.majorNpcs);
    CHECK(relations.factions==world.factionRelationships); CHECK(relations.npcs==world.npcRelationships);
}
TEST_CASE("name collision fallback is bounded and deterministic") {
    const auto family=FactionFamily::ItalianMafia; const auto& pool=actor_detail::names[1];
    std::set<std::string> used;
    for (auto first : pool.given) for (auto last : pool.surname) used.insert(std::string{first}+" "+std::string{last});
    auto copy=used; core::DeterministicRng a{42},b{42};
    const auto first=actor_detail::uniqueName(family,true,a,71,used);
    CHECK(first==actor_detail::uniqueName(family,true,b,71,copy)); CHECK(first.ends_with(" #71"));
    for (auto x : pool.factionFirst) for (auto y : pool.factionLast) used.insert(std::string{x}+" "+std::string{y});
    CHECK(actor_detail::uniqueName(family,false,a,72,used).ends_with(" #72"));
}
TEST_CASE("major NPC counts vary and micro worlds retain a viable five faction base") {
    std::set<std::size_t> counts;
    const auto world=generateWorld({{12345},{3},{}});
    for (std::uint64_t seed=1; seed<=24; ++seed) {
        const auto npcs=generateMajorNpcs({{seed},{3},{}},world,world.factions);
        CHECK(npcs.npcs.size()>=15); CHECK(npcs.npcs.size()<=25); counts.insert(npcs.npcs.size());
    }
    CHECK(counts.size()>1);
    WorldGenerationConfig config; config.width=config.depth=512;
    CHECK(validateGeneratedWorld(generateWorld({{42},{3},config})).passed());
}
TEST_CASE("actor validation rejects corrupt families homes leaders and social records") {
    const auto base=generateWorld({{12345},{3},{}});
    const auto reject=[](const GeneratedWorld& w){CHECK_FALSE(validateActors(w).passed());};
    auto bad=base; bad.factions.pop_back(); reject(bad);
    bad=base; bad.factions[1].id=bad.factions[0].id; reject(bad);
    bad=base; bad.factions[0].state.family=static_cast<FactionFamily>(4); reject(bad);
    bad=base; bad.factions[1].state.name=bad.factions[0].state.name; reject(bad);
    bad=base; bad.factions[1].state.homeTerritory=bad.factions[0].state.homeTerritory; reject(bad);
    bad=base; bad.factions[0].state.leader=NpcId{99999}; reject(bad);
    bad=base; bad.factions[0].state.reserveMembers=0; CHECK_FALSE(validateFactionViability(bad,bad.factions[0].id).passed());
    bad=base; bad.factions[0].state.treasury=-1; reject(bad);
    bad=base; bad.majorNpcs[0].state.faction=FactionId{9999}; reject(bad);
    bad=base; bad.majorNpcs[1].id=bad.majorNpcs[0].id; reject(bad);
    bad=base; bad.majorNpcs[1].state.identity.name=bad.majorNpcs[0].state.identity.name; reject(bad);
    bad=base; bad.majorNpcs[0].state.lifecycle=NpcLifecycle::Dead; reject(bad);
    bad=base; bad.majorNpcs[0].state.activeEntity=EntityId{1}; reject(bad);
    bad=base; bad.majorNpcs[0].state.traits.greed=65535; reject(bad);
    bad=base; bad.majorNpcs[0].state.location.building=BuildingId{9999999}; reject(bad);
    bad=base; bad.majorNpcs[0].state.roles.add(NpcRole::Dealer); reject(bad);
    bad=base; bad.territoryAssignments[0].owner=FactionId{99}; reject(bad);
    bad=base; bad.territoryAssignments.pop_back(); reject(bad);
    bad=base; bad.factionRelationships[0].to=bad.factionRelationships[0].from; reject(bad);
    bad=base; bad.factionRelationships[0].trust=65535; reject(bad);
    bad=base; bad.factionRelationships[0].historicalGrievance=1; reject(bad);
    bad=base; bad.factionRelationships.push_back(bad.factionRelationships[0]); reject(bad);
    bad=base; bad.npcRelationships[0].to=NpcId{99999}; reject(bad);
    bad=base; bad.npcRelationships[0].debt=1; reject(bad);
    bad=base; bad.npcRelationships.clear(); reject(bad);
}
TEST_CASE("runtime imports semantic actors and fingerprints mutable domain data") {
    Game game{WorldSeed{12345}}; const auto& initial=game.state();
    REQUIRE(initial.world.base); CHECK(initial.world.npcs.entries().size()==initial.world.base->majorNpcs.size());
    CHECK(initial.world.factions.entries().size()==5); CHECK(initial.world.entities.entries().empty());
    game.requestValidation(0); CHECK_NOTHROW(game.tick());
    auto state=game.state(); const auto hash=stateHash(state);
    --state.world.factions.get(FactionId{1}).treasury; CHECK(stateHash(state)!=hash);
    state=game.state(); --state.world.npcs.get(NpcId{1}).loyalty; CHECK(stateHash(state)!=hash);
    state=game.state(); --state.world.factionRelationships[0].trust; CHECK(stateHash(state)!=hash);
    state=game.state(); const auto npc=NpcId{1};
    const auto body=materializeNpc(state.world,npc); state.world.health.get(body).current=67;
    destroyEntity(state.world,body); const auto newBody=materializeNpc(state.world,npc);
    CHECK(state.world.npcLinks.get(newBody).npc==npc); CHECK(state.world.health.get(newBody).current==67);
    CHECK_NOTHROW(validateWorld(state.world));
}
TEST_CASE("faction viability rejects detached islands and overwhelming initial control") {
    const auto base=generateWorld({{12345},{3},{}}); auto bad=base;
    const auto graph=territoryAdjacency(base.territories); const auto owner=base.factions.front().id;
    std::map<TerritoryId,FactionId> ownership;
    for (const auto& a : base.territoryAssignments) ownership[a.territory]=a.owner;
    auto island=std::find_if(bad.territoryAssignments.begin(),bad.territoryAssignments.end(),[&](const auto& a){
        return !a.owner && std::none_of(graph.at(a.territory).begin(),graph.at(a.territory).end(),[&](auto id){return ownership.at(id)==owner;});
    });
    REQUIRE(island!=bad.territoryAssignments.end()); island->owner=owner; island->residentMembers=3;
    CHECK_FALSE(validateFactionViability(bad,owner).passed());
    bad=base; for (auto& a : bad.territoryAssignments) {a.owner=owner; a.residentMembers=3;}
    CHECK_FALSE(validateFactionViability(bad,owner).passed());
}
