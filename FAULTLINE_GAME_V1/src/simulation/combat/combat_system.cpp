#include "simulation/combat/combat_system.hpp"
#include "simulation/runtime/game.hpp"
#include "simulation/npc/npc_system.hpp"
#include "world/spatial/streaming.hpp"
#include "world/spatial/character_movement.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace faultline::simulation {
namespace {
constexpr std::array<WeaponDefinition,3> definitions{{
    {WeaponDefinitionId{1},WeaponArchetype::Pistol,25,25.0F,12,6,30,false},
    {WeaponDefinitionId{2},WeaponArchetype::Shotgun,45,12.0F,6,20,40,false},
    {WeaponDefinitionId{3},WeaponArchetype::Smg,12,20.0F,30,2,35,true}
}};

std::uint64_t actorKey(CombatActorRef actor) {
    return actor.kind==CombatActorKind::Player ? 0 : actor.npc.value();
}
CombatantState* combatant(GameState& state,CombatActorRef actor) {
    if (actor.kind==CombatActorKind::Player) return &state.combat.player;
    const auto it=state.combat.npcs.find(actor.npc); return it==state.combat.npcs.end()?nullptr:&it->second;
}
const world::Health* health(const GameState& state,CombatActorRef actor) {
    if (!actor.entity || !state.world.entities.exists(actor.entity) || !state.world.health.contains(actor.entity)) return nullptr;
    if (actor.kind==CombatActorKind::Player)
        return actor.entity==state.player.controlledEntity && !state.world.npcLinks.contains(actor.entity) ? &state.world.health.get(actor.entity) : nullptr;
    if (!actor.npc || !state.world.npcs.exists(actor.npc)) return nullptr;
    const auto& npc=state.world.npcs.get(actor.npc);
    if (npc.activeEntity!=actor.entity || !state.world.npcLinks.contains(actor.entity) ||
        state.world.npcLinks.get(actor.entity).npc!=actor.npc) return nullptr;
    return &state.world.health.get(actor.entity);
}
std::optional<CombatActorRef> playerTarget(const GameState& state,const PlayerInputFrame& input) {
    if (!state.player.controlledEntity || (input.aimX==0 && input.aimZ==0)) return std::nullopt;
    const auto origin=world::toGlobal(state.world.transforms.get(state.player.controlledEntity).position,state.world.spatial.cellSize());
    const double length=std::hypot(input.aimX,input.aimZ),ax=input.aimX/length,az=input.aimZ/length;
    std::optional<std::pair<std::pair<double,NpcId>,CombatActorRef>> best;
    for (const auto& [id,npc]:state.world.npcs.entries()) if (npc.activeEntity && npc.lifecycle!=world::NpcLifecycle::Dead) {
        const auto p=world::toGlobal(state.world.transforms.get(npc.activeEntity).position,state.world.spatial.cellSize());
        const double dx=p.x-origin.x,dz=p.z-origin.z,forward=dx*ax+dz*az,side=std::abs(dx*az-dz*ax);
        if (forward>0 && side<=0.75) {
            CombatActorRef ref{CombatActorKind::Npc,npc.activeEntity,id};
            const auto key=std::pair{forward,id}; if (!best || key<best->first) best={{key,ref}};
        }
    }
    return best?std::optional{best->second}:std::nullopt;
}
std::optional<CombatActorRef> npcTarget(const GameState& state,NpcId attacker) {
    const auto& source=state.world.npcs.get(attacker); if (!source.activeEntity) return std::nullopt;
    if (source.roles.has(world::NpcRole::PoliceOfficer)) {
        if (state.player.controlledEntity && state.world.entities.exists(state.player.controlledEntity) &&
            state.world.health.get(state.player.controlledEntity).current>0)
            return CombatActorRef{CombatActorKind::Player,state.player.controlledEntity,{}};
        return std::nullopt;
    }
    if (!source.faction) return std::nullopt;
    const auto a=world::toGlobal(state.world.transforms.get(source.activeEntity).position,state.world.spatial.cellSize());
    std::optional<std::pair<std::pair<double,NpcId>,CombatActorRef>> best;
    for (const auto& [id,npc]:state.world.npcs.entries()) if (id!=attacker && npc.activeEntity && npc.faction && npc.faction!=source.faction && npc.lifecycle==world::NpcLifecycle::Alive) {
        const auto b=world::toGlobal(state.world.transforms.get(npc.activeEntity).position,state.world.spatial.cellSize());
        const double d=std::hypot(b.x-a.x,b.z-a.z); const auto key=std::pair{d,id};
        if (!best || key<best->first) best={{key,{CombatActorKind::Npc,npc.activeEntity,id}}};
    }
    return best?std::optional{best->second}:std::nullopt;
}
void succeedLeader(GameState& state,NpcId victim) {
    const auto faction=state.world.npcs.get(victim).faction; if (!faction || state.world.factions.get(faction).leader!=victim) return;
    NpcId successor;
    for (const auto& [id,n]:state.world.npcs.entries()) if (id!=victim && n.faction==faction && n.lifecycle==world::NpcLifecycle::Alive && n.roles.has(world::NpcRole::FactionMember)) {
        if (!successor) successor=id;
        else {
            const auto& current=state.world.npcs.get(successor);
            if (std::pair{n.capabilities.leadership,id}<std::pair{current.capabilities.leadership,successor}) continue;
            successor=id;
        }
    }
    auto& dead=state.world.npcs.get(victim);
    dead.roles.bits=static_cast<std::uint16_t>(dead.roles.bits & static_cast<std::uint16_t>(~static_cast<std::uint16_t>(world::NpcRole::FactionLeader)));
    dead.authority=world::AuthorityTier::Command;
    state.world.factions.get(faction).leader=successor;
    if (successor) { auto& next=state.world.npcs.get(successor); next.roles.add(world::NpcRole::FactionLeader); next.authority=world::AuthorityTier::Leader; }
}
}

const WeaponDefinition& weaponDefinition(WeaponDefinitionId id) {
    core::require(id && id.value()<=definitions.size(),"Unknown weapon definition"); return definitions.at(static_cast<std::size_t>(id.value()-1));
}
WeaponDefinitionId weaponDefinitionId(WeaponArchetype a) { return WeaponDefinitionId{static_cast<std::uint64_t>(a)+1}; }

void initializeCombatSimulation(GameState& state) {
    state.combat={};
    const auto give=[&](WeaponArchetype archetype) {
        const auto& d=weaponDefinition(weaponDefinitionId(archetype));
        return state.combat.weapons.create({d.id,d.magazineCapacity,static_cast<std::uint16_t>(d.magazineCapacity*3)});
    };
    state.combat.player.equipped=give(WeaponArchetype::Pistol);
    for (const auto& [id,npc]:state.world.npcs.entries()) {
        (void)npc; const auto archetype=static_cast<WeaponArchetype>(id.value()%3);
        state.combat.npcs.emplace(id,CombatantState{give(archetype)});
    }
}

void addNpcCombatant(GameState& state,NpcId npc,WeaponArchetype archetype) {
    core::require(state.world.npcs.exists(npc) && !state.combat.npcs.contains(npc),
                  "Invalid new NPC combatant");
    const auto& d=weaponDefinition(weaponDefinitionId(archetype));
    const auto weapon=state.combat.weapons.create(
        {d.id,d.magazineCapacity,static_cast<std::uint16_t>(d.magazineCapacity*3)});
    state.combat.npcs.emplace(npc,CombatantState{weapon});
}

void requestReload(GameState& state,CombatActorRef actor) {
    auto* c=combatant(state,actor); core::require(c,"Combatant has no loadout");
    auto& w=state.combat.weapons.get(c->equipped); const auto& d=weaponDefinition(w.definition);
    if (!c->reloading && w.ammoInMagazine<d.magazineCapacity && w.reserveAmmo) {
        c->reloading=true; c->reloadCompleteTick=state.clock.tick()+d.reloadTicks;
    }
}

CombatEvent resolveCombatIntent(GameState& state,const CombatIntent& intent,NpcEventBuffer& npcEvents) {
    CombatEvent event{intent,CombatResult::InvalidAttacker,{},0,false,state.clock.tick(),{}, {}}; ++state.combat.statistics.attempts;
    auto* c=combatant(state,intent.attacker); const auto* attackerHealth=health(state,intent.attacker);
    if (!c || !attackerHealth || attackerHealth->current<=0) return event;
    event.weapon=c->equipped; auto& weapon=state.combat.weapons.get(c->equipped); const auto& def=weaponDefinition(weapon.definition);
    if (intent.attacker.entity==intent.target.entity) { event.result=CombatResult::SelfTarget; return event; }
    const auto* targetHealth=health(state,intent.target);
    if (!targetHealth) { event.result=CombatResult::InvalidTarget; return event; }
    if (targetHealth->current<=0) { event.result=CombatResult::DeadTarget; return event; }
    if (c->reloading) { event.result=CombatResult::Reloading; return event; }
    if (state.clock.tick()<c->nextAttackTick) { event.result=CombatResult::Cooldown; return event; }
    if (!weapon.ammoInMagazine) { requestReload(state,intent.attacker); event.result=CombatResult::EmptyMagazine; return event; }
    --weapon.ammoInMagazine; ++state.combat.statistics.shots; c->nextAttackTick=state.clock.tick()+def.cooldownTicks;
    const auto from=state.world.transforms.get(intent.attacker.entity).position,to=state.world.transforms.get(intent.target.entity).position;
    event.attackerPosition=from; event.targetPosition=to;
    const auto a=world::toGlobal(from,state.world.spatial.cellSize()),b=world::toGlobal(to,state.world.spatial.cellSize());
    if (std::hypot(b.x-a.x,b.z-a.z)>def.rangeMeters) { event.result=CombatResult::MissOutOfRange; ++state.combat.statistics.misses; return event; }
    if (!world::hasStaticLineOfSight(state.world,from,to)) { event.result=CombatResult::MissBlocked; ++state.combat.statistics.misses; return event; }
    event.result=CombatResult::Hit; event.damage=def.damage; ++state.combat.statistics.hits; ++state.combat.statistics.damageEvents;
    auto& hp=state.world.health.get(intent.target.entity); hp.current=std::max(0.0F,hp.current-static_cast<float>(def.damage));
    if (intent.target.kind==CombatActorKind::Npc) state.world.npcs.get(intent.target.npc).health=hp;
    if (hp.current<=0) {
        event.death=true; ++state.combat.statistics.deaths;
        if (intent.target.kind==CombatActorKind::Npc) {
            auto& npc=state.world.npcs.get(intent.target.npc); succeedLeader(state,intent.target.npc);
            std::erase_if(state.player.residents,[&](const auto& r){return r.npc==intent.target.npc;});
            world::destroyEntity(state.world,intent.target.entity);
            setNpcLifecycle(state,intent.target.npc,world::NpcLifecycle::Dead,npcEvents);
            npc.health.current=0;
        }
    }
    return event;
}

void updateCombat(GameState& state,const PlayerInputFrame& input,const CharacterAiEventBuffer& aiEvents,CombatEventBuffer& events,NpcEventBuffer& npcEvents) {
    events.clear();
    const auto complete=[&](CombatantState& c) { if (c.reloading && state.clock.tick()>=c.reloadCompleteTick) {
        auto& w=state.combat.weapons.get(c.equipped); const auto& d=weaponDefinition(w.definition);
        const auto amount=std::min<std::uint16_t>(static_cast<std::uint16_t>(d.magazineCapacity-w.ammoInMagazine),w.reserveAmmo);
        w.ammoInMagazine=static_cast<std::uint16_t>(w.ammoInMagazine+amount); w.reserveAmmo=static_cast<std::uint16_t>(w.reserveAmmo-amount);
        c.reloading=false;c.reloadCompleteTick=0;
    }};
    complete(state.combat.player); for (auto& [id,c]:state.combat.npcs) {(void)id;complete(c);}
    if (input.reloadPressed && state.player.controlledEntity) requestReload(state,{CombatActorKind::Player,state.player.controlledEntity,{}});
    std::vector<CombatIntent> intents;
    if (state.player.controlledEntity) {
        const auto& def=weaponDefinition(state.combat.weapons.get(state.combat.player.equipped).definition);
        if (input.shootPressed || (input.shootHeld && def.automatic)) if (auto target=playerTarget(state,input))
            intents.push_back({{CombatActorKind::Player,state.player.controlledEntity,{}},*target});
    }
    for (const auto& e:aiEvents.values()) if (e.intent.behavior==CharacterAiBehavior::AttackIntent ||
        e.intent.behavior==CharacterAiBehavior::PolicePursuit)
        if (auto target=npcTarget(state,e.intent.npc)) intents.push_back({{CombatActorKind::Npc,e.intent.entity,e.intent.npc},*target});
    std::sort(intents.begin(),intents.end(),[](const auto& a,const auto& b){return std::pair{actorKey(a.attacker),actorKey(a.target)}<std::pair{actorKey(b.attacker),actorKey(b.target)};});
    for (const auto& intent:intents) events.push(resolveCombatIntent(state,intent,npcEvents));
}

void validateCombatSimulation(const GameState& state) {
    core::require(state.combat.version==currentCombatStateVersion,"Unsupported combat state version");
    const auto check=[&](const CombatantState& c) {
        core::require(c.equipped && state.combat.weapons.exists(c.equipped),"Invalid equipped weapon");
        const auto& w=state.combat.weapons.get(c.equipped); const auto& d=weaponDefinition(w.definition);
        core::require(w.ammoInMagazine<=d.magazineCapacity,"Weapon magazine overflow");
        core::require(c.reloading==(c.reloadCompleteTick!=0),"Invalid reload state");
    };
    check(state.combat.player);
    for (const auto& [id,c]:state.combat.npcs) {core::require(state.world.npcs.exists(id),"Unknown NPC combatant");check(c);}
}

void appendCombatSimulationHash(core::StateHash& h,const GameState& state) {
    h.u64(state.combat.version);h.u64(state.combat.weapons.nextId());h.u64(state.combat.weapons.entries().size());
    for (const auto& [id,w]:state.combat.weapons.entries()) {h.u64(id.value());h.u64(w.definition.value());h.u64(w.ammoInMagazine);h.u64(w.reserveAmmo);}
    const auto add=[&](const CombatantState& c){h.u64(c.equipped.value());h.u64(c.nextAttackTick);h.u64(c.reloadCompleteTick);h.u64(c.reloading);};
    add(state.combat.player);h.u64(state.combat.npcs.size());for(const auto& [id,c]:state.combat.npcs){h.u64(id.value());add(c);}
    const auto& s=state.combat.statistics;for(auto v:{s.attempts,s.shots,s.hits,s.misses,s.damageEvents,s.deaths})h.u64(v);
}
} // namespace faultline::simulation
