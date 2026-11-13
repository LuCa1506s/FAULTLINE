#include "simulation/missions/mission_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/story/story_system.hpp"
#include "core/diagnostics/diagnostics.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>

namespace faultline::simulation {

namespace {

[[nodiscard]] float distance2D(const WorldPosition& a, const WorldPosition& b, float cellSize) {
    const float dx = static_cast<float>(a.cell.x - b.cell.x) * cellSize + (a.local.x - b.local.x);
    const float dz = static_cast<float>(a.cell.z - b.cell.z) * cellSize + (a.local.z - b.local.z);
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

void initializeMissionSimulation(GameState& state) {
    auto& sim = state.missionSimulation;
    sim.version = currentMissionSimulationVersion;
    sim.config = {};
    sim.statistics = {};
}

bool isValidMissionTransition(MissionState from, MissionState to) noexcept {
    if (from == to) return true; // idempotent
    if (isTerminalMissionState(from)) return false; // terminal states cannot transition

    switch (from) {
    case MissionState::Available:
        return to == MissionState::Accepted ||
               to == MissionState::Active ||
               to == MissionState::Expired ||
               to == MissionState::Invalidated ||
               to == MissionState::ResolvedByNpc ||
               to == MissionState::Failed;

    case MissionState::Accepted:
        return to == MissionState::Active ||
               to == MissionState::Expired ||
               to == MissionState::Invalidated ||
               to == MissionState::ResolvedByNpc ||
               to == MissionState::Failed;

    case MissionState::Active:
        return to == MissionState::Completed ||
               to == MissionState::Failed ||
               to == MissionState::Expired ||
               to == MissionState::Invalidated ||
               to == MissionState::ResolvedByNpc;

    default:
        return false;
    }
}

bool evaluateCondition(const GameState& state, const ObjectiveCondition& cond,
                       const ObjectiveRuntimeState* runtime) {
    switch (cond.type) {
    case ConditionType::AlwaysTrue:
        return true;

    case ConditionType::NpcAlive:
        return state.world.npcs.exists(cond.npc) &&
               state.world.npcs.get(cond.npc).lifecycle == world::NpcLifecycle::Alive;

    case ConditionType::NpcDead:
        return state.world.npcs.exists(cond.npc) &&
               state.world.npcs.get(cond.npc).lifecycle == world::NpcLifecycle::Dead;

     case ConditionType::PlayerInTerritory:
         // Semantic query: evaluate if target territory exists and is valid
         // Player presence check requires player controller (future phase)
         return false;

     case ConditionType::PlayerNearLocation:
         // Semantic location query
         // Player presence check requires player controller (future phase)
         return false;

    case ConditionType::FactionControlsTerritory:
        return cond.faction && cond.territory && controlsTerritory(state, cond.faction, cond.territory);

    case ConditionType::TerritoryContested: {
        if (!cond.territory || !state.territorySimulation.territories.contains(cond.territory)) return false;
        const auto ctrl = territoryControl(state, cond.territory);
        return ctrl.state == TerritoryControlState::Contested;
    }

    case ConditionType::RelationshipHostilityAbove: {
        for (const auto& r : state.world.factionRelationships) {
            if (r.from == cond.faction && r.hostility >= static_cast<std::uint32_t>(cond.threshold)) return true;
        }
        return false;
    }

    case ConditionType::RelationshipTrustAbove: {
        for (const auto& r : state.world.factionRelationships) {
            if (r.from == cond.faction && r.trust >= static_cast<std::uint32_t>(cond.threshold)) return true;
        }
        return false;
    }

    case ConditionType::MarketDemandExceedsSupply: {
        if (!cond.district || !state.marketSimulation.districts.contains(cond.district)) return false;
        const auto& d = state.marketSimulation.districts.at(cond.district);
        return d.demand > d.supply;
    }

    case ConditionType::StoryNodeOutcome: {
        if (!state.story.version || !cond.storyNode || !state.story.nodes.exists(cond.storyNode)) return false;
        return state.story.nodes.get(cond.storyNode).outcome == cond.storyOutcome;
    }

    case ConditionType::ObjectiveCompleted: {
        // Checked in objective context
        return runtime && runtime->state == ObjectiveState::Completed;
    }

    case ConditionType::SimulationTimePassed:
        return state.clock.gameTime().milliseconds >= cond.time.milliseconds;

    case ConditionType::LocationReached:
        return runtime && runtime->locationReached;

    case ConditionType::InteractionOccurred:
        return runtime && runtime->interactionOccurred;

    case ConditionType::ItemDelivered:
        return runtime && runtime->deliveryCompleted;

    case ConditionType::TargetProtected:
        return runtime && runtime->protectResolved;

    case ConditionType::TargetAttacked:
        return runtime && runtime->attackResolved;
    }

    return false;
}

MissionId createMission(GameState& state, const MissionDefinition& def,
                       const MissionContext& context,
                       StoryNodeId originStory,
                       const MissionIssuer& issuer) {
    validateMissionDefinition(def, state);

    auto& sim = state.missionSimulation;
    core::require(sim.instances.entries().size() < 4096, "Mission store resource limit");

    MissionInstance instance;
    instance.templateId = def.templateId;
    instance.category = def.category;
    instance.status = MissionState::Available;
    instance.issuer = issuer;
    instance.originStoryNode = originStory;
    instance.context = context;
    instance.createdAt = state.clock.gameTime();
    if (def.durationTicks > 0) {
        instance.expiresAt = core::GameTime{state.clock.gameTime().milliseconds +
                                           def.durationTicks * core::SimulationClock::gameMillisecondsPerTick};
    }
    instance.objectiveDefinitions = def.objectives;
    instance.reward = def.reward;

    // Initialize objective runtime states
    for (const auto& obj : def.objectives) {
        ObjectiveRuntimeState runtime;
        if (obj.id == def.initialObjective) {
            runtime.state = ObjectiveState::Available;
        } else {
            runtime.state = ObjectiveState::Locked;
        }
        instance.objectiveStates.emplace(obj.id, runtime);
    }

    const auto id = sim.instances.create(std::move(instance));
    sim.instances.get(id).id = id;
    sim.statistics.missionsCreated++;

    return id;
}

void acceptMission(GameState& state, MissionId id, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Accepting non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Accepted),
                  "Illegal transition to Accepted");

    instance.status = MissionState::Accepted;
    state.missionSimulation.statistics.missionsAccepted++;
    events.push({MissionEventKind::Accepted, id, {}, instance.status, state.clock.tick()});
}

void activateMission(GameState& state, MissionId id, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Activating non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Active),
                  "Illegal transition to Active");

    instance.status = MissionState::Active;
    state.missionSimulation.statistics.missionsActive++;

    // Activate available objectives
    for (const auto& objDef : instance.objectiveDefinitions) {
        auto& rState = instance.objectiveStates.at(objDef.id);
        if (rState.state == ObjectiveState::Available) {
            rState.state = ObjectiveState::Active;
            rState.startedAt = state.clock.gameTime();
        }
    }

    events.push({MissionEventKind::Activated, id, {}, instance.status, state.clock.tick()});
}

static void applyMissionReward(GameState& state, const MissionInstance& instance) {
    // Deliver rewards through existing domain systems without cheating
    if (instance.reward.cash > 0 && instance.issuer.faction &&
        state.world.factions.exists(instance.issuer.faction)) {
        // Strategic faction rewards: faction operations or balance changes
    }
}

void completeMission(GameState& state, MissionId id, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Completing non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Completed),
                  "Illegal transition to Completed");

    instance.status = MissionState::Completed;
    state.missionSimulation.statistics.missionsCompleted++;

    // Mark remaining active optional objectives as skipped
    for (const auto& objDef : instance.objectiveDefinitions) {
        auto& rState = instance.objectiveStates.at(objDef.id);
        if (rState.state == ObjectiveState::Active || rState.state == ObjectiveState::Available) {
            if (objDef.optional) {
                rState.state = ObjectiveState::Skipped;
            }
        }
    }

    applyMissionReward(state, instance);

    // Story Graph integration bridge: if originating from a story node, resolve story node
    if (instance.originStoryNode && state.story.version != 0 &&
        state.story.nodes.exists(instance.originStoryNode)) {
        if (storyAvailability(state, instance.originStoryNode) == StoryAvailability::Eligible) {
            resolveStoryNode(state, instance.originStoryNode, StoryOutcome::Succeeded);
        }
    }

    events.push({MissionEventKind::Completed, id, {}, instance.status, state.clock.tick()});
}

void failMission(GameState& state, MissionId id, const std::string& /*reason*/, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Failing non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Failed),
                  "Illegal transition to Failed");

    instance.status = MissionState::Failed;
    state.missionSimulation.statistics.missionsFailed++;

    // Story Graph integration bridge: if originating from a story node, resolve story node
    if (instance.originStoryNode && state.story.version != 0 &&
        state.story.nodes.exists(instance.originStoryNode)) {
        if (storyAvailability(state, instance.originStoryNode) == StoryAvailability::Eligible) {
            resolveStoryNode(state, instance.originStoryNode, StoryOutcome::Failed);
        }
    }

    events.push({MissionEventKind::Failed, id, {}, instance.status, state.clock.tick()});
}

void expireMission(GameState& state, MissionId id, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Expiring non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Expired),
                  "Illegal transition to Expired");

    instance.status = MissionState::Expired;
    state.missionSimulation.statistics.missionsExpired++;
    events.push({MissionEventKind::Expired, id, {}, instance.status, state.clock.tick()});
}

void invalidateMission(GameState& state, MissionId id, const std::string& /*reason*/, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Invalidating non-existent mission");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::Invalidated),
                  "Illegal transition to Invalidated");

    instance.status = MissionState::Invalidated;
    state.missionSimulation.statistics.missionsInvalidated++;
    events.push({MissionEventKind::Invalidated, id, {}, instance.status, state.clock.tick()});
}

void resolveMissionByNpc(GameState& state, MissionId id, const std::string& /*systemicCause*/, MissionEventBuffer& events) {
    core::require(state.missionSimulation.instances.exists(id), "Resolving non-existent mission by NPC");
    auto& instance = state.missionSimulation.instances.get(id);
    core::require(isValidMissionTransition(instance.status, MissionState::ResolvedByNpc),
                  "Illegal transition to ResolvedByNpc");

    instance.status = MissionState::ResolvedByNpc;
    state.missionSimulation.statistics.missionsResolvedByNpc++;
    events.push({MissionEventKind::ResolvedByNpc, id, {}, instance.status, state.clock.tick()});
}

static void processObjectiveSuccess(GameState& state, MissionInstance& instance,
                                    const ObjectiveDefinition& objDef, MissionEventBuffer& events) {
    auto& rState = instance.objectiveStates.at(objDef.id);
    rState.state = ObjectiveState::Completed;
    rState.completedAt = state.clock.gameTime();
    events.push({MissionEventKind::ObjectiveCompleted, instance.id, objDef.id, instance.status, state.clock.tick()});

    bool missionCompleteTriggered = false;
    bool missionFailTriggered = false;

    for (const auto& transition : objDef.transitions) {
        if (transition.trigger != TransitionTrigger::OnSuccess) continue;

        switch (transition.action) {
        case TransitionAction::ActivateObjective:
            if (instance.objectiveStates.contains(transition.targetObjective)) {
                auto& targetState = instance.objectiveStates.at(transition.targetObjective);
                if (targetState.state == ObjectiveState::Locked || targetState.state == ObjectiveState::Available) {
                    targetState.state = ObjectiveState::Active;
                    targetState.startedAt = state.clock.gameTime();
                }
            }
            break;

        case TransitionAction::SkipObjective:
            if (instance.objectiveStates.contains(transition.targetObjective)) {
                auto& targetState = instance.objectiveStates.at(transition.targetObjective);
                if (targetState.state != ObjectiveState::Completed && targetState.state != ObjectiveState::Failed) {
                    targetState.state = ObjectiveState::Skipped;
                }
            }
            break;

        case TransitionAction::CompleteMission:
            missionCompleteTriggered = true;
            break;

        case TransitionAction::FailMission:
            missionFailTriggered = true;
            break;
        }
    }

    if (missionCompleteTriggered) {
        completeMission(state, instance.id, events);
    } else if (missionFailTriggered) {
        failMission(state, instance.id, "Objective transition requested failure", events);
    }
}

static void processObjectiveFailure(GameState& state, MissionInstance& instance,
                                    const ObjectiveDefinition& objDef, MissionEventBuffer& events) {
    auto& rState = instance.objectiveStates.at(objDef.id);
    rState.state = ObjectiveState::Failed;
    rState.completedAt = state.clock.gameTime();
    events.push({MissionEventKind::ObjectiveFailed, instance.id, objDef.id, instance.status, state.clock.tick()});

    bool transitionHandled = false;
    for (const auto& transition : objDef.transitions) {
        if (transition.trigger != TransitionTrigger::OnFailure) continue;
        transitionHandled = true;

        switch (transition.action) {
        case TransitionAction::ActivateObjective:
            if (instance.objectiveStates.contains(transition.targetObjective)) {
                auto& targetState = instance.objectiveStates.at(transition.targetObjective);
                targetState.state = ObjectiveState::Active;
                targetState.startedAt = state.clock.gameTime();
            }
            break;

        case TransitionAction::SkipObjective:
            if (instance.objectiveStates.contains(transition.targetObjective)) {
                auto& targetState = instance.objectiveStates.at(transition.targetObjective);
                targetState.state = ObjectiveState::Skipped;
            }
            break;

        case TransitionAction::CompleteMission:
            completeMission(state, instance.id, events);
            return;

        case TransitionAction::FailMission:
            failMission(state, instance.id, "Objective failure terminal transition", events);
            return;
        }
    }

    if (!transitionHandled && !objDef.optional) {
        failMission(state, instance.id, "Mandatory objective failed without branch", events);
    }
}

void notifyLocationReached(GameState& state, MissionId id, ObjectiveId objective,
                           const WorldPosition& pos, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(id)) return;
    auto& instance = state.missionSimulation.instances.get(id);
    if (instance.status != MissionState::Active) return;

    for (const auto& objDef : instance.objectiveDefinitions) {
        if (objDef.id != objective) continue;
        auto& rState = instance.objectiveStates.at(objective);
        if (rState.state != ObjectiveState::Active) continue;

        const float dist = distance2D(pos, objDef.targetPosition, state.world.spatial.cellSize());
        if (dist <= objDef.targetRadius) {
            rState.locationReached = true;
            processObjectiveSuccess(state, instance, objDef, events);
        }
        break;
    }
}

void notifyInteractionOccurred(GameState& state, MissionId id, ObjectiveId objective,
                               NpcId npc, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(id)) return;
    auto& instance = state.missionSimulation.instances.get(id);
    if (instance.status != MissionState::Active) return;

    for (const auto& objDef : instance.objectiveDefinitions) {
        if (objDef.id != objective) continue;
        auto& rState = instance.objectiveStates.at(objective);
        if (rState.state != ObjectiveState::Active) continue;

        if (!objDef.targetNpc || objDef.targetNpc == npc) {
            rState.interactionOccurred = true;
            processObjectiveSuccess(state, instance, objDef, events);
        }
        break;
    }
}

void notifyDeliveryCompleted(GameState& state, MissionId id, ObjectiveId objective,
                             NpcId recipient, std::uint32_t payloadItemId, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(id)) return;
    auto& instance = state.missionSimulation.instances.get(id);
    if (instance.status != MissionState::Active) return;

    for (const auto& objDef : instance.objectiveDefinitions) {
        if (objDef.id != objective) continue;
        auto& rState = instance.objectiveStates.at(objective);
        if (rState.state != ObjectiveState::Active) continue;

        // Check prerequisites: recipient must be alive
        if (objDef.recipientNpc && state.world.npcs.exists(objDef.recipientNpc)) {
            if (state.world.npcs.get(objDef.recipientNpc).lifecycle != world::NpcLifecycle::Alive) {
                processObjectiveFailure(state, instance, objDef, events);
                return;
            }
        }

        if ((!objDef.recipientNpc || objDef.recipientNpc == recipient) &&
            (!objDef.payloadItemId || objDef.payloadItemId == payloadItemId)) {
            rState.deliveryCompleted = true;
            processObjectiveSuccess(state, instance, objDef, events);
        }
        break;
    }
}

void notifyProtectedTargetResolved(GameState& state, MissionId id, ObjectiveId objective,
                                   NpcId target, bool success, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(id)) return;
    auto& instance = state.missionSimulation.instances.get(id);
    if (instance.status != MissionState::Active) return;

    for (const auto& objDef : instance.objectiveDefinitions) {
        if (objDef.id != objective) continue;
        auto& rState = instance.objectiveStates.at(objective);
        if (rState.state != ObjectiveState::Active) continue;

        if (!objDef.targetNpc || objDef.targetNpc == target) {
            rState.protectResolved = success;
            if (success) {
                processObjectiveSuccess(state, instance, objDef, events);
            } else {
                processObjectiveFailure(state, instance, objDef, events);
            }
        }
        break;
    }
}

void notifyAttackTargetResolved(GameState& state, MissionId id, ObjectiveId objective,
                                NpcId target, bool success, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(id)) return;
    auto& instance = state.missionSimulation.instances.get(id);
    if (instance.status != MissionState::Active) return;

    for (const auto& objDef : instance.objectiveDefinitions) {
        if (objDef.id != objective) continue;
        auto& rState = instance.objectiveStates.at(objective);
        if (rState.state != ObjectiveState::Active) continue;

        if (!objDef.targetNpc || objDef.targetNpc == target) {
            rState.attackResolved = success;
            if (success) {
                processObjectiveSuccess(state, instance, objDef, events);
            } else {
                processObjectiveFailure(state, instance, objDef, events);
            }
        }
        break;
    }
}

void updateMissions(GameState& state, MissionEventBuffer& events) {
    const auto now = state.clock.gameTime();
    std::vector<MissionId> toExpire;

    for (const auto& [id, instance] : state.missionSimulation.instances.entries()) {
        if (isTerminalMissionState(instance.status)) continue;

        // Check expiration
        if (instance.expiresAt.has_value() && now.milliseconds >= instance.expiresAt->milliseconds) {
            toExpire.push_back(id);
        }
    }

    for (auto id : toExpire) {
        expireMission(state, id, events);
    }
}

void onNpcDied(GameState& state, NpcId npc, MissionEventBuffer& events) {
    std::vector<MissionId> toInvalidate;
    std::vector<MissionId> toFail;

    for (const auto& [id, instance] : state.missionSimulation.instances.entries()) {
        if (isTerminalMissionState(instance.status)) continue;

        // Check issuer
        if (instance.issuer.kind == IssuerKind::Npc && instance.issuer.npc == npc) {
            if (instance.status == MissionState::Available) {
                toInvalidate.push_back(id);
                continue;
            }
        }

        // Check context target
        if (instance.context.targetNpc == npc) {
            bool hasAlternateBranch = false;
            for (const auto& objDef : instance.objectiveDefinitions) {
                if (objDef.targetNpc == npc) {
                    for (const auto& t : objDef.transitions) {
                        if (t.trigger == TransitionTrigger::OnFailure) {
                            hasAlternateBranch = true;
                            break;
                        }
                    }
                }
            }
            if (!hasAlternateBranch) {
                if (instance.status == MissionState::Available) {
                    toInvalidate.push_back(id);
                } else if (instance.status == MissionState::Active || instance.status == MissionState::Accepted) {
                    toFail.push_back(id);
                }
            }
        }
    }

    for (auto id : toInvalidate) invalidateMission(state, id, "Required NPC died", events);
    for (auto id : toFail) failMission(state, id, "Target NPC died", events);
}

void onTerritoryControlChanged(GameState& state, TerritoryId territory, MissionEventBuffer& events) {
    for (const auto& [id, instance] : state.missionSimulation.instances.entries()) {
        if (instance.status != MissionState::Active) continue;
        if (instance.context.targetTerritory == territory) {
            // Observe live territory truth without mutating it
            (void)events;
        }
    }
}

void onFactionOperationResolved(GameState& state, OperationId operation, MissionEventBuffer& events) {
    if (!state.factionSimulation.operations.exists(operation)) return;
    const auto& op = state.factionSimulation.operations.get(operation);

    std::vector<MissionId> toResolve;
    for (const auto& [id, instance] : state.missionSimulation.instances.entries()) {
        if (instance.status == MissionState::Available || instance.status == MissionState::Accepted) {
            if (instance.context.targetTerritory == op.target &&
                instance.context.targetFaction == op.faction) {
                // Autonomous NPC/faction resolution of opportunity
                toResolve.push_back(id);
            }
        }
    }

    for (auto id : toResolve) {
        resolveMissionByNpc(state, id, "Faction operation resolved conflict", events);
    }
}

void validateMissionDefinition(const MissionDefinition& def, const GameState& state) {
    core::require(!def.templateId.empty(), "Empty mission template ID");
    core::require(!def.title.empty(), "Empty mission title");
    core::require(!def.objectives.empty(), "Mission definition has no objectives");

    std::set<ObjectiveId> objectiveIds;
    bool hasInitial = false;
    bool hasTerminal = false;

    for (const auto& obj : def.objectives) {
        core::require(static_cast<bool>(obj.id), "Null objective ID in definition");
        core::require(objectiveIds.insert(obj.id).second, "Duplicate objective ID in definition");

        if (obj.id == def.initialObjective) hasInitial = true;

        if (obj.targetNpc) {
            core::require(state.world.npcs.exists(obj.targetNpc), "Objective target NPC does not exist");
        }
        if (obj.targetFaction) {
            core::require(state.world.factions.exists(obj.targetFaction), "Objective target faction does not exist");
        }
        if (obj.targetTerritory) {
            core::require(state.world.territories.exists(obj.targetTerritory), "Objective target territory does not exist");
        }
        if (obj.targetDistrict) {
            core::require(state.world.districts.exists(obj.targetDistrict), "Objective target district does not exist");
        }
        if (obj.recipientNpc) {
            core::require(state.world.npcs.exists(obj.recipientNpc), "Objective recipient NPC does not exist");
        }

        for (const auto& t : obj.transitions) {
            if (t.action == TransitionAction::CompleteMission || t.action == TransitionAction::FailMission) {
                hasTerminal = true;
            }
        }
    }

    core::require(hasInitial, "Initial objective missing from objective graph");
    core::require(hasTerminal, "Mission definition has no terminal outcome");

    // Check transition targets exist
    for (const auto& obj : def.objectives) {
        for (const auto& t : obj.transitions) {
            if (t.action == TransitionAction::ActivateObjective || t.action == TransitionAction::SkipObjective) {
                core::require(objectiveIds.contains(t.targetObjective), "Transition target objective does not exist");
            }
        }
    }

    // Graph reachability check from initial objective
    std::set<ObjectiveId> reachable;
    std::queue<ObjectiveId> q;
    reachable.insert(def.initialObjective);
    q.push(def.initialObjective);

    while (!q.empty()) {
        const auto curr = q.front();
        q.pop();

        for (const auto& obj : def.objectives) {
            if (obj.id != curr) continue;
            for (const auto& t : obj.transitions) {
                if (t.action == TransitionAction::ActivateObjective || t.action == TransitionAction::SkipObjective) {
                    if (reachable.insert(t.targetObjective).second) {
                        q.push(t.targetObjective);
                    }
                }
            }
            break;
        }
    }

    for (const auto& obj : def.objectives) {
        core::require(reachable.contains(obj.id), "Unreachable objective in definition");
    }
}

void validateMissionInstance(const MissionInstance& instance, const GameState& state) {
    core::require(static_cast<bool>(instance.id), "Null mission ID in instance");
    core::require(static_cast<std::uint8_t>(instance.status) <= static_cast<std::uint8_t>(MissionState::ResolvedByNpc),
                  "Invalid mission status");
    core::require(static_cast<std::uint8_t>(instance.category) <= static_cast<std::uint8_t>(MissionCategory::Opportunity),
                  "Invalid mission category");

    if (instance.originStoryNode) {
        core::require(state.story.version != 0 && state.story.nodes.exists(instance.originStoryNode),
                      "Origin story node does not exist");
    }

    if (instance.issuer.kind == IssuerKind::Npc && instance.issuer.npc) {
        core::require(state.world.npcs.exists(instance.issuer.npc), "Issuer NPC does not exist");
    }
    if (instance.issuer.kind == IssuerKind::Faction && instance.issuer.faction) {
        core::require(state.world.factions.exists(instance.issuer.faction), "Issuer faction does not exist");
    }

    core::require(!instance.objectiveDefinitions.empty(), "Mission instance has no objective definitions");
    core::require(instance.objectiveDefinitions.size() == instance.objectiveStates.size(),
                  "Objective definition and state count mismatch");

    for (const auto& def : instance.objectiveDefinitions) {
        core::require(instance.objectiveStates.contains(def.id), "Missing objective runtime state for definition");
        const auto& r = instance.objectiveStates.at(def.id);
        core::require(static_cast<std::uint8_t>(r.state) <= static_cast<std::uint8_t>(ObjectiveState::Invalidated),
                      "Invalid objective state");
    }
}

void validateMissionSimulation(const GameState& state) {
    const auto& sim = state.missionSimulation;
    if (sim.version == 0) {
        core::require(sim.instances.entries().empty(), "Unversioned mission payload has instances");
        return;
    }

    core::require(sim.version == currentMissionSimulationVersion, "Unsupported mission simulation version");

    for (const auto& [id, instance] : sim.instances.entries()) {
        core::require(id == instance.id, "Mission ID store key mismatch");
        validateMissionInstance(instance, state);
    }
}

void appendMissionSimulationHash(core::StateHash& hash, const GameState& state) {
    const auto& sim = state.missionSimulation;
    hash.u64(sim.version);
    hash.u64(sim.config.maxActiveMissions);
    hash.u64(sim.config.maxAvailableMissions);

    hash.u64(sim.statistics.missionsCreated);
    hash.u64(sim.statistics.missionsAccepted);
    hash.u64(sim.statistics.missionsActive);
    hash.u64(sim.statistics.missionsCompleted);
    hash.u64(sim.statistics.missionsFailed);
    hash.u64(sim.statistics.missionsExpired);
    hash.u64(sim.statistics.missionsInvalidated);
    hash.u64(sim.statistics.missionsResolvedByNpc);

    hash.u64(sim.instances.nextId());
    hash.u64(sim.instances.entries().size());

    for (const auto& [id, m] : sim.instances.entries()) {
        hash.u64(id.value());
        hash.string(m.templateId);
        hash.u64(static_cast<std::uint64_t>(m.category));
        hash.u64(static_cast<std::uint64_t>(m.status));
        hash.u64(static_cast<std::uint64_t>(m.issuer.kind));
        hash.u64(m.issuer.npc.value());
        hash.u64(m.issuer.faction.value());
        hash.u64(m.issuer.hidden ? 1 : 0);
        hash.u64(m.originStoryNode.value());

        hash.u64(m.context.targetNpc.value());
        hash.u64(m.context.targetFaction.value());
        hash.u64(m.context.targetTerritory.value());
        hash.u64(m.context.targetDistrict.value());
        hash.u64(m.context.payloadItemId);
        hash.u64(m.context.recipientNpc.value());

        hash.u64(m.createdAt.milliseconds);
        hash.u64(m.expiresAt.has_value() ? 1 : 0);
        if (m.expiresAt.has_value()) hash.u64(m.expiresAt->milliseconds);

        hash.u64(static_cast<std::uint64_t>(m.reward.cash));
        hash.u64(static_cast<std::uint64_t>(m.reward.respect));
        hash.u64(m.reward.factionFavor.value());
        hash.u64(static_cast<std::uint64_t>(m.reward.favorAmount));

        hash.u64(m.objectiveDefinitions.size());
        for (const auto& def : m.objectiveDefinitions) {
            hash.u64(def.id.value());
            hash.u64(static_cast<std::uint64_t>(def.type));
            hash.string(def.description);
            hash.u64(def.targetNpc.value());
            hash.u64(def.targetFaction.value());
            hash.u64(def.targetTerritory.value());
            hash.u64(def.targetDistrict.value());
            hash.u64(def.payloadItemId);
            hash.u64(def.recipientNpc.value());
            hash.u64(def.optional ? 1 : 0);

            hash.u64(def.transitions.size());
            for (const auto& t : def.transitions) {
                hash.u64(static_cast<std::uint64_t>(t.trigger));
                hash.u64(static_cast<std::uint64_t>(t.action));
                hash.u64(t.targetObjective.value());
            }
        }

        hash.u64(m.objectiveStates.size());
        for (const auto& [objId, r] : m.objectiveStates) {
            hash.u64(objId.value());
            hash.u64(static_cast<std::uint64_t>(r.state));
            hash.u64(r.startedAt.milliseconds);
            hash.u64(r.completedAt.milliseconds);
            hash.u64(r.locationReached ? 1 : 0);
            hash.u64(r.interactionOccurred ? 1 : 0);
            hash.u64(r.deliveryCompleted ? 1 : 0);
            hash.u64(r.protectResolved ? 1 : 0);
            hash.u64(r.attackResolved ? 1 : 0);
        }
    }
}

} // namespace faultline::simulation
