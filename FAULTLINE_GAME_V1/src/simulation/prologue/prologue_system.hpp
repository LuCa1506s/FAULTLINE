#pragma once
#include "simulation/prologue/prologue_state.hpp"
#include "simulation/missions/mission_system.hpp"

namespace faultline::simulation {

inline constexpr std::uint32_t primaryFirstDeliveryTemplateId = 1;
inline constexpr std::uint32_t fictionalProductToken = 1;

void initializePrologueState(GameState& state);

MissionId createFirstDeliveryMission(GameState& state, FactionId origin);

bool completeFirstDeliveryObjectives(GameState& state, MissionId missionId, MissionEventBuffer& events);

PrologueEvidence generatePrologueEvidence(GameState& state, FactionId origin, std::uint64_t deliveryTick);

void commitPrologueEvidence(GameState& state, const PrologueEvidence& evidence);

void runPrologue(GameState& state, FactionId origin = FactionId{1});

void validatePrologueState(const GameState& state);

void appendPrologueStateHash(core::StateHash& hash, const GameState& state);

void repairPrologueState(GameState& state);

}
