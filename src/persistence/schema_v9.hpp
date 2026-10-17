#pragma once
#include "persistence/schema_v8.hpp"
#include "simulation/missions/mission_state.hpp"

namespace faultline::persistence::detail {

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionIssuer>
void fields(A& a, T& v) {
    a(v.kind, v.npc, v.faction, v.hidden);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionReward>
void fields(A& a, T& v) {
    a(v.cash, v.respect, v.factionFavor, v.favorAmount);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionContext>
void fields(A& a, T& v) {
    a(v.targetNpc, v.targetFaction, v.targetTerritory, v.targetDistrict,
      v.targetPosition, v.payloadItemId, v.recipientNpc);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::ObjectiveCondition>
void fields(A& a, T& v) {
    std::uint64_t ms = v.time.milliseconds;
    a(v.type, v.npc, v.faction, v.territory, v.district, v.position, v.radius,
      v.storyNode, v.storyOutcome, v.objective, v.threshold, ms);
    v.time.milliseconds = ms;
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::ObjectiveTransition>
void fields(A& a, T& v) {
    a(v.trigger, v.action, v.targetObjective);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::ObjectiveDefinition>
void fields(A& a, T& v) {
    a(v.id, v.type, v.description, v.targetNpc, v.targetFaction, v.targetTerritory,
      v.targetDistrict, v.targetPosition, v.targetRadius, v.payloadItemId,
      v.recipientNpc, v.startCondition, v.successCondition, v.failureCondition,
      v.transitions, v.optional);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::ObjectiveRuntimeState>
void fields(A& a, T& v) {
    std::uint64_t startMs = v.startedAt.milliseconds;
    std::uint64_t endMs = v.completedAt.milliseconds;
    a(v.state, startMs, endMs, v.locationReached, v.interactionOccurred,
      v.deliveryCompleted, v.protectResolved, v.attackResolved);
    v.startedAt.milliseconds = startMs;
    v.completedAt.milliseconds = endMs;
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionInstance>
void fields(A& a, T& v) {
    std::uint64_t createdMs = v.createdAt.milliseconds;
    std::optional<std::uint64_t> expiresMs;
    if (v.expiresAt.has_value()) expiresMs = v.expiresAt->milliseconds;

    a(v.id, v.templateId, v.category, v.status, v.issuer, v.originStoryNode,
      v.context, createdMs, expiresMs, v.reward, v.objectiveDefinitions);

    v.createdAt.milliseconds = createdMs;
    if (expiresMs.has_value()) v.expiresAt = core::GameTime{*expiresMs};
    else v.expiresAt.reset();

    // Map serialization
    if constexpr (std::is_same_v<A, Writer>) {
        a.collection(v.objectiveStates.size());
        for (const auto& [id, rState] : v.objectiveStates) {
            a(id, rState);
        }
    } else {
        const auto count = a.count();
        v.objectiveStates.clear();
        ObjectiveId prior{};
        for (std::uint32_t i = 0; i < count; ++i) {
            ObjectiveId id{};
            simulation::ObjectiveRuntimeState rState{};
            a(id, rState);
            if (!id || id <= prior) {
                a.fail(SaveErrorCode::InvalidData, "Null, duplicate or unordered objective runtime state ID");
            }
            prior = id;
            v.objectiveStates.emplace(id, rState);
        }
    }
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionSimulationConfig>
void fields(A& a, T& v) {
    a(v.maxActiveMissions, v.maxAvailableMissions);
}

template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::MissionSimulationStatistics>
void fields(A& a, T& v) {
    a(v.missionsCreated, v.missionsAccepted, v.missionsActive, v.missionsCompleted,
      v.missionsFailed, v.missionsExpired, v.missionsInvalidated, v.missionsResolvedByNpc);
}

} // namespace faultline::persistence::detail
