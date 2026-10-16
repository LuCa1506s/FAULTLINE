#include "simulation/factions/faction_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "world/generation/urban_generation.hpp"
#include <algorithm>
#include <tuple>

namespace faultline::simulation {
namespace {
bool eligible(const world::FactionState& faction) {
    return faction.lifecycle==world::FactionLifecycle::Active || faction.lifecycle==world::FactionLifecycle::Reformed;
}
std::uint64_t after(std::uint64_t tick, std::uint64_t duration) {
    return duration>core::SimulationClock::maxTick-tick ? neverTick : tick+duration;
}
std::uint64_t deadline(const FactionOperation& operation) {
    return operation.status==OperationStatus::Preparing ? operation.activeTick : operation.resolutionTick;
}
std::int64_t cost(const FactionSimulationConfig& config, OperationType type, std::uint32_t force) {
    return (type==OperationType::AttackTerritory ? config.attackBaseCost : config.defenseBaseCost)+config.costPerMember*force;
}
std::string context(const GameState& state, OperationId operation={}, FactionId faction={}, TerritoryId territory={}) {
    return " seed="+std::to_string(state.metadata.seed.value)+" generator="+std::to_string(state.metadata.generatorVersion.value)+
        " tick="+std::to_string(state.clock.tick())+" gameMs="+std::to_string(state.clock.gameTime().milliseconds)+
        " faction="+std::to_string(faction.value())+" operation="+std::to_string(operation.value())+
        " territory="+std::to_string(territory.value());
}
void check(bool condition, const char* message, const GameState& state, OperationId operation={}, FactionId faction={}, TerritoryId territory={}) {
    if (!condition) core::require(false,(std::string{message}+context(state,operation,faction,territory)).c_str());
}
OperationId defendingOperation(const GameState& state, OperationId attack) {
    for (auto id : state.factionSimulation.pendingOperations) {
        const auto& op=state.factionSimulation.operations.get(id);
        if (op.type==OperationType::DefendTerritory && op.threat==attack) return id;
    }
    return {};
}
bool adjacentToFaction(const GameState& state, FactionId faction, TerritoryId target) {
    const auto found=state.factionSimulation.adjacency.find(target);
    if (found==state.factionSimulation.adjacency.end()) return false;
    for (auto neighbor : found->second)
        if (state.world.territories.exists(neighbor) && controlsTerritory(state,faction,neighbor)) return true;
    return false;
}
void emit(FactionEventBuffer& events, OperationEventKind kind, OperationId id, const FactionOperation& op, std::uint64_t tick) {
    events.push({kind,id,op.faction,op.opponent,op.target,tick,op.result});
}
void transition(GameState& state, OperationId id, OperationStatus status, FactionEventBuffer& events, OperationResult result={}) {
    auto& system=state.factionSimulation; auto& op=system.operations.get(id);
    check(legalOperationTransition(op.status,status),"Illegal operation transition",state,id,op.faction,op.target);
    system.deadlines.erase({deadline(op),id});
    if (!pending(status)) {
        check(result.ownLosses<=op.commitment.manpower,"Loss exceeds committed force",state,id,op.faction,op.target);
        auto& faction=state.world.factions.get(op.faction);
        const auto survivors=op.commitment.manpower-result.ownLosses;
        check(survivors<=std::numeric_limits<std::uint32_t>::max()-faction.reserveMembers,"Reserve overflow",state,id,op.faction,op.target);
        faction.reserveMembers+=survivors; op.result=result; op.finishedTick=state.clock.tick(); system.pendingOperations.erase(id);
    }
    op.status=status;
    if (pending(status)) system.deadlines.emplace(deadline(op),id);
    const auto kind=status==OperationStatus::Active ? OperationEventKind::Started : status==OperationStatus::Resolved ? OperationEventKind::Resolved :
        status==OperationStatus::Failed ? OperationEventKind::Failed : OperationEventKind::Cancelled;
    emit(events,kind,id,op,state.clock.tick());
}
void morale(world::FactionState& faction, bool victory) {
    const auto bounded=[](std::uint16_t value, int delta) { return static_cast<std::uint16_t>(std::clamp(static_cast<int>(value)+delta,0,10000)); };
    faction.strategy.confidence=bounded(faction.strategy.confidence,victory ? 120 : -160);
    faction.cohesion=bounded(faction.cohesion,victory ? 25 : -30);
}
std::uint64_t quality(const world::FactionState& faction) {
    return 5000ULL+faction.cohesion/4+faction.strategy.confidence/4;
}
void resolveAttack(GameState& state, OperationId id, FactionEventBuffer& events) {
    auto& system=state.factionSimulation; const auto attack=system.operations.get(id);
    auto& territory=state.world.territories.get(attack.target);
    auto& attacker=state.world.factions.get(attack.faction);
    auto& rng=state.randomness.get(RandomDomain::Factions);
    const auto defenseId=defendingOperation(state,id);
    const auto defenseForce=defenseId ? system.operations.get(defenseId).commitment.manpower : 0U;
    const auto defenderQuality=attack.opponent ? quality(state.world.factions.get(attack.opponent)) : 0ULL;
    const auto attackStrength=attack.commitment.manpower*quality(attacker)*rng.uniform(9000,11001)/10000;
    const auto garrison=territoryGarrison(state,attack.target,attack.opponent);
    const auto defenseStrength=(static_cast<std::uint64_t>(garrison)*120/100+defenseForce)*
        defenderQuality*rng.uniform(9000,11001)/10000;
    const bool victory=attackStrength>defenseStrength;
    // Low aggregate attrition; named semantic support never adds aggregate force or suffers random death. There is no recruitment/revenue loop yet.
    const auto ownLoss=defenseStrength>0 && rng.chance(victory ? 1 : 2,4) ? std::min(attack.commitment.manpower,1+attack.commitment.manpower/12) : 0U;
    const auto garrisonLoss=garrison>0 && rng.chance(victory ? 2 : 1,4) ?
        std::min(garrison,1+attack.commitment.manpower/16) : 0U;
    const auto defenseLoss=defenseForce>0 && rng.chance(victory ? 2 : 1,4) ? std::min(defenseForce,1+attack.commitment.manpower/20) : 0U;
    territory.residentMembers-=garrisonLoss;
    // Emit evidence only. TerritorySystem consumes outcomes and derives control.
    const auto pressure=static_cast<std::uint16_t>(victory ? std::min(5000U,500+attack.commitment.manpower*100) : std::min(500U,attack.commitment.manpower*10));
    transition(state,id,victory ? OperationStatus::Resolved : OperationStatus::Failed,events,{ownLoss,garrisonLoss+defenseLoss,garrisonLoss,pressure});
    morale(attacker,victory);
    if (attack.opponent) morale(state.world.factions.get(attack.opponent),!victory);
    if (defenseId) transition(state,defenseId,victory ? OperationStatus::Failed : OperationStatus::Resolved,events,{defenseLoss,ownLoss,0,0});
}
}
void validateFactionConfig(const FactionSimulationConfig& c) {
    for (auto interval : {c.evaluationIntervalTicks,c.preparationTicks,c.activeDurationTicks,c.defensePreparationTicks,c.attackCooldownTicks})
        core::require(interval>0 && interval<=365*gameDayTicks,"Faction interval must be 1 tick..365 game days");
    core::require(c.defensePreparationTicks<c.activeDurationTicks,"Defense preparation must leave an active response window");
    core::require(c.minForce>0 && c.maxForce>=c.minForce && c.maxForce<=128 && c.maxConcurrentOperations>0 && c.maxConcurrentOperations<=8,
                  "Invalid faction force/capacity configuration");
    for (auto money : {c.attackBaseCost,c.defenseBaseCost,c.costPerMember,c.minimumAttackUtility})
        core::require(money>0 && money<=1000000000,"Faction cost/utility configuration outside bounds");
}
void initializeFactionSimulation(GameState& state) {
    state.factionSimulation.nextEvaluationTick=after(state.clock.tick(),state.factionSimulation.config.evaluationIntervalTicks);
    rebuildFactionIndexes(state);
}
void rebuildFactionIndexes(GameState& state) {
    auto& system=state.factionSimulation; system.pendingOperations.clear(); system.deadlines.clear(); system.adjacency.clear();
    if (state.world.base) {
        const auto& base=*state.world.base;
        // Loading rebuilds this O(T^2) index before general world validation.
        // Bound untrusted geometry by what supported generators can actually emit.
        world::validateConfig(base.config);
        core::require(base.districts.size()<=static_cast<std::uint64_t>(base.config.maxMacroZones)*3,
                      "Supported generator topology bounds exceeded (districts)");
        if (base.metadata.generatorVersion.value>=2) {
            world::validateUrbanConfig(base.config.urban);
            core::require(base.territories.size()<=base.districts.size()*base.config.urban.maxTerritoriesPerDistrict,
                          "Supported generator topology bounds exceeded (territories)");
        } else core::require(base.territories.empty(),"Generator v1 cannot contain territory geometry");
        system.adjacency=world::territoryAdjacency(base.territories);
    }
    for (const auto& [id,op] : system.operations.entries()) if (pending(op.status)) {
        system.pendingOperations.insert(id); system.deadlines.emplace(deadline(op),id);
    }
}
FactionResources factionResources(const GameState& state, FactionId id) {
    const auto& faction=state.world.factions.get(id); const auto& system=state.factionSimulation;
    FactionResources result; result.deployableManpower=faction.reserveMembers; result.totalManpower=faction.reserveMembers;
    std::uint64_t paid=0;
    for (auto operation : system.pendingOperations) {
        const auto& op=system.operations.get(operation); if (op.faction!=id) continue;
        result.committedManpower+=op.commitment.manpower; ++result.activeOperations; paid+=static_cast<std::uint64_t>(op.commitment.paidCost);
    }
    result.totalManpower+=result.committedManpower;
    for (const auto& [key,t] : state.world.territories.entries()) { (void)key; if (state.territorySimulation.territories.at(key).garrisonFaction==id) result.totalManpower+=t.residentMembers; }
    for (const auto& [key,n] : state.world.npcs.entries()) { (void)key; if (n.faction==id && n.lifecycle!=world::NpcLifecycle::Dead) ++result.totalManpower; }
    result.availableManpower=result.totalManpower-result.committedManpower;
    // Add already paid live commitments so spending on an operation does not invalidate its own capacity.
    const auto money=static_cast<std::uint64_t>(std::max<std::int64_t>(0,faction.treasury));
    const auto costCapacity=(money/ static_cast<std::uint64_t>(std::min(system.config.attackBaseCost,system.config.defenseBaseCost)+system.config.costPerMember*system.config.minForce));
    const auto paidCapacity=paid/static_cast<std::uint64_t>(std::min(system.config.attackBaseCost,system.config.defenseBaseCost)+system.config.costPerMember*system.config.minForce);
    result.operationalCapacity=static_cast<std::uint32_t>(std::min({static_cast<std::uint64_t>(system.config.maxConcurrentOperations),
        (static_cast<std::uint64_t>(faction.reserveMembers)+result.committedManpower)/system.config.minForce,costCapacity+paidCapacity}));
    return result;
}
std::uint64_t nextFactionDueTick(const GameState& state) {
    const auto& system=state.factionSimulation;
    return system.deadlines.empty() ? system.nextEvaluationTick : std::min(system.nextEvaluationTick,system.deadlines.begin()->first);
}
bool legalOperationTransition(OperationStatus from, OperationStatus to) {
    return (from==OperationStatus::Preparing && (to==OperationStatus::Active || to==OperationStatus::Cancelled)) ||
        (from==OperationStatus::Active && (to==OperationStatus::Resolved || to==OperationStatus::Failed || to==OperationStatus::Cancelled));
}
OperationLaunchResult launchFactionOperation(GameState& state, FactionId id, OperationCandidate candidate, FactionEventBuffer& events) {
    auto& system=state.factionSimulation; const auto& config=system.config;
    const auto reject=[](OperationRejection reason) { return OperationLaunchResult{{},reason}; };
    if (!state.world.factions.exists(id) || !eligible(state.world.factions.get(id))) return reject(OperationRejection::InvalidFaction);
    if (!state.world.territories.exists(candidate.target) || !system.adjacency.contains(candidate.target)) return reject(OperationRejection::InvalidTarget);
    if (candidate.type>OperationType::DefendTerritory) return reject(OperationRejection::InvalidType);
    auto& faction=state.world.factions.get(id);
    const auto now=state.clock.tick(); FactionId opponent=territoryDefender(state,candidate.target,id);
    if (opponent && !state.world.factions.exists(opponent)) return reject(OperationRejection::InvalidDefender);
    if (candidate.type==OperationType::AttackTerritory) {
        if (controlsTerritory(state,id,candidate.target)) return reject(OperationRejection::OwnTerritory);
        if (!adjacentToFaction(state,id,candidate.target)) return reject(OperationRejection::NotAdjacent);
        if (now<faction.strategy.nextAttackTick) return reject(OperationRejection::Cooldown);
        for (auto operation : system.pendingOperations) {
            const auto& op=system.operations.get(operation);
            if (op.type==OperationType::AttackTerritory && op.target==candidate.target) return reject(OperationRejection::DuplicateTarget);
        }
        if (candidate.threat) return reject(OperationRejection::InvalidThreat);
    } else {
        if (!territoryPresence(state,candidate.target).contains(id) || !candidate.threat || !system.operations.exists(candidate.threat)) return reject(OperationRejection::InvalidThreat);
        const auto& attack=system.operations.get(candidate.threat);
        if (!pending(attack.status) || attack.type!=OperationType::AttackTerritory || attack.target!=candidate.target ||
            attack.opponent!=id || attack.faction==id || defendingOperation(state,candidate.threat)) return reject(OperationRejection::InvalidThreat);
        opponent=attack.faction;
    }
    if (candidate.manpower<config.minForce || candidate.manpower>config.maxForce || candidate.manpower>faction.reserveMembers)
        return reject(OperationRejection::Manpower);
    const auto payment=cost(config,candidate.type,candidate.manpower);
    if (faction.treasury<payment) return reject(OperationRejection::Treasury);
    const auto resources=factionResources(state,id);
    if (resources.activeOperations>=resources.operationalCapacity) return reject(OperationRejection::Capacity);
    const auto activation=after(now,candidate.type==OperationType::AttackTerritory ? config.preparationTicks : config.defensePreparationTicks);
    const auto resolution=candidate.type==OperationType::AttackTerritory ?
        (activation==neverTick ? neverTick : after(activation,config.activeDurationTicks)) : system.operations.get(candidate.threat).resolutionTick;
    if (activation==neverTick || resolution==neverTick || activation>=resolution) return reject(OperationRejection::TimeLimit);
    if (system.operations.nextId()==0) return reject(OperationRejection::Capacity);
    FactionOperation op{id,opponent,candidate.type,OperationStatus::Preparing,candidate.target,candidate.threat,
        {candidate.manpower,payment},now,activation,resolution,0,{}};
    const auto operation=system.operations.create(op);
    faction.reserveMembers-=candidate.manpower; faction.treasury-=payment;
    if (candidate.type==OperationType::AttackTerritory) faction.strategy.nextAttackTick=after(now,config.attackCooldownTicks);
    system.pendingOperations.insert(operation); system.deadlines.emplace(activation,operation);
    emit(events,OperationEventKind::Created,operation,op,now); return {operation,OperationRejection::None};
}
void cancelFactionOperation(GameState& state, OperationId id, FactionEventBuffer& events) {
    const auto op=state.factionSimulation.operations.get(id);
    check(pending(op.status),"Cannot cancel terminal operation",state,id,op.faction,op.target);
    const auto defense=op.type==OperationType::AttackTerritory ? defendingOperation(state,id) : OperationId{};
    transition(state,id,OperationStatus::Cancelled,events);
    if (defense) transition(state,defense,OperationStatus::Cancelled,events);
}
std::vector<OperationCandidate> factionCandidates(const GameState& state, FactionId id) {
    std::vector<OperationCandidate> result;
    if (!state.world.factions.exists(id)) return result;
    const auto& faction=state.world.factions.get(id); const auto& system=state.factionSimulation; const auto& c=system.config;
    const auto resources=factionResources(state,id);
    if (!eligible(faction) || resources.activeOperations>=resources.operationalCapacity || faction.reserveMembers<c.minForce || system.operations.nextId()==0) return result;
    const auto affordableForce=[&](OperationType type) {
        const auto base=type==OperationType::AttackTerritory ? c.attackBaseCost : c.defenseBaseCost;
        if (faction.treasury<base) return 0U;
        const auto budget=static_cast<std::uint64_t>((faction.treasury-base)/c.costPerMember);
        return static_cast<std::uint32_t>(std::min({static_cast<std::uint64_t>(c.maxForce),static_cast<std::uint64_t>(faction.reserveMembers),
            static_cast<std::uint64_t>(std::max(c.minForce,6U+faction.personality.aggression/2500U)),budget}));
    };
    const auto force=affordableForce(OperationType::AttackTerritory);
    const auto attackCost=cost(c,OperationType::AttackTerritory,force);
    const auto activation=after(state.clock.tick(),c.preparationTicks);
    const bool attackTimeFits=activation!=neverTick && after(activation,c.activeDurationTicks)!=neverTick;
    if (force>=c.minForce && state.clock.tick()>=faction.strategy.nextAttackTick && attackTimeFits) {
        for (const auto& [target,t] : state.world.territories.entries()) {
            (void)t; if (controlsTerritory(state,id,target) || !adjacentToFaction(state,id,target)) continue;
            const auto opponent=territoryDefender(state,target,id);
            bool occupied=false;
            for (auto operation : system.pendingOperations) {
                const auto& op=system.operations.get(operation); occupied |= op.type==OperationType::AttackTerritory && op.target==target;
            }
            if (occupied) continue;
            std::int64_t relation=0;
            if (opponent) for (const auto& r : state.world.factionRelationships) if (r.from==id && r.to==opponent)
                relation=static_cast<std::int64_t>(r.hostility)/3-static_cast<std::int64_t>(r.trust)/5-static_cast<std::int64_t>(r.sharedInterest)/10;
            const auto attackPower=force*quality(faction);
            const auto defensePower=opponent ? static_cast<std::uint64_t>(territoryGarrison(state,target,opponent))*quality(state.world.factions.get(opponent))*120/100 : 0;
            const auto ratio=static_cast<std::int64_t>(attackPower*1000/(attackPower+defensePower));
            std::int64_t value=0;
            if (state.world.base) for (const auto& geometry : state.world.base->territories) if (geometry.id==target)
                value=std::min<std::int64_t>(500,geometry.buildingCount);
            const auto risk=(1000-ratio)*(10000-faction.personality.riskTolerance)/2500;
            const auto utility=1200+faction.personality.aggression/3+faction.personality.territorialFocus/5+
                faction.strategy.confidence/10+faction.cohesion/20+relation+value+ratio*2-risk-attackCost/4;
            if (utility>=c.minimumAttackUtility) result.push_back({OperationType::AttackTerritory,target,{},force,utility});
        }
    }
    const auto defenseForce=affordableForce(OperationType::DefendTerritory);
    for (auto operation : system.pendingOperations) {
        const auto& attack=system.operations.get(operation);
        if (attack.type!=OperationType::AttackTerritory || attack.opponent!=id || !territoryPresence(state,attack.target).contains(id) ||
            defendingOperation(state,operation) || after(state.clock.tick(),c.defensePreparationTicks)>=attack.resolutionTick) continue;
        if (defenseForce<c.minForce) continue;
        const auto payment=cost(c,OperationType::DefendTerritory,defenseForce);
        const auto utility=12000+(attack.target==faction.homeTerritory ? 8000 : 0)+attack.commitment.manpower*100+faction.cohesion/10-payment/4;
        if (utility>0) result.push_back({OperationType::DefendTerritory,attack.target,operation,defenseForce,utility});
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        if (a.utility!=b.utility) return a.utility>b.utility;
        return std::tie(a.type,a.target,a.threat)<std::tie(b.type,b.target,b.threat);
    }); return result;
}
void reconcileFactionOperations(GameState& state, FactionEventBuffer& events) {
    auto& system=state.factionSimulation;
    std::vector<OperationId> invalid;
    for (auto id : system.pendingOperations) {
        const auto& op=system.operations.get(id);
        const bool validOwner=op.type==OperationType::AttackTerritory ?
            (!controlsTerritory(state,op.faction,op.target) && adjacentToFaction(state,op.faction,op.target) && territoryDefender(state,op.target,op.faction)==op.opponent) :
            (territoryPresence(state,op.target).contains(op.faction) && pending(system.operations.get(op.threat).status));
        if (!validOwner || !eligible(state.world.factions.get(op.faction))) invalid.push_back(id);
    }
    for (auto id : invalid) if (pending(system.operations.get(id).status)) cancelFactionOperation(state,id,events);
}
void updateFactionOperations(GameState& state,FactionEventBuffer& events) {
    const auto now=state.clock.tick();
    if (now<nextFactionDueTick(state)) return;
    reconcileFactionOperations(state,events);
    auto& system=state.factionSimulation;
    std::vector<OperationId> due;
    for (auto it=system.deadlines.begin();it!=system.deadlines.end() && it->first<=now;++it) due.push_back(it->second);
    std::sort(due.begin(),due.end());
    for (auto id : due) {
        const auto op=system.operations.get(id); if (!pending(op.status)) continue;
        if (op.status==OperationStatus::Preparing) transition(state,id,OperationStatus::Active,events);
        else if (op.type==OperationType::AttackTerritory) resolveAttack(state,id,events);
        else cancelFactionOperation(state,id,events); // An orphan response never keeps a permanent commitment.
    }
}
void evaluateFactions(GameState& state,FactionEventBuffer& events) {
    auto& system=state.factionSimulation; const auto now=state.clock.tick();
    if (now>=system.nextEvaluationTick) {
        system.nextEvaluationTick=after(now,system.config.evaluationIntervalTicks);
        for (const auto& [id,record] : state.world.factions.entries()) {
            (void)record; auto& faction=state.world.factions.get(id); auto& strategy=faction.strategy;
            ++strategy.evaluations; strategy.lastEvaluationTick=now; strategy.lastTarget={}; strategy.lastUtility=0;
            const auto candidates=factionCandidates(state,id);
            if (!candidates.empty()) {
                const auto& selected=candidates.front(); const auto launched=launchFactionOperation(state,id,selected,events);
                check(static_cast<bool>(launched),"Viable utility candidate failed launch",state,{},id,selected.target);
                strategy.lastTarget=selected.target; strategy.lastUtility=selected.utility;
            }
        }
    }
}
void updateFactions(GameState& state,FactionEventBuffer& events) {
    updateFactionOperations(state,events);
    TerritoryEventBuffer territoryEvents;
    applyTerritoryConsequences(state,events.values(),territoryEvents);
    reconcileFactionOperations(state,events); evaluateFactions(state,events);
}
void validateFactionSimulation(const GameState& state) {
    const auto& system=state.factionSimulation; validateFactionConfig(system.config); const auto now=state.clock.tick();
    check(system.nextEvaluationTick==neverTick || (system.nextEvaluationTick>=now && system.nextEvaluationTick<=core::SimulationClock::maxTick),
          "Invalid strategic scheduler deadline",state);
    std::set<OperationId> expectedPending; std::set<std::pair<std::uint64_t,OperationId>> expectedDeadlines;
    std::set<TerritoryId> attackTargets; std::set<OperationId> defendedThreats;
    for (const auto& [id,op] : system.operations.entries()) {
        const auto valid=[&](bool condition,const char* message){check(condition,message,state,id,op.faction,op.target);};
        valid(static_cast<bool>(id) && state.world.factions.exists(op.faction) && state.world.territories.exists(op.target),"Invalid operation references");
        valid(!op.opponent || (state.world.factions.exists(op.opponent) && op.opponent!=op.faction),"Invalid operation opponent");
        valid(op.type<=OperationType::DefendTerritory && op.status<=OperationStatus::Cancelled,"Invalid operation type/status");
        valid(op.commitment.manpower>=system.config.minForce && op.commitment.manpower<=system.config.maxForce &&
            op.commitment.paidCost==cost(system.config,op.type,op.commitment.manpower),"Invalid operation commitment");
        valid(op.createdTick<=now && op.createdTick<op.activeTick && op.activeTick<op.resolutionTick &&
            op.resolutionTick<=core::SimulationClock::maxTick,"Invalid operation times");
        valid(op.result.ownLosses<=op.commitment.manpower && op.result.strategicPressure<=10000 &&
            op.result.opposingLosses<=system.config.maxForce*2 && op.result.garrisonLosses<=system.config.maxForce &&
            op.result.garrisonLosses<=op.result.opposingLosses,"Invalid operation result");
        if (op.type==OperationType::DefendTerritory) {
            valid(static_cast<bool>(op.threat) && op.threat<id && system.operations.exists(op.threat),"Invalid defense threat reference");
            const auto& attack=system.operations.get(op.threat);
            valid(attack.type==OperationType::AttackTerritory && attack.target==op.target && attack.faction==op.opponent &&
                attack.opponent==op.faction && attack.resolutionTick==op.resolutionTick,"Incoherent defense relationship");
            if (pending(op.status)) valid(pending(attack.status) && defendedThreats.insert(op.threat).second,"Duplicate/orphan live defense");
        } else valid(!op.threat,"Attack cannot reference another attack");
        if (pending(op.status)) {
            valid(eligible(state.world.factions.get(op.faction)),"Inactive faction has a live operation");
            expectedPending.insert(id); expectedDeadlines.emplace(deadline(op),id);
            valid(deadline(op)>=now && op.finishedTick==0 && op.result==OperationResult{},"Invalid pending operation state");
            if (op.status==OperationStatus::Active) valid(op.activeTick<=now,"Active operation was not started on its tick");
            valid(op.type==OperationType::AttackTerritory ? (!controlsTerritory(state,op.faction,op.target) && territoryDefender(state,op.target,op.faction)==op.opponent) : territoryPresence(state,op.target).contains(op.faction),"Live operation target control/presence mismatch");
            if (op.type==OperationType::AttackTerritory) {
                valid(adjacentToFaction(state,op.faction,op.target),"Unreachable attack target");
                valid(attackTargets.insert(op.target).second,"Duplicate live attack target");
            }
        } else {
            valid(op.finishedTick>=op.createdTick && op.finishedTick<=now,"Invalid completed operation time");
            if (op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed)
                valid(op.finishedTick>=op.resolutionTick,"Operation resolved before deadline");
            if (op.status==OperationStatus::Cancelled) valid(op.result==OperationResult{},"Cancelled operation has resolution effects");
        }
    }
    check(system.pendingOperations==expectedPending && system.deadlines==expectedDeadlines,"Stale operation indexes",state);
    const auto expectedAdjacency=state.world.base ? world::territoryAdjacency(state.world.base->territories) : world::TerritoryAdjacency{};
    check(system.adjacency==expectedAdjacency,"Stale territory adjacency",state);
    for (const auto& [id,faction] : state.world.factions.entries()) {
        const auto r=factionResources(state,id); const auto& strategy=faction.strategy;
        check(strategy.confidence<=10000 && strategy.lastEvaluationTick<=now && strategy.evaluations<=now+1 &&
            strategy.lastUtility>=0 && strategy.lastUtility<=1000000 && static_cast<bool>(strategy.lastTarget)==(strategy.lastUtility>0) &&
            (!strategy.lastTarget || state.world.territories.exists(strategy.lastTarget)),"Invalid faction strategic state",state,{},id);
        check(strategy.nextAttackTick==neverTick || strategy.nextAttackTick<=core::SimulationClock::maxTick,"Invalid faction cooldown",state,{},id);
        check(r.committedManpower<=r.totalManpower && r.availableManpower==r.totalManpower-r.committedManpower &&
            r.activeOperations<=r.operationalCapacity && static_cast<std::uint64_t>(faction.reserveMembers)+r.committedManpower<=std::numeric_limits<std::uint32_t>::max(),
            "Invalid faction resource commitments",state,{},id);
    }
}
void appendFactionSimulationHash(core::StateHash& h, const GameState& state) {
    const auto& s=state.factionSimulation; const auto& c=s.config;
    for (auto value : {c.evaluationIntervalTicks,c.preparationTicks,c.activeDurationTicks,c.defensePreparationTicks,c.attackCooldownTicks}) h.u64(value);
    for (auto value : {c.minForce,c.maxForce,c.maxConcurrentOperations}) h.u64(value);
    for (auto value : {c.attackBaseCost,c.defenseBaseCost,c.costPerMember,c.minimumAttackUtility}) h.u64(static_cast<std::uint64_t>(value));
    h.u64(s.nextEvaluationTick); h.u64(s.operations.nextId()); h.u64(s.operations.entries().size());
    for (const auto& [id,op] : s.operations.entries()) {
        h.u64(id.value()); h.u64(op.faction.value()); h.u64(op.opponent.value()); h.u64(static_cast<std::uint64_t>(op.type));
        h.u64(static_cast<std::uint64_t>(op.status)); h.u64(op.target.value()); h.u64(op.threat.value());
        h.u64(op.commitment.manpower); h.u64(static_cast<std::uint64_t>(op.commitment.paidCost));
        h.u64(op.createdTick); h.u64(op.activeTick); h.u64(op.resolutionTick); h.u64(op.finishedTick);
        h.u64(op.result.ownLosses); h.u64(op.result.opposingLosses); h.u64(op.result.garrisonLosses); h.u64(op.result.strategicPressure);
    }
    h.u64(state.world.factions.entries().size());
    for (const auto& [id,faction] : state.world.factions.entries()) {
        const auto& r=faction.strategy;
        h.u64(id.value()); h.u64(r.confidence); h.u64(r.evaluations); h.u64(r.lastEvaluationTick); h.u64(r.nextAttackTick);
        h.u64(r.lastTarget.value()); h.u64(static_cast<std::uint64_t>(r.lastUtility));
    }
}
}
