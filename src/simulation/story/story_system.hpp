#pragma once
#include "simulation/runtime/game_state.hpp"
namespace faultline::simulation {
// Deterministic initial projection and incremental repair. Neither consumes RNG
// nor writes world state. Repeated repair is idempotent; historical outcomes stay.
// Initialization is explicit after completed history. Foreign semantic mutations
// outside Simulation must call repair; availability always reads live world facts.
void initializeStoryGraph(GameState& state);
void repairStoryGraph(GameState& state);
[[nodiscard]] StoryAvailability storyAvailability(const GameState& state,StoryNodeId node);
// Integration hook for a future owning system's resolution; no rewards/effects.
void resolveStoryNode(GameState& state,StoryNodeId node,StoryOutcome outcome);
void validateStoryGraph(const GameState& state);
void appendStoryGraphHash(core::StateHash& hash,const GameState& state);
}
