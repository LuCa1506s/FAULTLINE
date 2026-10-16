#include "simulation/territory/territory_system.hpp"
#include <algorithm>

namespace faultline::simulation {
namespace {
void count(std::uint64_t& value) { core::require(value!=neverTick,"Territory statistics overflow"); ++value; }
void change(GameState& state,TerritoryId t,FactionId f,std::int64_t delta) {
    if (!f || delta==0) return;
    auto& values=state.territorySimulation.territories.at(t).entrenchment;
    const auto found=values.find(f); const auto old=found==values.end() ? 0U : found->second;
    const auto value=static_cast<std::uint32_t>(std::clamp(static_cast<std::int64_t>(old)+delta,
        std::int64_t{0},static_cast<std::int64_t>(state.territorySimulation.rules.maximumPresence)));
    if (value) values[f]=value; else values.erase(f);
}
}
void validateTerritoryRules(const TerritoryRules& r) {
    core::require(r.maximumPresence>0 && r.maximumPresence<=1000000 && r.minimumControlPresence>0 &&
        r.minimumControlPresence<=r.maximumPresence && r.contestThreshold>0 && r.contestThreshold<=r.minimumControlPresence &&
        r.holdMargin>0 && r.holdMargin<r.gainMargin && r.gainMargin<=r.maximumPresence &&
        r.garrisonUnit>0 && r.garrisonUnit<=1000 && r.initialEntrenchment>=r.minimumControlPresence &&
        r.initialEntrenchment<=r.maximumPresence && r.homeEntrenchment<=r.maximumPresence-r.initialEntrenchment &&
        r.attackPressureDivisor>0 && r.attackPressureDivisor<=10000 && r.defenseSurvivorUnit>0 && r.defenseSurvivorUnit<=1000,
        "Invalid territory rules");
}
std::map<FactionId,std::uint32_t> territoryPresence(const GameState& state,TerritoryId id) {
    const auto& r=state.territorySimulation.rules; const auto& t=state.territorySimulation.territories.at(id);
    auto values=t.entrenchment;
    if (const auto members=state.world.territories.get(id).residentMembers; members && t.garrisonFaction) {
        const auto sum=static_cast<std::uint64_t>(values[t.garrisonFaction])+static_cast<std::uint64_t>(members)*r.garrisonUnit;
        values[t.garrisonFaction]=static_cast<std::uint32_t>(std::min<std::uint64_t>(r.maximumPresence,sum));
    }
    return values;
}
TerritoryControl evaluateTerritoryControl(const TerritoryRules& r,const std::map<FactionId,std::uint32_t>& values,FactionId incumbent) {
    FactionId top; std::uint32_t first=0,second=0;
    for (const auto& [id,value] : values) {
        if (value>first) { second=first; first=value; top=id; }
        else second=std::max(second,value);
    }
    if (first<r.minimumControlPresence) return {};
    const auto margin=top==incumbent ? r.holdMargin : r.gainMargin;
    if (second>=r.contestThreshold && first-second<margin) return {TerritoryControlState::Contested,{}};
    return {TerritoryControlState::Controlled,top};
}
TerritoryControl territoryControl(const GameState& state,TerritoryId t) {
    return evaluateTerritoryControl(state.territorySimulation.rules,territoryPresence(state,t),
        state.territorySimulation.territories.at(t).incumbent);
}
bool controlsTerritory(const GameState& state,FactionId f,TerritoryId t) {
    const auto c=territoryControl(state,t); return f && c.state==TerritoryControlState::Controlled && c.controller==f;
}
std::uint32_t territoryGarrison(const GameState& state,TerritoryId t,FactionId f) {
    return f && state.territorySimulation.territories.at(t).garrisonFaction==f ? state.world.territories.get(t).residentMembers : 0;
}
FactionId territoryDefender(const GameState& state,TerritoryId t,FactionId attacker) {
    const auto& runtime=state.territorySimulation.territories.at(t);
    const auto c=territoryControl(state,t); if (c.controller && c.controller!=attacker) return c.controller;
    // In contested/uncontrolled regions, surviving local members identify the
    // primary opponent. Residual foreign garrisons never hide a current controller.
    if (runtime.garrisonFaction && runtime.garrisonFaction!=attacker && state.world.territories.get(t).residentMembers) return runtime.garrisonFaction;
    FactionId best; std::uint32_t strength=0;
    for (const auto& [id,value] : territoryPresence(state,t)) if (id!=attacker && value>strength) { best=id; strength=value; }
    return best;
}
std::size_t controlledTerritoryCount(const GameState& state,FactionId f) {
    std::size_t count=0;
    for (const auto& [id,t] : state.world.territories.entries()) { (void)t; if (controlsTerritory(state,f,id)) ++count; }
    return count;
}
void reevaluateTerritoryControl(GameState& state,TerritoryId id,TerritoryEventBuffer& events) {
    auto& runtime=state.territorySimulation.territories.at(id);
    const auto old=state.territorySimulation.controlCache.at(id);
    const auto current=territoryControl(state,id);
    runtime.incumbent=current.controller;
    state.world.territories.get(id).owner=current.controller; // Legacy mirror only; never read as control authority.
    state.territorySimulation.controlCache[id]=current;
    if (old!=current) {
        auto& stats=state.territorySimulation.statistics; count(stats.transitions);
        if (old.controller!=current.controller) count(stats.controlChanges);
        if (current.state==TerritoryControlState::Contested) count(stats.contestedTransitions);
        if (current.state==TerritoryControlState::Uncontrolled) count(stats.uncontrolledTransitions);
        events.push({id,old,current,state.clock.tick()});
    }
}
void rebuildTerritoryControlCache(GameState& state) {
    auto& s=state.territorySimulation; s.controlCache.clear();
    for (const auto& [id,t] : s.territories) { (void)t; s.controlCache.emplace(id,territoryControl(state,id)); }
}
void initializeTerritorySimulation(GameState& state) {
    auto& s=state.territorySimulation; validateTerritoryRules(s.rules);
    s.territories.clear(); s.controlCache.clear();
    for (const auto& [id,t] : state.world.territories.entries()) {
        TerritoryRuntimeState runtime; runtime.garrisonFaction=t.owner; runtime.incumbent=t.owner;
        if (t.owner) {
            auto value=s.rules.initialEntrenchment;
            if (state.world.factions.exists(t.owner) && state.world.factions.get(t.owner).homeTerritory==id) value+=s.rules.homeEntrenchment;
            runtime.entrenchment.emplace(t.owner,value);
        }
        s.territories.emplace(id,std::move(runtime));
    }
    rebuildTerritoryControlCache(state);
    for (const auto& [id,t] : state.world.territories.entries()) {
        const auto control=s.controlCache.at(id);
        core::require(control.controller==t.owner && control.state==(t.owner ? TerritoryControlState::Controlled : TerritoryControlState::Uncontrolled),
            "Territory initialization changed the imported ownership map");
    }
}
void markHistoricalTerritoryResults(GameState& state) {
    auto& done=state.territorySimulation.appliedOperations; done.clear();
    for (const auto& [id,op] : state.factionSimulation.operations.entries())
        if (op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed) done.insert(id);
}
void applyTerritoryConsequences(GameState& state,std::span<const FactionOperationEvent> effects,TerritoryEventBuffer& events) {
    auto& s=state.territorySimulation; const auto& r=s.rules; std::set<TerritoryId> changed;
    std::set<OperationId> ordered;
    for (const auto& event : effects) if (event.kind==OperationEventKind::Resolved || event.kind==OperationEventKind::Failed) ordered.insert(event.operation);
    for (auto id : ordered) {
        core::require(state.factionSimulation.operations.exists(id),"Unknown territory operation effect");
        const auto& op=state.factionSimulation.operations.get(id);
        core::require(op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed,"Nonterminal territory effect");
        core::require(op.type<=OperationType::DefendTerritory && op.commitment.manpower>0 &&
            op.result.ownLosses<=op.commitment.manpower && state.world.territories.exists(op.target) &&
            state.world.factions.exists(op.faction) && (!op.opponent || state.world.factions.exists(op.opponent)),
            "Invalid territorial operation evidence");
        if (s.appliedOperations.contains(id)) continue;
        const auto survivors=op.commitment.manpower-op.result.ownLosses;
        if (op.type==OperationType::AttackTerritory) {
            if (op.status==OperationStatus::Resolved) {
                const auto earned=static_cast<std::uint64_t>(op.result.strategicPressure)*survivors/op.commitment.manpower/r.attackPressureDivisor;
                change(state,op.target,op.faction,static_cast<std::int64_t>(earned));
                change(state,op.target,op.opponent,-static_cast<std::int64_t>(earned));
            } else change(state,op.target,op.faction,-static_cast<std::int64_t>(op.result.ownLosses)*r.garrisonUnit);
        } else {
            const auto reinforcement=static_cast<std::int64_t>(survivors)*r.defenseSurvivorUnit;
            change(state,op.target,op.faction,reinforcement);
            change(state,op.target,op.opponent,-reinforcement/(op.status==OperationStatus::Resolved ? 1 : 2));
        }
        s.appliedOperations.insert(id); changed.insert(op.target);
    }
    // One logical transition per territory after the entire simultaneous outcome,
    // not a transient attacker win followed by a second defense transition.
    for (auto t : changed) reevaluateTerritoryControl(state,t,events);
}
void validateTerritorySimulation(const GameState& state) {
    const auto& s=state.territorySimulation; validateTerritoryRules(s.rules);
    core::require(s.territories.size()==state.world.territories.entries().size() && s.controlCache.size()==s.territories.size(),"Territory runtime coverage mismatch");
    for (const auto& [id,t] : s.territories) {
        const auto require=[&](bool valid,const char* reason){
            core::require(valid,(std::string{reason}+" seed="+std::to_string(state.metadata.seed.value)+" generator="+std::to_string(state.metadata.generatorVersion.value)+" tick="+std::to_string(state.clock.tick())+
                " gameMs="+std::to_string(state.clock.gameTime().milliseconds)+" territory="+std::to_string(id.value())).c_str());
        };
        require(state.world.territories.exists(id),"Missing territory");
        require(!t.garrisonFaction || state.world.factions.exists(t.garrisonFaction),"Invalid garrison faction");
        require(state.world.territories.get(id).residentMembers==0 || static_cast<bool>(t.garrisonFaction),"Garrison manpower without membership");
        require(!t.incumbent || state.world.factions.exists(t.incumbent),"Invalid control latch");
        for (const auto& [f,value] : t.entrenchment) require(state.world.factions.exists(f) && value>0 && value<=s.rules.maximumPresence,"Invalid territorial presence");
        const auto control=territoryControl(state,id);
        require(control.controller==t.incumbent && state.world.territories.get(id).owner==control.controller &&
            s.controlCache.contains(id) && s.controlCache.at(id)==control,"Control cache/latch diverges from presence");
    }
    for (auto id : s.appliedOperations) {
        core::require(state.factionSimulation.operations.exists(id),"Applied territory result missing operation");
        const auto& op=state.factionSimulation.operations.get(id);
        core::require(op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed,"Applied nonterminal territory result");
    }
    for (const auto& [id,op] : state.factionSimulation.operations.entries())
        if (op.status==OperationStatus::Resolved || op.status==OperationStatus::Failed)
            core::require(s.appliedOperations.contains(id),"Unconsumed territorial consequence");
    const auto& stats=s.statistics;
    core::require(stats.controlChanges<=stats.transitions && stats.contestedTransitions<=stats.transitions && stats.uncontrolledTransitions<=stats.transitions,"Invalid territory statistics");
}
void appendTerritorySimulationHash(core::StateHash& h,const GameState& state) {
    const auto& s=state.territorySimulation; const auto& r=s.rules;
    for (auto v : {r.maximumPresence,r.minimumControlPresence,r.contestThreshold,r.gainMargin,r.holdMargin,r.garrisonUnit,
        r.initialEntrenchment,r.homeEntrenchment,r.attackPressureDivisor,r.defenseSurvivorUnit}) h.u64(v);
    h.u64(s.territories.size());
    for (const auto& [id,t] : s.territories) {
        const auto control=territoryControl(state,id);
        h.u64(id.value()); h.u64(t.garrisonFaction.value()); h.u64(t.incumbent.value());
        h.u64(static_cast<std::uint64_t>(control.state)); h.u64(control.controller.value()); h.u64(t.entrenchment.size());
        for (const auto& [f,value] : t.entrenchment) {h.u64(f.value());h.u64(value);}
    }
    h.u64(s.appliedOperations.size()); for (auto id : s.appliedOperations) h.u64(id.value());
    h.u64(s.statistics.transitions);h.u64(s.statistics.controlChanges);h.u64(s.statistics.contestedTransitions);h.u64(s.statistics.uncontrolledTransitions);
}
}
