#include "simulation/npc/npc_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include <algorithm>
#include <sstream>

namespace faultline::simulation {
using namespace world;
namespace {
std::uint64_t after(std::uint64_t tick, std::uint64_t duration) {
    return duration<=core::SimulationClock::maxTick-tick ? tick+duration : neverTick;
}
void increment(std::uint64_t& counter) {
    core::require(counter!=neverTick,"NPC statistics overflow"); ++counter;
}
template<class T, class IdType> const T* find(const std::vector<T>& values, IdType id) {
    const auto it=std::lower_bound(values.begin(),values.end(),id,[](const T& v,IdType key){return v.id<key;});
    return it!=values.end() && it->id==id ? &*it : nullptr;
}
DoubleVec3 locationPoint(const GameState& state, WorldLocation location) {
    core::require(validNpcLocation(state.world,location) && state.world.base.has_value(),"Travel requires valid generated geography");
    if (location.building) {
        const auto* b=find(state.world.base->buildings,location.building);
        return {static_cast<double>(b->transform.centerX2)/2,0,static_cast<double>(b->transform.centerZ2)/2};
    }
    Rect boundary;
    if (location.territory) boundary=find(state.world.base->territories,location.territory)->boundary;
    else boundary=find(state.world.base->districts,location.district)->boundary;
    return {static_cast<double>(boundary.x)+boundary.width/2.0,0,static_cast<double>(boundary.z)+boundary.depth/2.0};
}
NpcActivity restrictedActivity(NpcLifecycle lifecycle) {
    if (lifecycle==NpcLifecycle::Arrested) return NpcActivity::Arrested;
    if (lifecycle==NpcLifecycle::Injured) return NpcActivity::Recovering;
    if (lifecycle==NpcLifecycle::Missing) return NpcActivity::Hiding;
    return NpcActivity::Idle;
}
void activity(GameState& state,NpcId id,NpcActivity value,NpcEventBuffer& events) {
    auto& semantic=state.world.npcs.get(id).semantic;
    if (semantic.activity!=value) {
        semantic.activity=value; events.push({NpcEventKind::ActivityChanged,id,{},semantic.operation,state.clock.tick()});
    }
}
void checkConfig(const NpcSimulationConfig& c) {
    core::require(c.updateIntervalTicks>0 && c.updateIntervalTicks<=gameDayTicks && c.minimumTravelTicks>0 &&
        c.minimumTravelTicks<=gameDayTicks && c.metersPerGameMinute>0 && c.metersPerGameMinute<=10000,"Invalid NPC simulation config");
}
WorldLocation operationLocation(const GameState& state,TerritoryId id) {
    const auto& t=state.world.territories.get(id);
    return {t.district,id,{}};
}
void completeAssignment(GameState& state,NpcId id,NpcEventBuffer& events) {
    auto& n=state.world.npcs.get(id); auto& s=n.semantic;
    const auto& op=state.factionSimulation.operations.get(s.operation);
    if (n.lifecycle==NpcLifecycle::Alive && op.status!=OperationStatus::Cancelled) {
        auto partner=state.world.factions.get(n.faction).leader;
        if (partner==id || !partner || state.world.npcs.get(partner).lifecycle!=NpcLifecycle::Alive) {
            partner={};
            for (const auto& [other,value] : state.world.npcs.entries())
                if (other!=id && value.faction==n.faction && value.lifecycle==NpcLifecycle::Alive) { partner=other; break; }
        }
        if (partner) RelationshipSystem::apply(state,{RelationshipEventKind::WorkedWith,id,partner},events);
    }
    s.operation={};
    if (s.travel) cancelNpcTravel(state,id,events);
    activity(state,id,restrictedActivity(n.lifecycle),events);
}
}
bool validNpcLocation(const WorldState& w,WorldLocation loc) {
    // Foundation/legacy worlds may have no semantic geography at all. In that
    // case an empty domain location denotes the existing normalized WorldPosition,
    // not a missing actor. Generated worlds always require a real district.
    if (!w.base && loc==WorldLocation{}) return true;
    if (!loc.district || !w.districts.exists(loc.district)) return false;
    if (loc.territory && (!w.territories.exists(loc.territory) || w.territories.get(loc.territory).district!=loc.district)) return false;
    if (!w.base) return !loc.building;
    if (!find(w.base->districts,loc.district)) return false;
    if (loc.territory && !find(w.base->territories,loc.territory)) return false;
    if (loc.building) {
        const auto* b=find(w.base->buildings,loc.building);
        if (!b || b->district!=loc.district) return false;
        if (loc.territory && !inside(b->footprint,find(w.base->territories,loc.territory)->boundary)) return false;
    }
    return true;
}
void rebuildNpcSchedule(GameState& state) {
    auto next=neverTick;
    for (const auto& [id,n] : state.world.npcs.entries()) {
        (void)id; next=std::min(next,n.semantic.nextUpdateTick);
        if (n.semantic.travel) next=std::min(next,n.semantic.travel->arrivalTick);
    }
    state.npcSimulation.nextDueTick=next;
}
void initializeNpc(GameState& state,NpcId id) {
    auto& n=state.world.npcs.get(id);
    core::require(!n.semantic.travel && !n.semantic.operation,"NPC initialization cannot discard active semantic commitments");
    n.semantic={}; n.semantic.home=n.location;
    n.semantic.activity=restrictedActivity(n.lifecycle);
    if (n.lifecycle==NpcLifecycle::Alive) n.semantic.nextUpdateTick=after(state.clock.tick(),state.npcSimulation.config.updateIntervalTicks);
    rebuildNpcSchedule(state);
}
void initializeNpcSimulation(GameState& state) {
    checkConfig(state.npcSimulation.config);
    for (const auto& [id,value] : state.world.npcs.entries()) {
        (void)value; auto& n=state.world.npcs.get(id); n.semantic={}; n.semantic.home=n.location;
        n.semantic.activity=restrictedActivity(n.lifecycle);
        if (n.lifecycle==NpcLifecycle::Alive) n.semantic.nextUpdateTick=after(state.clock.tick(),state.npcSimulation.config.updateIntervalTicks);
    }
    rebuildNpcSchedule(state);
}
std::uint64_t nextNpcDueTick(const GameState& state) { return state.npcSimulation.nextDueTick; }
std::uint64_t npcTravelDuration(const GameState& state,WorldLocation from,WorldLocation to) {
    checkConfig(state.npcSimulation.config);
    core::require(from!=to,"Travel destination equals origin");
    const auto a=locationPoint(state,from),b=locationPoint(state,to);
    // Conservative semantic road estimate: Manhattan distance * 1.25, rounded up
    // in half-meter units. No physical route/cache becomes persistent authority.
    const auto halfMeters=static_cast<std::uint64_t>(std::ceil((std::abs(a.x-b.x)+std::abs(a.z-b.z))*2));
    const auto denominator=static_cast<std::uint64_t>(state.npcSimulation.config.metersPerGameMinute)*8*3;
    // One game minute = 80/3 fixed ticks, so duration = distance*5/4*80/(3*speed).
    const auto ticks=(halfMeters*5*80+denominator-1)/denominator;
    return std::max(state.npcSimulation.config.minimumTravelTicks,ticks);
}
void startNpcTravel(GameState& state,NpcId id,WorldLocation destination,NpcEventBuffer& events) {
    auto& n=state.world.npcs.get(id);
    core::require(n.lifecycle==NpcLifecycle::Alive && !n.activeEntity && !n.semantic.travel,"NPC cannot start semantic travel");
    const auto duration=npcTravelDuration(state,n.location,destination);
    const auto arrival=after(state.clock.tick(),duration);
    core::require(arrival!=neverTick,"NPC travel exceeds clock range");
    n.semantic.travel=NpcTravelState{n.location,destination,state.clock.tick(),arrival};
    activity(state,id,NpcActivity::Travelling,events); increment(state.npcSimulation.statistics.travelsStarted);
    events.push({NpcEventKind::TravelStarted,id,{},n.semantic.operation,state.clock.tick()}); rebuildNpcSchedule(state);
}
void cancelNpcTravel(GameState& state,NpcId id,NpcEventBuffer& events) {
    auto& n=state.world.npcs.get(id);
    core::require(n.semantic.travel.has_value(),"NPC has no travel to cancel");
    // Origin remains authoritative until arrival. Interruption never teleports.
    n.semantic.travel.reset(); activity(state,id,restrictedActivity(n.lifecycle),events);
    increment(state.npcSimulation.statistics.travelsCancelled);
    events.push({NpcEventKind::TravelCancelled,id,{},n.semantic.operation,state.clock.tick()}); rebuildNpcSchedule(state);
}
void setNpcLifecycle(GameState& state,NpcId id,NpcLifecycle lifecycle,NpcEventBuffer& events) {
    auto& n=state.world.npcs.get(id);
    core::require(lifecycle<=NpcLifecycle::Dead,"Invalid NPC lifecycle");
    core::require(n.lifecycle!=NpcLifecycle::Dead || lifecycle==NpcLifecycle::Dead,"Dead NPC cannot revive");
    for (const auto& [fid,f] : state.world.factions.entries()) {
        (void)fid; core::require(f.leader!=id || lifecycle==NpcLifecycle::Alive,"Leadership vacuum requires a future explicit system");
    }
    core::require(!n.activeEntity || lifecycle==NpcLifecycle::Alive || lifecycle==NpcLifecycle::Injured,"Abstractify NPC before restrictive lifecycle transition");
    if (n.semantic.travel) cancelNpcTravel(state,id,events);
    n.lifecycle=lifecycle; n.semantic.operation={}; activity(state,id,restrictedActivity(lifecycle),events);
    n.semantic.nextUpdateTick=lifecycle==NpcLifecycle::Alive ? after(state.clock.tick(),state.npcSimulation.config.updateIntervalTicks) : neverTick;
    rebuildNpcSchedule(state);
}
void RelationshipSystem::apply(GameState& state,RelationshipEvent event,NpcEventBuffer& events) {
    core::require(event.kind<=RelationshipEventKind::Abandoned && event.from!=event.to &&
        state.world.npcs.exists(event.from) && state.world.npcs.exists(event.to),"Invalid relationship event");
    auto& records=state.world.npcRelationships;
    auto it=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.from==event.from && r.to==event.to;});
    if (it==records.end()) {
        records.push_back({event.from,event.to,5000,0,0,5000,0,NpcRelationContext::MeaningfulInteraction}); it=std::prev(records.end());
    }
    const auto before=*it;
    const auto adjust=[](std::uint16_t& value,int delta){ value=static_cast<std::uint16_t>(std::clamp(static_cast<int>(value)+delta,0,10000)); };
    switch (event.kind) {
    case RelationshipEventKind::Helped: adjust(it->trust,120); adjust(it->personalLoyalty,80); adjust(it->debt,100); break;
    case RelationshipEventKind::Threatened: adjust(it->fear,120); adjust(it->hostility,100); adjust(it->trust,-120); break;
    case RelationshipEventKind::WorkedWith: adjust(it->trust,30); adjust(it->personalLoyalty,20); adjust(it->hostility,-10); break;
    case RelationshipEventKind::Abandoned: adjust(it->trust,-150); adjust(it->personalLoyalty,-100); adjust(it->hostility,80); break;
    }
    if (*it!=before) {
        increment(state.npcSimulation.statistics.relationshipChanges);
        events.push({NpcEventKind::RelationshipChanged,event.from,event.to,{},state.clock.tick()});
    }
}
NpcId promoteNpc(GameState& state,EphemeralActor& actor,NpcEventBuffer& events) {
    core::require(!actor.promotedTo,"Ephemeral actor already promoted");
    auto n=actor.state;
    core::require(!n.major && !n.roles.has(NpcRole::FactionLeader),"Promotion cannot invent major leadership");
    core::require(n.lifecycle==NpcLifecycle::Alive && validNpcLocation(state.world,n.location) && !n.identity.name.empty(),"Invalid ephemeral semantic state");
    const auto priorActivity=n.semantic.activity;
    core::require(!n.semantic.travel && !n.semantic.operation && (priorActivity==NpcActivity::Idle || priorActivity==NpcActivity::Working ||
        priorActivity==NpcActivity::Meeting || priorActivity==NpcActivity::Hiding),"Invalid ephemeral activity");
    core::require(static_cast<bool>(n.faction)==actor.fromFactionReserve,"Faction promotion must transfer an aggregate reserve member");
    // Promotion is rare and transactional: validate a candidate before committing
    // allocator/resource/link changes, including allocation exhaustion failures.
    auto candidate=state;
    if (n.faction) {
        core::require(candidate.world.factions.exists(n.faction),"Ephemeral faction missing");
        auto& f=candidate.world.factions.get(n.faction);
        core::require(f.reserveMembers>0,"No aggregate member available for promotion"); --f.reserveMembers;
        n.roles.add(NpcRole::FactionMember);
    }
    const auto entity=n.activeEntity;
    if (entity) {
        core::require(candidate.world.entities.exists(entity) && !candidate.world.npcLinks.contains(entity) &&
            !candidate.world.vehicleLinks.contains(entity) && candidate.world.transforms.contains(entity) &&
            candidate.world.health.contains(entity),"Invalid ephemeral body");
        n.position=candidate.world.transforms.get(entity).position; n.health=candidate.world.health.get(entity);
    }
    n.semantic={}; const auto id=candidate.world.npcs.create(n);
    if (entity) candidate.world.npcLinks.set(candidate.world.entities,entity,{id});
    initializeNpc(candidate,id); candidate.world.npcs.get(id).semantic.activity=priorActivity; increment(candidate.npcSimulation.statistics.promotions); validateState(candidate);
    state=std::move(candidate); actor.promotedTo=id;
    events.push({NpcEventKind::Promoted,id,{},{},state.clock.tick()}); return id;
}
void updateNpcs(GameState& state,NpcEventBuffer& events,bool operationsChanged) {
    const auto now=state.clock.tick();
    if (now<nextNpcDueTick(state) && !operationsChanged) return;
    for (const auto& [id,record] : state.world.npcs.entries()) {
        (void)record; auto& n=state.world.npcs.get(id); auto& s=n.semantic;
        if (s.operation && !pending(state.factionSimulation.operations.get(s.operation).status)) completeAssignment(state,id,events);
        if (n.lifecycle!=NpcLifecycle::Alive) continue;
        if (s.travel && s.travel->arrivalTick<=now) {
            n.location=s.travel->destination; n.position=fromGlobal(locationPoint(state,n.location),state.world.spatial.cellSize());
            s.travel.reset(); activity(state,id,s.operation ? NpcActivity::FactionOperation : NpcActivity::Meeting,events);
            increment(state.npcSimulation.statistics.travelsCompleted);
            events.push({NpcEventKind::TravelCompleted,id,{},s.operation,now});
        }
        if (s.nextUpdateTick>now) continue;
        increment(state.npcSimulation.statistics.updates);
        s.nextUpdateTick=after(now,state.npcSimulation.config.updateIntervalTicks);
        if (n.activeEntity || s.travel) continue; // Physical navigation is outside this semantic system.
        if (s.operation) { activity(state,id,NpcActivity::FactionOperation,events); continue; }
        const bool leader=n.roles.has(NpcRole::FactionLeader);
        const auto willingness=static_cast<unsigned>(n.traits.courage)+n.traits.loyaltyBias+n.traits.ambition/2+n.capabilities.leadership/2;
        OperationId selected; unsigned priority=0;
        if (n.faction && n.roles.has(NpcRole::FactionMember) && (leader || willingness>=14000)) {
            for (const auto opId : state.factionSimulation.pendingOperations) {
                const auto& op=state.factionSimulation.operations.get(opId);
                if (op.faction!=n.faction) continue;
                if (n.roles.has(NpcRole::Dealer) && (op.type==OperationType::AttackTerritory || n.traits.courage<5000)) continue;
                const unsigned value=op.type==OperationType::DefendTerritory ?
                    (op.target==state.world.factions.get(n.faction).homeTerritory ? 3U : 2U) : 1U;
                if (value>priority) { selected=opId; priority=value; }
            }
        }
        if (selected) {
            s.operation=selected; increment(state.npcSimulation.statistics.participations);
            activity(state,id,NpcActivity::FactionOperation,events);
            events.push({NpcEventKind::OperationAssigned,id,{},selected,now});
            // Leaders coordinate remotely at base. Named support is NOT extra
            // aggregate manpower and never changes the Phase 9 combat formula.
            const auto dest=operationLocation(state,state.factionSimulation.operations.get(selected).target);
            if (!leader && n.location!=dest && state.world.base && after(now,npcTravelDuration(state,n.location,dest))!=neverTick)
                startNpcTravel(state,id,dest,events);
            continue;
        }
        if (s.home.district && n.location!=s.home && state.world.base && after(now,npcTravelDuration(state,n.location,s.home))!=neverTick) {
            startNpcTravel(state,id,s.home,events); continue;
        }
        if (leader) { activity(state,id,NpcActivity::Working,events); continue; }
        // Bounded choice within role responsibility, not a citizen timetable.
        auto& rng=state.randomness.get(RandomDomain::Npc);
        if (n.roles.has(NpcRole::Dealer)) {
            const auto confidence=n.faction ? state.world.factions.get(n.faction).strategy.confidence : 5000;
            activity(state,id,confidence<3000 && n.traits.courage<5000 ? NpcActivity::Hiding : NpcActivity::Working,events);
        } else if (n.faction && rng.chance(1,8)) {
            const auto partner=state.world.factions.get(n.faction).leader;
            if (partner && partner!=id && state.world.npcs.get(partner).lifecycle==NpcLifecycle::Alive) {
                const auto dest=state.world.npcs.get(partner).semantic.home;
                if (dest!=n.location && validNpcLocation(state.world,dest) && state.world.base && after(now,npcTravelDuration(state,n.location,dest))!=neverTick)
                    startNpcTravel(state,id,dest,events);
                else activity(state,id,NpcActivity::Meeting,events);
            } else activity(state,id,NpcActivity::Working,events);
        } else activity(state,id,n.roles.has(NpcRole::FactionMember) ? NpcActivity::Working : NpcActivity::Idle,events);
    }
    rebuildNpcSchedule(state);
}
void validateNpcSimulation(const GameState& state) {
    checkConfig(state.npcSimulation.config); auto next=neverTick; std::uint64_t travelling=0;
    for (const auto& [id,n] : state.world.npcs.entries()) {
        const auto& s=n.semantic;
        std::ostringstream context; context << "NPC invariant seed=" << state.metadata.seed.value << " generator=" << state.metadata.generatorVersion.value
            << " tick=" << state.clock.tick() << " gameMs=" << state.clock.gameTime().milliseconds << " npc=" << id << " faction=" << n.faction
            << " operation=" << s.operation << " territory=" << n.location.territory;
        const auto require=[&](bool valid,const char* reason){core::require(valid,(context.str()+": "+reason).c_str());};
        require(s.activity<=NpcActivity::Recovering,"invalid activity");
        require(n.lifecycle==NpcLifecycle::Dead || validNpcLocation(state.world,n.location),"invalid living semantic location");
        require(s.home==WorldLocation{} || validNpcLocation(state.world,s.home),"invalid home");
        require(!n.roles.has(NpcRole::FactionMember) || static_cast<bool>(n.faction),"member without faction");
        require(n.lifecycle!=NpcLifecycle::Alive || (s.activity!=NpcActivity::Arrested && s.activity!=NpcActivity::Recovering),"alive actor has restrictive activity");
        require(s.nextUpdateTick==neverTick || (s.nextUpdateTick>=state.clock.tick() && s.nextUpdateTick<=core::SimulationClock::maxTick),"stale/invalid schedule");
        next=std::min(next,s.nextUpdateTick);
        require(s.travel.has_value()==(s.activity==NpcActivity::Travelling),"travel/activity mismatch");
        if (s.travel) {
            ++travelling; const auto& t=*s.travel; next=std::min(next,t.arrivalTick);
            require(n.lifecycle==NpcLifecycle::Alive && !n.activeEntity && state.world.base.has_value(),"travel requires abstract living actor and geometry");
            require(validNpcLocation(state.world,t.origin) && validNpcLocation(state.world,t.destination) && t.origin!=t.destination && n.location==t.origin,"invalid travel locations");
            require(t.departureTick<=state.clock.tick() && t.arrivalTick>t.departureTick && t.arrivalTick>=state.clock.tick() && t.arrivalTick<=core::SimulationClock::maxTick,"invalid travel time");
        }
        if (n.lifecycle!=NpcLifecycle::Alive) {
            require(!s.travel && !s.operation && s.activity==restrictedActivity(n.lifecycle) && s.nextUpdateTick==neverTick,"restricted lifecycle acting");
        }
        if (s.operation) {
            require(n.lifecycle==NpcLifecycle::Alive && state.factionSimulation.operations.exists(s.operation),"invalid participant/operation");
            const auto& op=state.factionSimulation.operations.get(s.operation);
            require(pending(op.status) && n.faction==op.faction && n.roles.has(NpcRole::FactionMember) &&
                (s.activity==NpcActivity::Travelling || s.activity==NpcActivity::FactionOperation),"incoherent operation participation");
        }
        require(s.activity!=NpcActivity::FactionOperation || static_cast<bool>(s.operation),"operation activity without assignment");
        if (n.roles.has(NpcRole::FactionLeader) && n.faction) require(state.world.factions.get(n.faction).leader==id,"leader role mismatch");
    }
    const auto& stats=state.npcSimulation.statistics;
    core::require(stats.travelsCompleted<=stats.travelsStarted && stats.travelsCancelled<=stats.travelsStarted-stats.travelsCompleted &&
        stats.travelsStarted-stats.travelsCompleted-stats.travelsCancelled==travelling,"NPC travel accounting mismatch");
    core::require(next==state.npcSimulation.nextDueTick,"NPC derived deadline index mismatch");
}
void appendNpcSimulationHash(core::StateHash& h,const GameState& state) {
    const auto& c=state.npcSimulation.config; const auto& stats=state.npcSimulation.statistics;
    h.u64(c.updateIntervalTicks); h.u64(c.minimumTravelTicks); h.u64(c.metersPerGameMinute);
    for (auto value : {stats.updates,stats.travelsStarted,stats.travelsCompleted,stats.travelsCancelled,stats.participations,stats.relationshipChanges,stats.promotions}) h.u64(value);
    const auto location=[&](WorldLocation l){h.u64(l.district.value());h.u64(l.territory.value());h.u64(l.building.value());};
    h.u64(state.world.npcs.entries().size());
    for (const auto& [id,n] : state.world.npcs.entries()) {
        const auto& s=n.semantic; h.u64(id.value()); h.u64(static_cast<std::uint64_t>(s.activity)); location(s.home);
        h.u64(s.nextUpdateTick); h.u64(s.operation.value()); h.u64(s.travel.has_value());
        if (s.travel) { location(s.travel->origin); location(s.travel->destination); h.u64(s.travel->departureTick); h.u64(s.travel->arrivalTick); }
    }
}
}
