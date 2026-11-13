#pragma once
#include "simulation/runtime/game_state.hpp"
namespace faultline::simulation {
// Deterministic candidate for tools / future prologue setup. Does not complete a mission.
[[nodiscard]] PrologueEvidence historyPrologueCandidate(const GameState& state,FactionId origin);
void beginStrategicHistory(GameState& state,const PrologueEvidence& evidence);
// One compressed month; can be saved between calls. Exactly 48 months / 1460 days.
void advanceStrategicHistory(GameState& state);
void validateStrategicHistory(const GameState& state);
void validatePostTimeskipWorld(const GameState& state);
void appendStrategicHistoryHash(core::StateHash& hash,const GameState& state);
}
