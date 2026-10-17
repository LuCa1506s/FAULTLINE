#pragma once
#include "simulation/missions/mission_state.hpp"
#include "simulation/runtime/game_state.hpp"

namespace faultline::simulation {

// Initialization
void initializeMissionSimulation(GameState& state);

// State transition rules & queries
[[nodiscard]] bool isValidMissionTransition(MissionState from, MissionState to) noexcept;

// Pure condition evaluator
[[nodiscard]] bool evaluateCondition(const GameState& state, const ObjectiveCondition& condition,
                                     const ObjectiveRuntimeState* runtime = nullptr);

// Mission instantiation & lifecycle
MissionId createMission(GameState& state, const MissionDefinition& def,
                       const MissionContext& context = {},
                       StoryNodeId originStory = {},
                       const MissionIssuer& issuer = {});

void acceptMission(GameState& state, MissionId id, MissionEventBuffer& events);
void activateMission(GameState& state, MissionId id, MissionEventBuffer& events);
void completeMission(GameState& state, MissionId id, MissionEventBuffer& events);
void failMission(GameState& state, MissionId id, const std::string& reason, MissionEventBuffer& events);
void expireMission(GameState& state, MissionId id, MissionEventBuffer& events);
void invalidateMission(GameState& state, MissionId id, const std::string& reason, MissionEventBuffer& events);
void resolveMissionByNpc(GameState& state, MissionId id, const std::string& systemicCause, MissionEventBuffer& events);

// Headless semantic hooks for the 5 basic objective families
void notifyLocationReached(GameState& state, MissionId id, ObjectiveId objective,
                           const WorldPosition& pos, MissionEventBuffer& events);

void notifyInteractionOccurred(GameState& state, MissionId id, ObjectiveId objective,
                               NpcId npc, MissionEventBuffer& events);

void notifyDeliveryCompleted(GameState& state, MissionId id, ObjectiveId objective,
                             NpcId recipient, std::uint32_t payloadItemId, MissionEventBuffer& events);

void notifyProtectedTargetResolved(GameState& state, MissionId id, ObjectiveId objective,
                                   NpcId target, bool success, MissionEventBuffer& events);

void notifyAttackTargetResolved(GameState& state, MissionId id, ObjectiveId objective,
                                NpcId target, bool success, MissionEventBuffer& events);

// Simulation tick & domain event observation
void updateMissions(GameState& state, MissionEventBuffer& events);
void onNpcDied(GameState& state, NpcId npc, MissionEventBuffer& events);
void onTerritoryControlChanged(GameState& state, TerritoryId territory, MissionEventBuffer& events);
void onFactionOperationResolved(GameState& state, OperationId operation, MissionEventBuffer& events);

// Validation
void validateMissionDefinition(const MissionDefinition& def, const GameState& state);
void validateMissionInstance(const MissionInstance& instance, const GameState& state);
void validateMissionSimulation(const GameState& state);

// Hashing
void appendMissionSimulationHash(core::StateHash& hash, const GameState& state);

} // namespace faultline::simulation
