#pragma once
#include "simulation/runtime/game_state.hpp"
#include <span>
namespace faultline::simulation {
void validateTerritoryRules(const TerritoryRules& rules);
void initializeTerritorySimulation(GameState& state);
void rebuildTerritoryControlCache(GameState& state);
void markHistoricalTerritoryResults(GameState& state);
[[nodiscard]] std::map<FactionId,std::uint32_t> territoryPresence(const GameState& state,TerritoryId territory);
[[nodiscard]] TerritoryControl evaluateTerritoryControl(const TerritoryRules& rules,
    const std::map<FactionId,std::uint32_t>& presence,FactionId incumbent = {});
[[nodiscard]] TerritoryControl territoryControl(const GameState& state,TerritoryId territory);
[[nodiscard]] bool controlsTerritory(const GameState& state,FactionId faction,TerritoryId territory);
[[nodiscard]] FactionId territoryDefender(const GameState& state,TerritoryId territory,FactionId attacker);
[[nodiscard]] std::uint32_t territoryGarrison(const GameState& state,TerritoryId territory,FactionId faction);
[[nodiscard]] std::size_t controlledTerritoryCount(const GameState& state,FactionId faction);
void reevaluateTerritoryControl(GameState& state,TerritoryId territory,TerritoryEventBuffer& events);
void applyTerritoryConsequences(GameState& state,std::span<const FactionOperationEvent> effects,TerritoryEventBuffer& events);
void validateTerritorySimulation(const GameState& state);
void appendTerritorySimulationHash(core::StateHash& hash,const GameState& state);
}
