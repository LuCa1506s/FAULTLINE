#pragma once
#include "simulation/player/player_state.hpp"
#include "simulation/runtime/game.hpp"
#include "world/spatial/streaming.hpp"

namespace faultline::simulation {
void initializePlayerState(GameState& state);
void restorePlayerResidency(GameState& state);
EntityId createPlayerEntity(GameState& state, WorldPosition spawnPosition);
void updatePlayerMovement(GameState& state, const PlayerInputFrame& input, world::WorldStreamingSystem& streaming);
void validatePlayerState(const GameState& state);
void appendPlayerStateHash(core::StateHash& hash, const GameState& state);
}
