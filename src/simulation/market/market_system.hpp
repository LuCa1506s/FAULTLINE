#pragma once
#include "simulation/runtime/game_state.hpp"
namespace faultline::simulation {
void initializeMarketSimulation(GameState& state);
// Registration requires an existing semantic Dealer; never allocates manpower or respawns NPCs.
void registerMarketDealer(GameState& state,NpcId npc,TerritoryId territory,std::uint32_t capacity);
void disruptDistrictMarket(GameState& state,DistrictId district,std::uint32_t severityBps);
void updateMarkets(GameState& state);
// Recompute an initial/structural snapshot without paying or advancing time.
void refreshMarkets(GameState& state);
void validateMarketSimulation(const GameState& state);
void appendMarketSimulationHash(core::StateHash& hash,const GameState& state);
}
