#include "simulation/police/police_system.hpp"
#include "simulation/runtime/game.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/market/market_system.hpp"
#include "world/spatial/character_movement.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <string>

namespace faultline::simulation {
namespace {

constexpr double arrestRangeMeters=1.25;

std::uint64_t after(std::uint64_t tick,std::uint64_t duration) {
    core::require(duration<=core::SimulationClock::maxTick-tick,"Police deadline overflow");
    return tick+duration;
}

bool inside(world::Rect r,world::DoubleVec3 p) {
    return p.x>=r.x && p.x<=static_cast<double>(world::right(r)) &&
           p.z>=r.z && p.z<=static_cast<double>(world::bottom(r));
}

world::WorldLocation locationAt(const GameState& state,world::WorldPosition position) {
    if (!state.world.base) return {};
    const auto point=world::toGlobal(position,state.world.spatial.cellSize());
    world::WorldLocation result;
    for (const auto& district:state.world.base->districts)
        if (inside(district.boundary,point)) { result.district=district.id; break; }
    for (const auto& territory:state.world.base->territories)
        if (inside(territory.boundary,point)) {
            result.territory=territory.id; result.district=territory.district; break;
        }
    return result;
}

const CrimeEvent& crime(const GameState& state,CrimeEventId id) {
    const auto it=std::lower_bound(state.police.crimes.begin(),state.police.crimes.end(),id,
        [](const CrimeEvent& value,CrimeEventId key){return value.id<key;});
    core::require(it!=state.police.crimes.end() && it->id==id,"Unknown crime event");
    return *it;
}

void addPressure(GameState& state,const CrimeEvent& value) {
    const auto location=locationAt(state,value.position);
    if (!location.district) return;
    auto& pressure=state.police.districtPressureBps[location.district];
    const auto increase=std::max<std::uint16_t>(100,static_cast<std::uint16_t>(value.severityBps/4));
    pressure=static_cast<std::uint16_t>(std::min<unsigned>(10000,pressure+increase));
    // PoliceSystem owns pressure; MarketSystem receives only the explicit strategic consequence.
    disruptDistrictMarket(state,location.district,increase);
}

void clearWanted(GameState& state) {
    state.police.wanted={};
    for (auto& response:state.police.responders)
        if (response.status!=PoliceResponderStatus::Released)
            response.status=PoliceResponderStatus::Released;
}

void dispatch(GameState& state,PoliceEventBuffer& events) {
    auto& police=state.police;
    if (!police.wanted.lastKnownPosition) return;
    const bool already=std::any_of(police.responders.begin(),police.responders.end(),
        [](const auto& value){return value.status!=PoliceResponderStatus::Released;});
    if (already) return;
    const std::size_t count=police.wanted.heatBps>=7500 ? 3U : police.wanted.heatBps>=4000 ? 2U : 1U;
    const auto target=*police.wanted.lastKnownPosition;
    auto global=world::toGlobal(target,state.world.spatial.cellSize());
    for (std::size_t index=0;index<count;++index) {
        auto spawn=global;
        spawn.x+=600.0+static_cast<double>(index)*10.0;
        if (state.world.base) {
            const auto& bounds=state.world.base->bounds;
            spawn.x=std::clamp(spawn.x,static_cast<double>(bounds.x),static_cast<double>(world::right(bounds)));
            spawn.z=std::clamp(spawn.z,static_cast<double>(bounds.z),static_cast<double>(world::bottom(bounds)));
        }
        auto position=world::fromGlobal(spawn,state.world.spatial.cellSize());
        auto location=locationAt(state,position);
        if (!location.district) { position=target; location=locationAt(state,target); }
        const auto officer=createPoliceOfficer(state,position,location);
        police.responders.push_back({officer,PoliceResponderStatus::Dispatching,state.clock.tick(),
            after(state.clock.tick(),police.rules.responseTravelTicks),target});
        ++police.statistics.responses;
        events.push({PoliceEventKind::ResponseDispatched,{}, {},officer,{}, {},state.clock.tick()});
    }
}

void knowledge(GameState& state,const CrimeEvent& value,NpcId officer,bool direct,
               PoliceEventBuffer& events) {
    auto& police=state.police;
    auto found=std::lower_bound(police.knowledge.begin(),police.knowledge.end(),value.id,
        [](const PoliceKnowledge& item,CrimeEventId id){return item.crime<id;});
    const bool identified=value.actor.kind!=CrimeActorKind::Unknown;
    if (found==police.knowledge.end() || found->crime!=value.id) {
        found=police.knowledge.insert(found,{value.id,identified,value.actor,value.position,
            state.clock.tick(),direct,static_cast<std::uint16_t>(direct?0:1)});
        addPressure(state,value);
        events.push({PoliceEventKind::KnowledgeCreated,value.id,{},officer,{}, {},state.clock.tick()});
    } else if (!direct && found->deliveredReports<std::numeric_limits<std::uint16_t>::max()) {
        ++found->deliveredReports;
    }
    if (value.actor.kind!=CrimeActorKind::Player) return;
    auto& wanted=police.wanted;
    const auto extra=found->deliveredReports>1 ? static_cast<unsigned>(value.severityBps/10U) : 0U;
    wanted.heatBps=static_cast<std::uint16_t>(std::min<unsigned>(10000U,
        static_cast<unsigned>(wanted.heatBps)+static_cast<unsigned>(value.severityBps)+extra));
    wanted.suspectIdentified=identified;
    wanted.lastKnownPosition=value.position;
    wanted.lastConfirmedTick=state.clock.tick();
    wanted.confidenceRadiusMeters=police.rules.initialSearchRadiusMeters;
    if (direct) {
        if (wanted.state!=WantedState::Located && wanted.state!=WantedState::Pursuit)
            transitionWanted(state,WantedState::Located,after(state.clock.tick(),1),events);
    } else if (wanted.state==WantedState::Clear || wanted.state==WantedState::Suspicion ||
               wanted.state==WantedState::Cooldown) {
        transitionWanted(state,identified?WantedState::Reported:WantedState::Suspicion,
            after(state.clock.tick(),identified?police.rules.reportedTicks:police.rules.suspicionTicks),events);
    }
}

bool eligibleWitness(const GameState& state,NpcId id,const CrimeEvent& value) {
    const auto& npc=state.world.npcs.get(id);
    if (!npc.activeEntity || (npc.lifecycle!=world::NpcLifecycle::Alive &&
        npc.lifecycle!=world::NpcLifecycle::Injured)) return false;
    if (value.actor.kind==CrimeActorKind::Npc && value.actor.npc==id) return false;
    if (value.victim.kind==CrimeActorKind::Npc && value.victim.npc==id) return false;
    return true;
}

bool observes(const GameState& state,NpcId id,const CrimeEvent& value) {
    const auto& npc=state.world.npcs.get(id);
    const auto position=state.world.transforms.get(npc.activeEntity).position;
    const auto distance=std::sqrt(world::distanceSquared(position,value.position,state.world.spatial.cellSize()));
    const bool audible=value.observability!=CrimeObservability::Visual &&
        distance<=state.police.rules.audibleRangeMeters;
    const bool visual=value.observability!=CrimeObservability::Audible &&
        distance<=state.police.rules.visualRangeMeters &&
        world::hasStaticLineOfSight(state.world,position,value.position);
    return audible||visual;
}

void advanceWantedDeadlines(GameState& state,PoliceEventBuffer& events) {
    auto& wanted=state.police.wanted; const auto now=state.clock.tick();
    if (!wanted.deadlineTick || now<wanted.deadlineTick) return;
    switch (wanted.state) {
    case WantedState::Suspicion:
        transitionWanted(state,WantedState::Clear,0,events); clearWanted(state); break;
    case WantedState::Reported:
        transitionWanted(state,WantedState::Searching,after(now,state.police.rules.searchTicks),events);
        dispatch(state,events); break;
    case WantedState::Searching:
        transitionWanted(state,WantedState::Cooldown,after(now,state.police.rules.cooldownTicks),events); break;
    case WantedState::Located:
        transitionWanted(state,WantedState::Pursuit,0,events); break;
    case WantedState::EscapedPending:
        transitionWanted(state,WantedState::Searching,after(now,state.police.rules.searchTicks),events); break;
    case WantedState::Cooldown:
        transitionWanted(state,WantedState::Clear,0,events); clearWanted(state); break;
    default: break;
    }
}

bool activeOfficer(const GameState& state,NpcId id) {
    if (!state.world.npcs.exists(id)) return false;
    const auto& npc=state.world.npcs.get(id);
    return npc.roles.has(world::NpcRole::PoliceOfficer) && npc.activeEntity &&
        (npc.lifecycle==world::NpcLifecycle::Alive || npc.lifecycle==world::NpcLifecycle::Injured) &&
        state.world.entities.exists(npc.activeEntity);
}

} // namespace

void initializePoliceSimulation(GameState& state) {
    state.police={};
    for (const auto& [id,district]:state.world.districts.entries()) {
        (void)district; state.police.districtPressureBps.emplace(id,0);
    }
}

CrimeEventId recordCrime(GameState& state,CrimeType type,CrimeActorRef actor,
                         CrimeActorRef victim,world::WorldPosition position,
                         std::uint16_t severity,CrimeObservability observability,
                         PoliceEventBuffer& events) {
    core::require(type<=CrimeType::PoliceAttack && actor.kind<=CrimeActorKind::Npc &&
        victim.kind<=CrimeActorKind::Npc && severity>0 && severity<=10000,
        "Invalid crime event");
    core::require((actor.kind==CrimeActorKind::Npc)==static_cast<bool>(actor.npc) &&
        (victim.kind==CrimeActorKind::Npc)==static_cast<bool>(victim.npc),
        "Invalid crime actor identity");
    if (actor.npc) core::require(state.world.npcs.exists(actor.npc),"Unknown crime actor");
    if (victim.npc) core::require(state.world.npcs.exists(victim.npc),"Unknown crime victim");
    core::require(state.police.nextCrimeId!=0,"Crime ID exhausted");
    const auto id=CrimeEventId{state.police.nextCrimeId++};
    state.police.crimes.push_back({id,type,actor,victim,
        world::normalize(position,state.world.spatial.cellSize()),state.clock.tick(),severity,observability});
    ++state.police.statistics.crimes;
    events.push({PoliceEventKind::CrimeRecorded,id,{}, {},{}, {},state.clock.tick()});
    return id;
}

void ingestCombatCrimes(GameState& state,const CombatEventBuffer& combat,PoliceEventBuffer& events) {
    std::uint64_t index=0;
    for (const auto& hit:combat.values()) {
        const ProcessedCombatFact source{hit.tick,index++};
        if (std::binary_search(state.police.processedCombatFacts.begin(),
                               state.police.processedCombatFacts.end(),source)) {
            ++state.police.statistics.duplicateFacts; continue;
        }
        const bool fired=hit.result==CombatResult::Hit || hit.result==CombatResult::MissOutOfRange ||
            hit.result==CombatResult::MissBlocked;
        if (!fired) continue;
        state.police.processedCombatFacts.insert(
            std::lower_bound(state.police.processedCombatFacts.begin(),
                             state.police.processedCombatFacts.end(),source),source);
        const auto convert=[](CombatActorRef ref) {
            return ref.kind==CombatActorKind::Player ? CrimeActorRef{CrimeActorKind::Player,{}} :
                CrimeActorRef{CrimeActorKind::Npc,ref.npc};
        };
        CrimeType type=CrimeType::WeaponDischarge; std::uint16_t severity=1500;
        if (hit.result==CombatResult::Hit) { type=hit.death?CrimeType::Murder:CrimeType::Assault; severity=hit.death?8000:4000; }
        if (hit.intent.target.kind==CombatActorKind::Npc && hit.intent.target.npc &&
            state.world.npcs.get(hit.intent.target.npc).roles.has(world::NpcRole::PoliceOfficer)) {
            type=CrimeType::PoliceAttack; severity=9000;
        }
        (void)recordCrime(state,type,convert(hit.intent.attacker),convert(hit.intent.target),
            hit.targetPosition,severity,type==CrimeType::WeaponDischarge?
                CrimeObservability::VisualAndAudible:CrimeObservability::Visual,events);
    }
}

void observeNewCrimes(GameState& state,PoliceEventBuffer& events) {
    auto& police=state.police;
    while (police.witnessProcessedCount<police.crimes.size()) {
        const auto& value=police.crimes.at(static_cast<std::size_t>(police.witnessProcessedCount++));
        const auto radius=std::max(police.rules.visualRangeMeters,police.rules.audibleRangeMeters);
        auto entities=state.world.spatial.queryRadius(value.position,radius,[&](EntityId id){
            return state.world.transforms.get(id).position;
        });
        std::vector<NpcId> candidates;
        for (auto entity:entities) if (state.world.npcLinks.contains(entity))
            candidates.push_back(state.world.npcLinks.get(entity).npc);
        std::sort(candidates.begin(),candidates.end());
        candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
        for (auto witness:candidates) {
            if (!eligibleWitness(state,witness,value) || !observes(state,witness,value)) continue;
            const WitnessObservation observation{value.id,witness,state.clock.tick()};
            if (std::binary_search(police.observations.begin(),police.observations.end(),observation)) {
                ++police.statistics.duplicateFacts; continue;
            }
            police.observations.insert(std::lower_bound(police.observations.begin(),
                police.observations.end(),observation),observation);
            ++police.statistics.observations;
            events.push({PoliceEventKind::WitnessObserved,value.id,{},witness,{}, {},state.clock.tick()});
            auto& npc=state.world.npcs.get(witness); npc.roles.add(world::NpcRole::Witness);
            if (npc.roles.has(world::NpcRole::PoliceOfficer)) {
                ++police.statistics.directObservations;
                events.push({PoliceEventKind::DirectObservation,value.id,{},witness,{}, {},state.clock.tick()});
                knowledge(state,value,witness,true,events);
                if (std::none_of(police.responders.begin(),police.responders.end(),
                    [&](const auto& response){return response.officer==witness && response.status!=PoliceResponderStatus::Released;}))
                    police.responders.push_back({witness,PoliceResponderStatus::Active,state.clock.tick(),state.clock.tick(),value.position});
                continue;
            }
            const bool duplicate=std::any_of(police.reports.begin(),police.reports.end(),[&](const auto& report){
                return report.crime==value.id && report.witness==witness;
            });
            if (duplicate) { ++police.statistics.duplicateFacts; continue; }
            core::require(police.nextReportId!=0,"Report ID exhausted");
            const auto reportId=CrimeReportId{police.nextReportId++};
            police.reports.push_back({reportId,value.id,witness,state.clock.tick(),
                after(state.clock.tick(),police.rules.reportDelayTicks),ReportStatus::Pending});
            ++police.statistics.reportsScheduled;
            events.push({PoliceEventKind::ReportScheduled,value.id,reportId,witness,{}, {},state.clock.tick()});
        }
    }
}

bool isAllowedWantedTransition(WantedState from,WantedState to) {
    if (from==to) return true;
    switch (from) {
    case WantedState::Clear: return to==WantedState::Suspicion || to==WantedState::Reported || to==WantedState::Located;
    case WantedState::Suspicion: return to==WantedState::Clear || to==WantedState::Reported || to==WantedState::Located;
    case WantedState::Reported: return to==WantedState::Searching || to==WantedState::Located;
    case WantedState::Searching: return to==WantedState::Reported || to==WantedState::Located || to==WantedState::Cooldown;
    case WantedState::Located: return to==WantedState::Pursuit || to==WantedState::EscapedPending;
    case WantedState::Pursuit: return to==WantedState::EscapedPending;
    case WantedState::EscapedPending: return to==WantedState::Located || to==WantedState::Searching || to==WantedState::Cooldown;
    case WantedState::Cooldown: return to==WantedState::Clear || to==WantedState::Reported || to==WantedState::Located;
    }
    return false;
}

void transitionWanted(GameState& state,WantedState to,std::uint64_t deadline,PoliceEventBuffer& events) {
    auto& wanted=state.police.wanted; const auto from=wanted.state;
    core::require(isAllowedWantedTransition(from,to),"Forbidden Wanted state transition");
    core::require(!deadline || deadline>=state.clock.tick(),"Stale Wanted deadline");
    if (from==to) return;
    wanted.state=to; wanted.deadlineTick=deadline; ++state.police.statistics.wantedTransitions;
    if (to==WantedState::Searching) ++state.police.statistics.searchTransitions;
    if (to==WantedState::Pursuit) ++state.police.statistics.pursuitTransitions;
    events.push({PoliceEventKind::WantedTransition,{}, {},{},from,to,state.clock.tick()});
}

void updateReportsAndWanted(GameState& state,PoliceEventBuffer& events) {
    for (auto& report:state.police.reports) if (report.status==ReportStatus::Pending &&
        state.clock.tick()>=report.completionTick) {
        const auto& witness=state.world.npcs.get(report.witness);
        if (witness.lifecycle!=world::NpcLifecycle::Alive && witness.lifecycle!=world::NpcLifecycle::Injured) {
            report.status=ReportStatus::Interrupted; ++state.police.statistics.reportsInterrupted;
            events.push({PoliceEventKind::ReportInterrupted,report.crime,report.id,report.witness,{}, {},state.clock.tick()});
        } else {
            report.status=ReportStatus::Delivered; ++state.police.statistics.reportsDelivered;
            events.push({PoliceEventKind::ReportDelivered,report.crime,report.id,report.witness,{}, {},state.clock.tick()});
            knowledge(state,crime(state,report.crime),report.witness,false,events);
        }
    }
    advanceWantedDeadlines(state,events);
}

std::uint64_t nextPoliceDueTick(const GameState& state) {
    std::uint64_t result=std::numeric_limits<std::uint64_t>::max();
    for (const auto& report:state.police.reports) if (report.status==ReportStatus::Pending)
        result=std::min(result,report.completionTick);
    if (state.police.wanted.deadlineTick) result=std::min(result,state.police.wanted.deadlineTick);
    for (const auto& response:state.police.responders)
        if (response.status==PoliceResponderStatus::Dispatching) result=std::min(result,response.arrivalTick);
    return result;
}

NpcId createPoliceOfficer(GameState& state,world::WorldPosition position,world::WorldLocation location) {
    core::require(location.district && validNpcLocation(state.world,location),"Police officer requires valid semantic location");
    world::NpcState officer;
    officer.lifecycle=world::NpcLifecycle::Alive; officer.location=location; officer.position=position;
    officer.identity.age=world::AgeBand::Adult; officer.identity.appearanceSeed=core::deriveSeed(
        state.metadata.seed.value,"runtime.police.officer",NpcId{state.world.npcs.nextId()});
    officer.roles.add(world::NpcRole::PoliceOfficer); officer.authority=world::AuthorityTier::Member;
    officer.traits.courage=8000; officer.traits.riskTolerance=7000; officer.capabilities.combat=7000;
    const auto id=state.world.npcs.create(officer);
    state.world.npcs.get(id).identity.name="Officer "+std::to_string(id.value());
    initializeNpc(state,id); addNpcCombatant(state,id,WeaponArchetype::Pistol);
    return id;
}

void updatePoliceResponse(GameState& state,CharacterAiEventBuffer& aiEvents,PoliceEventBuffer& events) {
    advanceWantedDeadlines(state,events);
    auto& police=state.police; auto& wanted=police.wanted;
    if (wanted.state==WantedState::Clear || wanted.state==WantedState::Suspicion) return;
    const auto player=state.player.controlledEntity;
    const bool playerValid=player && state.world.entities.exists(player) && state.world.transforms.contains(player) &&
        state.world.health.get(player).current>0;
    bool seen=false;
    for (auto& response:police.responders) {
        if (response.status==PoliceResponderStatus::Released) continue;
        auto& officer=state.world.npcs.get(response.officer);
        if (response.status==PoliceResponderStatus::Dispatching && state.clock.tick()>=response.arrivalTick) {
            response.status=PoliceResponderStatus::Active;
            if (wanted.lastKnownPosition) {
                auto point=world::toGlobal(*wanted.lastKnownPosition,state.world.spatial.cellSize());
                point.x+=10.0+static_cast<double>(response.officer.value()%3)*3.0;
                officer.position=world::fromGlobal(point,state.world.spatial.cellSize());
                officer.location=locationAt(state,officer.position); response.target=*wanted.lastKnownPosition;
                if (officer.activeEntity) {
                    state.world.transforms.get(officer.activeEntity).position=officer.position;
                    state.world.spatial.upsert(officer.activeEntity,officer.position);
                }
            }
        }
        if (response.status!=PoliceResponderStatus::Active || !activeOfficer(state,response.officer)) continue;
        const auto entity=officer.activeEntity;
        if (playerValid) {
            const auto officerPosition=state.world.transforms.get(entity).position;
            const auto playerPosition=state.world.transforms.get(player).position;
            const auto distance=std::sqrt(world::distanceSquared(officerPosition,playerPosition,state.world.spatial.cellSize()));
            if (distance<=police.rules.visualRangeMeters &&
                world::hasStaticLineOfSight(state.world,officerPosition,playerPosition)) {
                const bool reacquired=wanted.state==WantedState::EscapedPending || wanted.state==WantedState::Searching || wanted.state==WantedState::Cooldown;
                wanted.lastKnownPosition=playerPosition; wanted.lastConfirmedTick=state.clock.tick();
                wanted.confidenceRadiusMeters=police.rules.initialSearchRadiusMeters; response.target=playerPosition; seen=true;
                if (wanted.state!=WantedState::Located && wanted.state!=WantedState::Pursuit)
                    transitionWanted(state,WantedState::Located,after(state.clock.tick(),1),events);
                if (reacquired) ++police.statistics.reacquisitions;
                events.push({PoliceEventKind::SuspectLocated,{}, {},response.officer,{}, {},state.clock.tick()});
                if ((wanted.state==WantedState::Located || wanted.state==WantedState::Pursuit) &&
                    distance<=arrestRangeMeters && state.world.health.get(player).current>0 &&
                    state.world.health.get(player).current<=police.rules.arrestHealthThreshold) {
                    police.pendingArrest=true; police.arrestPosition=playerPosition;
                    events.push({PoliceEventKind::Arrested,{}, {},response.officer,{}, {},state.clock.tick()});
                }
            }
        }
        if (wanted.lastKnownPosition) {
            CharacterAiIntent intent{response.officer,entity,
                seen?CharacterAiBehavior::PolicePursuit:CharacterAiBehavior::PoliceSearch,
                true,*wanted.lastKnownPosition,{}};
            aiEvents.push(executeCharacterIntent(state,intent));
        }
    }
    if (!seen && wanted.state==WantedState::Pursuit &&
        state.clock.tick()>=wanted.lastConfirmedTick+police.rules.lostContactTicks) {
        transitionWanted(state,WantedState::EscapedPending,
            after(state.clock.tick(),police.rules.escapedPendingTicks),events);
        events.push({PoliceEventKind::ContactLost,{}, {},{}, {}, {},state.clock.tick()});
    }
    if (!seen && (wanted.state==WantedState::Searching || wanted.state==WantedState::EscapedPending) && wanted.lastKnownPosition) {
        const auto elapsed=state.clock.tick()-wanted.lastConfirmedTick;
        wanted.confidenceRadiusMeters=police.rules.initialSearchRadiusMeters+
            static_cast<std::uint32_t>(std::min<std::uint64_t>(1000,(elapsed/20)*police.rules.searchExpansionMeters));
    }
}

bool arrestPending(const GameState& state) { return state.police.pendingArrest; }

void resolvePlayerArrest(GameState& state) {
    core::require(state.police.pendingArrest && state.player.controlledEntity,"No pending player arrest");
    auto release=state.police.arrestPosition;
    if (state.world.base) {
        const auto location=locationAt(state,release);
        for (const auto& district:state.world.base->districts) if (district.id==location.district) {
            // The current generated model has no station object. This deterministic
            // road-adjacent district release point represents the station exterior.
            release=world::fromGlobal({district.boundary.x+2.0,0,district.boundary.z+2.0},state.world.spatial.cellSize());
            break;
        }
    }
    std::vector<EntityId> bodies;
    for (const auto& [id,npc]:state.world.npcs.entries()) { (void)id; if (npc.activeEntity) bodies.push_back(npc.activeEntity); }
    for (auto entity:bodies) world::destroyEntity(state.world,entity);
    world::destroyEntity(state.world,state.player.controlledEntity);
    state.player={};
    for (auto& report:state.police.reports) if (report.status==ReportStatus::Pending)
        report.status=ReportStatus::Interrupted;
    state.police.pendingArrest=false; clearWanted(state); ++state.police.statistics.arrests;
    Simulation coarse;
    coarse.advanceHeadless(state,state.police.rules.arrestAdvanceTicks);
    createPlayerEntity(state,release);
}

void validatePoliceSimulation(const GameState& state) {
    const auto& police=state.police; const auto& rules=police.rules;
    core::require(police.version==currentPoliceStateVersion,"Unsupported police state version");
    core::require(rules.reportDelayTicks && rules.reportedTicks && rules.lostContactTicks &&
        rules.escapedPendingTicks && rules.searchTicks && rules.cooldownTicks && rules.suspicionTicks &&
        rules.responseTravelTicks && rules.arrestAdvanceTicks && rules.visualRangeMeters &&
        rules.audibleRangeMeters && rules.initialSearchRadiusMeters && rules.arrestHealthThreshold>0,
        "Invalid police rules");
    core::require(police.nextCrimeId && police.nextCrimeId>police.crimes.size() &&
        police.nextReportId && police.nextReportId>police.reports.size() &&
        police.witnessProcessedCount<=police.crimes.size(),"Invalid police allocators/progress");
    CrimeEventId previousCrime;
    for (const auto& value:police.crimes) {
        core::require(value.id>previousCrime && value.id.value()<police.nextCrimeId && value.type<=CrimeType::PoliceAttack &&
            value.actor.kind<=CrimeActorKind::Npc && value.victim.kind<=CrimeActorKind::Npc && value.severityBps>0 && value.severityBps<=10000 &&
            world::normalize(value.position,state.world.spatial.cellSize())==value.position && value.tick<=state.clock.tick(),"Invalid crime record");
        if (value.actor.npc) core::require(state.world.npcs.exists(value.actor.npc),"Missing crime actor");
        if (value.victim.npc) core::require(state.world.npcs.exists(value.victim.npc),"Missing crime victim");
        previousCrime=value.id;
    }
    CrimeReportId previousReport;
    for (const auto& report:police.reports) {
        core::require(report.id>previousReport && report.id.value()<police.nextReportId && report.status<=ReportStatus::Interrupted &&
            state.world.npcs.exists(report.witness) && report.observedTick<=report.completionTick &&
            crime(state,report.crime).tick<=report.observedTick,"Invalid crime report");
        if (report.status==ReportStatus::Pending) core::require(report.completionTick>=state.clock.tick(),"Stale pending report");
        previousReport=report.id;
    }
    for (std::size_t i=1;i<police.observations.size();++i)
        core::require(police.observations[i-1]<police.observations[i],"Duplicate/unordered witness observations");
    for (std::size_t i=1;i<police.knowledge.size();++i)
        core::require(police.knowledge[i-1].crime<police.knowledge[i].crime,"Duplicate/unordered police knowledge");
    for (const auto& item:police.knowledge) core::require(item.deliveredReports || item.directObservation,"Knowledge without source");
    core::require(police.wanted.state<=WantedState::Cooldown && police.wanted.heatBps<=10000,
                  "Invalid Wanted state");
    if (police.wanted.state==WantedState::Clear)
        core::require(!police.wanted.lastKnownPosition && !police.wanted.deadlineTick && police.wanted.heatBps==0,"Dirty clear Wanted state");
    if (police.wanted.lastKnownPosition)
        core::require(world::normalize(*police.wanted.lastKnownPosition,state.world.spatial.cellSize())==*police.wanted.lastKnownPosition,"Invalid last-known position");
    std::set<NpcId> responders;
    for (const auto& response:police.responders) core::require(responders.insert(response.officer).second &&
        state.world.npcs.exists(response.officer) && state.world.npcs.get(response.officer).roles.has(world::NpcRole::PoliceOfficer) &&
        response.status<=PoliceResponderStatus::Released && response.dispatchTick<=response.arrivalTick,
        "Invalid police responder");
    for (const auto& [id,pressure]:police.districtPressureBps)
        core::require(state.world.districts.exists(id) && pressure<=10000,"Invalid district police pressure");
}

void appendPoliceSimulationHash(core::StateHash& h,const GameState& state) {
    const auto& p=state.police; h.u64(p.version);
    const auto& r=p.rules;
    for (auto value:{r.reportDelayTicks,r.reportedTicks,r.lostContactTicks,r.escapedPendingTicks,r.searchTicks,
        r.cooldownTicks,r.suspicionTicks,r.responseTravelTicks,r.arrestAdvanceTicks}) h.u64(value);
    for (auto value:{r.visualRangeMeters,r.audibleRangeMeters,r.initialSearchRadiusMeters,r.searchExpansionMeters}) h.u64(value);
    h.u64(r.arrestHealthThreshold);
    h.u64(p.nextCrimeId);h.u64(p.nextReportId);h.u64(p.witnessProcessedCount);
    const auto position=[&](world::WorldPosition value){h.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(value.cell.x)));h.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(value.cell.z)));h.u64(std::bit_cast<std::uint32_t>(value.local.x==0?0.0F:value.local.x));h.u64(std::bit_cast<std::uint32_t>(value.local.y==0?0.0F:value.local.y));h.u64(std::bit_cast<std::uint32_t>(value.local.z==0?0.0F:value.local.z));};
    const auto actor=[&](CrimeActorRef value){h.u64(static_cast<std::uint64_t>(value.kind));h.u64(value.npc.value());};
    h.u64(p.crimes.size());for(const auto& value:p.crimes){h.u64(value.id.value());h.u64(static_cast<std::uint64_t>(value.type));actor(value.actor);actor(value.victim);position(value.position);h.u64(value.tick);h.u64(value.severityBps);h.u64(static_cast<std::uint64_t>(value.observability));}
    h.u64(p.observations.size());for(const auto& value:p.observations){h.u64(value.crime.value());h.u64(value.witness.value());h.u64(value.tick);}
    h.u64(p.reports.size());for(const auto& value:p.reports){h.u64(value.id.value());h.u64(value.crime.value());h.u64(value.witness.value());h.u64(value.observedTick);h.u64(value.completionTick);h.u64(static_cast<std::uint64_t>(value.status));}
    h.u64(p.knowledge.size());for(const auto& value:p.knowledge){h.u64(value.crime.value());h.u64(value.suspectIdentified);actor(value.suspect);position(value.reportedPosition);h.u64(value.knownTick);h.u64(value.directObservation);h.u64(value.deliveredReports);}
    h.u64(p.processedCombatFacts.size());for(const auto& value:p.processedCombatFacts){h.u64(value.tick);h.u64(value.eventIndex);}
    const auto& w=p.wanted;h.u64(static_cast<std::uint64_t>(w.state));h.u64(w.heatBps);h.u64(w.suspectIdentified);h.u64(w.lastKnownPosition.has_value());if(w.lastKnownPosition)position(*w.lastKnownPosition);h.u64(w.lastConfirmedTick);h.u64(w.deadlineTick);h.u64(w.confidenceRadiusMeters);
    h.u64(p.responders.size());for(const auto& value:p.responders){h.u64(value.officer.value());h.u64(static_cast<std::uint64_t>(value.status));h.u64(value.dispatchTick);h.u64(value.arrivalTick);position(value.target);}
    h.u64(p.districtPressureBps.size());for(const auto& [id,value]:p.districtPressureBps){h.u64(id.value());h.u64(value);}
    h.u64(p.pendingArrest);position(p.arrestPosition);
}

} // namespace faultline::simulation
