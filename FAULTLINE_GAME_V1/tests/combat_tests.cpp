#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest.h"
#include "simulation/combat/combat_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include <filesystem>

using namespace faultline;
using namespace faultline::simulation;

namespace {
GameState fixture() {
    GameState s{WorldSeed{12345}};
    createPlayerEntity(s,s.world.npcs.entries().begin()->second.position); return s;
}
NpcId npcFrom(GameState& s,FactionId avoid={}) {
    for(const auto& [id,n]:s.world.npcs.entries()) if(!avoid || n.faction!=avoid) return id;
    return {};
}
EntityId activate(GameState& s,NpcId id,world::WorldPosition p) {
    auto& n=s.world.npcs.get(id); n.semantic.travel.reset(); n.semantic.activity=world::NpcActivity::Idle;n.position=p;
    const auto e=s.world.entities.create();s.world.transforms.set(s.world.entities,e,{p});s.world.health.set(s.world.entities,e,n.health);
    s.world.characterBodies.set(s.world.entities,e,{0,world::defaultNpcMovementSpeedMetersPerSec,world::defaultNpcStanceHeightMeters});
    s.world.npcLinks.set(s.world.entities,e,{id});s.world.spatial.upsert(e,p);n.activeEntity=e;s.player.residents.push_back({id,e});
    std::sort(s.player.residents.begin(),s.player.residents.end());return e;
}
CombatActorRef player(const GameState& s){return {CombatActorKind::Player,s.player.controlledEntity,{}};}
CombatActorRef actor(const GameState& s,NpcId id){return {CombatActorKind::Npc,s.world.npcs.get(id).activeEntity,id};}
world::WorldPosition nearby(const GameState& s,double meters=5){auto p=world::toGlobal(s.world.transforms.get(s.player.controlledEntity).position);p.x+=meters;return world::fromGlobal(p);}
}

TEST_CASE("canonical combat definitions contain pistol shotgun and SMG") {
    CHECK(weaponDefinition(weaponDefinitionId(WeaponArchetype::Pistol)).damage==25);
    CHECK(weaponDefinition(weaponDefinitionId(WeaponArchetype::Shotgun)).damage==45);
    CHECK(weaponDefinition(weaponDefinitionId(WeaponArchetype::Smg)).automatic);
}
TEST_CASE("valid combat damages existing Health once and cooldown rejects repetition") {
    auto s=fixture();const auto n=npcFrom(s);activate(s,n,nearby(s));NpcEventBuffer ne;
    const auto first=resolveCombatIntent(s,{player(s),actor(s,n)},ne);CHECK(first.result==CombatResult::Hit);CHECK(first.damage==25);
    CHECK(s.world.health.get(actor(s,n).entity).current==75.0F);CHECK(s.world.npcs.get(n).health.current==75.0F);
    const auto second=resolveCombatIntent(s,{player(s),actor(s,n)},ne);CHECK(second.result==CombatResult::Cooldown);CHECK(s.world.health.get(actor(s,n).entity).current==75.0F);
}
TEST_CASE("target validation rejects self stale abstract and dead targets") {
    auto s=fixture();const auto n=npcFrom(s);activate(s,n,nearby(s));NpcEventBuffer ne;
    CHECK(resolveCombatIntent(s,{player(s),player(s)},ne).result==CombatResult::SelfTarget);
    auto stale=actor(s,n);stale.entity=EntityId{999999};CHECK(resolveCombatIntent(s,{player(s),stale},ne).result==CombatResult::InvalidTarget);
    world::destroyEntity(s.world,actor(s,n).entity);CHECK(resolveCombatIntent(s,{player(s),{CombatActorKind::Npc,{},n}},ne).result==CombatResult::InvalidTarget);
}
TEST_CASE("range uses global WorldPosition across cell boundaries") {
    auto s=fixture();auto p=world::WorldPosition{{0,0},{127,0,64}};s.world.transforms.get(s.player.controlledEntity).position=p;
    s.world.base.reset();
    const auto n=npcFrom(s);activate(s,n,world::WorldPosition{{1,0},{1,0,64}});NpcEventBuffer ne;
    CHECK(resolveCombatIntent(s,{player(s),actor(s,n)},ne).result==CombatResult::Hit);
    s.clock=core::SimulationClock{10};s.world.transforms.get(actor(s,n).entity).position=world::WorldPosition{{2,0},{1,0,64}};
    CHECK(resolveCombatIntent(s,{player(s),actor(s,n)},ne).result==CombatResult::MissOutOfRange);
}
TEST_CASE("damage death preserves NpcId and removes only transient body") {
    auto s=fixture();NpcId n{};for(const auto& [id,v]:s.world.npcs.entries())if(!v.roles.has(world::NpcRole::FactionLeader)){n=id;break;}
    activate(s,n,nearby(s));s.world.health.get(actor(s,n).entity).current=20;s.world.npcs.get(n).health.current=20;NpcEventBuffer ne;
    const auto result=resolveCombatIntent(s,{player(s),actor(s,n)},ne);CHECK(result.death);CHECK(s.world.npcs.exists(n));CHECK(s.world.npcs.get(n).lifecycle==world::NpcLifecycle::Dead);
    CHECK_FALSE(s.world.npcs.get(n).activeEntity);CHECK(ne.values().back().kind==NpcEventKind::Died);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("player terminal health is persistent and blocks movement") {
    auto s=fixture();const auto attacker=npcFrom(s);activate(s,attacker,nearby(s));
    auto& w=s.combat.weapons.get(s.combat.npcs.at(attacker).equipped);w.definition=weaponDefinitionId(WeaponArchetype::Shotgun);
    s.world.health.get(s.player.controlledEntity).current=40;NpcEventBuffer ne;CHECK(resolveCombatIntent(s,{actor(s,attacker),player(s)},ne).death);
    const auto before=s.world.transforms.get(s.player.controlledEntity).position;world::WorldStreamingSystem streaming;updatePlayerMovement(s,{1,0,0,0},streaming);
    CHECK(s.world.transforms.get(s.player.controlledEntity).position==before);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("reload and cooldown use fixed simulation ticks") {
    auto s=fixture();auto& c=s.combat.player;auto& w=s.combat.weapons.get(c.equipped);w.ammoInMagazine=0;w.reserveAmmo=3;
    requestReload(s,player(s));CHECK(c.reloading);CHECK(c.reloadCompleteTick==30);
    s.clock=core::SimulationClock{29};CombatEventBuffer ce;NpcEventBuffer ne;CharacterAiEventBuffer ai;updateCombat(s,{},ai,ce,ne);CHECK(w.ammoInMagazine==0);
    s.clock=core::SimulationClock{30};updateCombat(s,{},ai,ce,ne);CHECK(w.ammoInMagazine==3);CHECK_FALSE(c.reloading);
}
TEST_CASE("player and Character AI intents reach CombatSystem without applying damage themselves") {
    auto s=fixture();const auto first=npcFrom(s);activate(s,first,nearby(s,2));CombatEventBuffer ce;NpcEventBuffer ne;CharacterAiEventBuffer ai;
    const auto before=s.world.health.get(actor(s,first).entity).current;
    ai.push({{first,actor(s,first).entity,CharacterAiBehavior::AttackIntent,false,{},{}},false,false});
    CHECK(s.world.health.get(actor(s,first).entity).current==before);
    updateCombat(s,{0,0,1,0,false,true},ai,ce,ne);
    REQUIRE_FALSE(ce.empty());CHECK(ce.values().front().intent.attacker.kind==CombatActorKind::Player);
    CHECK(s.world.health.get(actor(s,first).entity).current<before);
}
TEST_CASE("semantic PlayerInputFrame fire is resolved inside the authoritative Simulation tick") {
    auto s=fixture();const auto n=npcFrom(s);activate(s,n,nearby(s,2));Simulation sim;
    const auto before=s.world.health.get(actor(s,n).entity).current;
    sim.tick(s,{s.clock.tick(),{0,0,1,0,false,true},{}});
    REQUIRE_FALSE(sim.combatEvents().empty());CHECK(sim.combatEvents().values().front().result==CombatResult::Hit);
    CHECK(s.world.npcs.get(n).health.current<before);
}
TEST_CASE("authoritative static geometry blocks line of sight") {
    auto s=fixture();const auto n=npcFrom(s);activate(s,n,nearby(s,8));const auto p=world::toGlobal(s.world.transforms.get(s.player.controlledEntity).position);
    REQUIRE_FALSE(s.world.base->water.empty());s.world.base->water.front().boundary={static_cast<std::int32_t>(std::floor(p.x+3)),static_cast<std::int32_t>(std::floor(p.z-1)),2,3};
    NpcEventBuffer ne;const auto ammo=s.combat.weapons.get(s.combat.player.equipped).ammoInMagazine;
    CHECK(resolveCombatIntent(s,{player(s),actor(s,n)},ne).result==CombatResult::MissBlocked);
    CHECK(s.combat.weapons.get(s.combat.player.equipped).ammoInMagazine+1==ammo);
}
TEST_CASE("damaged NPC abstraction and deterministic rematerialization preserve Health") {
    auto s=fixture();const auto n=npcFrom(s);const auto e=activate(s,n,nearby(s));NpcEventBuffer ne;
    CHECK(resolveCombatIntent(s,{player(s),actor(s,n)},ne).result==CombatResult::Hit);CHECK(s.world.health.get(e).current==75.0F);
    world::destroyEntity(s.world,e);CHECK(s.world.npcs.get(n).health.current==75.0F);restorePlayerResidency(s);
    CHECK(s.world.npcs.get(n).activeEntity==e);CHECK(s.world.health.get(e).current==75.0F);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("important faction leader remains mortal and succession uses an existing NPC") {
    auto s=fixture();const auto faction=s.world.factions.entries().begin()->first;const auto leader=s.world.factions.get(faction).leader;
    activate(s,leader,nearby(s));s.world.health.get(actor(s,leader).entity).current=1;s.world.npcs.get(leader).health.current=1;NpcEventBuffer ne;
    CHECK(resolveCombatIntent(s,{player(s),actor(s,leader)},ne).death);CHECK(s.world.npcs.get(leader).lifecycle==world::NpcLifecycle::Dead);
    const auto successor=s.world.factions.get(faction).leader;CHECK(successor);CHECK(successor!=leader);CHECK(s.world.npcs.get(successor).lifecycle==world::NpcLifecycle::Alive);CHECK_NOTHROW(validateState(s));
}
TEST_CASE("same-tick NPC combat resolves in stable NpcId order") {
    auto s=fixture();NpcId a{},b{},target{};FactionId faction{};
    for(const auto& [id,n]:s.world.npcs.entries()) {
        if(!faction){faction=n.faction;a=id;continue;}
        if(n.faction==faction&&!b)b=id;else if(n.faction!=faction&&!target)target=id;
    }
    REQUIRE(a);REQUIRE(b);REQUIRE(target);activate(s,a,nearby(s,2));activate(s,b,nearby(s,3));activate(s,target,nearby(s,4));
    CharacterAiEventBuffer ai;ai.push({{b,actor(s,b).entity,CharacterAiBehavior::AttackIntent,false,{},{}},false,false});
    ai.push({{a,actor(s,a).entity,CharacterAiBehavior::AttackIntent,false,{},{}},false,false});
    CombatEventBuffer ce;NpcEventBuffer ne;updateCombat(s,{},ai,ce,ne);REQUIRE(ce.values().size()==2);
    CHECK(ce.values()[0].intent.attacker.npc<ce.values()[1].intent.attacker.npc);
}
TEST_CASE("combat validation rejects malformed authoritative reload state") {
    auto s=fixture();s.combat.player.reloading=true;s.combat.player.reloadCompleteTick=0;
    CHECK_THROWS(validateCombatSimulation(s));
}
TEST_CASE("save load preserves combat and deterministic continuation") {
    auto a=fixture();const auto n=npcFrom(a);activate(a,n,nearby(a));NpcEventBuffer ne;
    CHECK(resolveCombatIntent(a,{player(a),actor(a,n)},ne).result==CombatResult::Hit);const auto bytes=persistence::saveGameState(a);auto b=persistence::loadGameState(bytes);
    CHECK(stateHash(a)==stateHash(b));CHECK(persistence::saveGameState(b)==bytes);
    restorePlayerResidency(a);restorePlayerResidency(b);
    a.clock=core::SimulationClock{10};b.clock=core::SimulationClock{10};NpcEventBuffer ea,eb;
    const auto ra=resolveCombatIntent(a,{player(a),actor(a,n)},ea);const auto rb=resolveCombatIntent(b,{player(b),actor(b,n)},eb);
    CHECK(ra.result==rb.result);CHECK(stateHash(a)==stateHash(b));CHECK(persistence::saveGameState(a)==persistence::saveGameState(b));
}
TEST_CASE("authentic Phase 18 v11 save migrates deterministically") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v11_phase18.flsave";
    const auto a=persistence::loadGameStateFile(path),b=persistence::loadGameStateFile(path);
    CHECK(a.combat.version==currentCombatStateVersion);CHECK(a.combat.npcs.size()==a.world.npcs.entries().size());
    CHECK(stateHash(a)==stateHash(b));CHECK(persistence::saveGameState(a)==persistence::saveGameState(b));CHECK_NOTHROW(validateState(a));
}
