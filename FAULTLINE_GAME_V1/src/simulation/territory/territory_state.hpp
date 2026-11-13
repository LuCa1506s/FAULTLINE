#pragma once
#include "simulation/factions/faction_state.hpp"

namespace faultline::simulation {
enum class TerritoryControlState : std::uint8_t { Controlled, Contested, Uncontrolled };
struct TerritoryControl {
    TerritoryControlState state{TerritoryControlState::Uncontrolled};
    FactionId controller;
    auto operator<=>(const TerritoryControl&) const = default;
};
struct TerritoryRules {
    std::uint32_t maximumPresence{10000}, minimumControlPresence{100}, contestThreshold{60};
    std::uint32_t gainMargin{80}, holdMargin{60};
    std::uint32_t garrisonUnit{20}, initialEntrenchment{120}, homeEntrenchment{100};
    std::uint32_t attackPressureDivisor{20}, defenseSurvivorUnit{5};
    std::uint32_t resistanceCasualtyUnit{5}, resistanceReliefUnit{2};
    auto operator<=>(const TerritoryRules&) const = default;
};
struct TerritoryRuntimeState {
    // Authority: causal entrenchment, concrete garrison membership and local
    // resistance. Effective presence is derived; entrenchment is influence, never additional manpower.
    std::map<FactionId,std::uint32_t> entrenchment;
    FactionId garrisonFaction;
    // Minimal hysteresis memory. Cleared on loss; contenders must earn gainMargin.
    FactionId incumbent;
    // Local opposition caused by territorial violence; never faction manpower.
    std::uint32_t localResistance{};
};
struct TerritoryStatistics {
    std::uint64_t transitions{}, controlChanges{}, contestedTransitions{}, uncontrolledTransitions{};
};
struct TerritorySimulationState {
    TerritoryRules rules;
    std::map<TerritoryId,TerritoryRuntimeState> territories;
    std::set<OperationId> appliedOperations; // Persisted exactly-once result consumption.
    TerritoryStatistics statistics;
    std::map<TerritoryId,TerritoryControl> controlCache; // DERIVED, not serialized or hashed.
};
struct TerritoryControlEvent {
    TerritoryId territory;
    TerritoryControl previous, current;
    std::uint64_t tick{};
};
using TerritoryEventBuffer=core::OrderedBuffer<TerritoryControlEvent>;
}
