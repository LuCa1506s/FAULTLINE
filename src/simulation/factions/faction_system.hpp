#pragma once
#include "simulation/runtime/game_state.hpp"

namespace faultline::simulation {
void validateFactionConfig(const FactionSimulationConfig& config);
// Initialization/migration consumes no RNG and changes no generated baseline data.
void initializeFactionSimulation(GameState& state);
void rebuildFactionIndexes(GameState& state);
void validateFactionSimulation(const GameState& state);
void appendFactionSimulationHash(core::StateHash& hash, const GameState& state);
[[nodiscard]] FactionResources factionResources(const GameState& state, FactionId faction);
[[nodiscard]] std::uint64_t nextFactionDueTick(const GameState& state);
[[nodiscard]] std::vector<OperationCandidate> factionCandidates(const GameState& state, FactionId faction);
[[nodiscard]] OperationLaunchResult launchFactionOperation(GameState& state, FactionId faction,
    OperationCandidate candidate, FactionEventBuffer& events);
void cancelFactionOperation(GameState& state, OperationId operation, FactionEventBuffer& events);
[[nodiscard]] bool legalOperationTransition(OperationStatus from, OperationStatus to);
// Same scheduled system for normal fixed ticks and headless time advancement.
void reconcileFactionOperations(GameState& state, FactionEventBuffer& events);
void updateFactionOperations(GameState& state, FactionEventBuffer& events);
void evaluateFactions(GameState& state, FactionEventBuffer& events);
void updateFactions(GameState& state, FactionEventBuffer& events);
}
