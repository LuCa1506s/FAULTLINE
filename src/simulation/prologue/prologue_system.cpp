#include "simulation/prologue/prologue_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/story/story_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include <algorithm>

namespace faultline::simulation {

void initializePrologueState(GameState& state) {
    state.prologue = PrologueState{};
}

MissionId createFirstDeliveryMission(GameState& state, FactionId origin) {
    core::require(state.world.factions.exists(origin), "Origin faction does not exist");
    auto& faction = state.world.factions.get(origin);
    core::require(faction.leader && state.world.npcs.exists(faction.leader), "Origin faction leader does not exist");

    const auto& leader = state.world.npcs.get(faction.leader);
    core::require(leader.faction == origin, "Mission giver faction mismatch");

    NpcId recipient = NpcId{};
    for (const auto& [id, npc] : state.world.npcs.entries()) {
        if (npc.faction == origin && npc.major && id != faction.leader && npc.lifecycle == world::NpcLifecycle::Alive) {
            recipient = id;
            break;
        }
    }
    core::require(static_cast<bool>(recipient), "No recipient NPC found for FIRST DELIVERY");

    const auto& recipientNpc = state.world.npcs.get(recipient);
    const auto& leaderNpc = state.world.npcs.get(faction.leader);

    ObjectiveId objGoTo = ObjectiveId{1};
    ObjectiveId objInteract = ObjectiveId{2};
    ObjectiveId objDeliver = ObjectiveId{3};

    MissionDefinition def;
    def.templateId = "PRIMARY_001_FIRST_DELIVERY";
    def.category = MissionCategory::Primary;
    def.title = "First Delivery";
    def.durationTicks = 0;
    def.reward = MissionReward{};

    ObjectiveCondition locReached{}; locReached.type=ConditionType::LocationReached;
    ObjectiveCondition interaction{}; interaction.type=ConditionType::InteractionOccurred;
    ObjectiveCondition delivered{}; delivered.type=ConditionType::ItemDelivered;

    def.objectives = {
        ObjectiveDefinition{
            objGoTo, ObjectiveType::GoTo, "Travel to the pickup location",
            faction.leader, origin, TerritoryId{}, DistrictId{},
            leaderNpc.position, 0.0f, 0u, NpcId{},
            ObjectiveCondition{},
            locReached,
            ObjectiveCondition{},
            std::vector<ObjectiveTransition>{{ObjectiveTransition{TransitionTrigger::OnSuccess, TransitionAction::ActivateObjective, objInteract}}},
            false
        },
        ObjectiveDefinition{
            objInteract, ObjectiveType::Interact, "Meet with " + leaderNpc.identity.name,
            faction.leader, origin, TerritoryId{}, DistrictId{},
            leaderNpc.position, 0.0f, 0u, NpcId{},
            ObjectiveCondition{},
            interaction,
            ObjectiveCondition{},
            std::vector<ObjectiveTransition>{{ObjectiveTransition{TransitionTrigger::OnSuccess, TransitionAction::ActivateObjective, objDeliver}}},
            false
        },
        ObjectiveDefinition{
            objDeliver, ObjectiveType::Deliver, "Deliver the package to " + recipientNpc.identity.name,
            recipient, recipientNpc.faction, TerritoryId{}, DistrictId{},
            recipientNpc.position, 0.0f, fictionalProductToken, recipient,
            ObjectiveCondition{},
            delivered,
            ObjectiveCondition{},
            std::vector<ObjectiveTransition>{{ObjectiveTransition{TransitionTrigger::OnSuccess, TransitionAction::CompleteMission}}},
            false
        }
    };
    def.initialObjective = objGoTo;

    validateMissionDefinition(def, state);

    MissionIssuer issuer;
    issuer.kind = IssuerKind::Npc;
    issuer.npc = faction.leader;
    issuer.faction = origin;

    MissionContext ctx;
    ctx.targetFaction = origin;
    ctx.targetNpc = faction.leader;
    ctx.recipientNpc = recipient;
    ctx.payloadItemId = fictionalProductToken;

    return createMission(state, def, ctx, StoryNodeId{}, issuer);
}

bool completeFirstDeliveryObjectives(GameState& state, MissionId missionId, MissionEventBuffer& events) {
    if (!state.missionSimulation.instances.exists(missionId)) return false;
    auto& instance = state.missionSimulation.instances.get(missionId);
    if (instance.status != MissionState::Active) return false;

    ObjectiveId objGoTo = ObjectiveId{1};
    ObjectiveId objInteract = ObjectiveId{2};
    ObjectiveId objDeliver = ObjectiveId{3};

    notifyLocationReached(state, missionId, objGoTo, instance.context.targetNpc
        ? state.world.npcs.get(instance.context.targetNpc).position : WorldPosition{}, events);
    if (instance.status == MissionState::Completed || instance.status == MissionState::Failed) return true;

    notifyInteractionOccurred(state, missionId, objInteract, instance.context.targetNpc, events);
    if (instance.status == MissionState::Completed || instance.status == MissionState::Failed) return true;

    if (instance.objectiveStates.at(objDeliver).state == ObjectiveState::Active) {
        notifyDeliveryCompleted(state, missionId, objDeliver, instance.context.recipientNpc,
                                instance.context.payloadItemId, events);
    }

    return instance.status == MissionState::Completed;
}

PrologueEvidence generatePrologueEvidence(GameState& state, FactionId origin, std::uint64_t deliveryTick) {
    core::require(state.world.factions.exists(origin), "Origin faction does not exist");
    const auto evidence = historyPrologueCandidate(state, origin);
    PrologueEvidence result = evidence;
    result.firstDeliveryCompleted = true;
    result.deliveryTick = deliveryTick;
    result.discoveryTick = deliveryTick;
    return result;
}

void commitPrologueEvidence(GameState& state, const PrologueEvidence& evidence) {
    state.prologue.phase = ProloguePhase::DeliveryComplete;
    state.prologue.evidence = evidence;
}

void runPrologue(GameState& state, FactionId origin) {
    core::require(!state.history.version, "Prologue requires no existing strategic history");
    core::require(state.world.entities.entries().empty(), "Prologue requires no physical entities");
    core::require(state.factionSimulation.operations.entries().empty(), "Prologue requires no active operations");
    core::require(state.prologue.phase == ProloguePhase::PreMission, "Prologue already started");

    state.prologue.origin = origin;
    state.prologue.phase = ProloguePhase::MissionAvailable;

    const auto missionId = createFirstDeliveryMission(state, origin);
    state.prologue.firstDelivery = missionId;
    state.prologue.giver = state.world.factions.get(origin).leader;
    state.prologue.recipient = state.missionSimulation.instances.get(missionId).context.recipientNpc;

    {
        MissionEventBuffer events;
        acceptMission(state, missionId, events);
        activateMission(state, missionId, events);
        state.prologue.phase = ProloguePhase::MissionActive;
    }

    {
        MissionEventBuffer events;
        completeFirstDeliveryObjectives(state, missionId, events);
    }

    core::require(state.missionSimulation.instances.exists(missionId), "FIRST DELIVERY mission missing");
    auto& instance = state.missionSimulation.instances.get(missionId);
    core::require(instance.status == MissionState::Completed, "FIRST DELIVERY did not complete");

    state.prologue.phase = ProloguePhase::DeliveryComplete;
    const auto deliveryTick = state.clock.tick();

    const auto evidence = generatePrologueEvidence(state, origin, deliveryTick);
    state.prologue.evidence = evidence;

    beginStrategicHistory(state, evidence);

    while (state.history.months < historyMonths) {
        advanceStrategicHistory(state);
    }

    initializeStoryGraph(state);
    state.prologue.phase = ProloguePhase::Complete;
    validateState(state);
}

void validatePrologueState(const GameState& state) {
    if (!state.prologue.version) {
        core::require(state.prologue == PrologueState{}, "Unversioned prologue state must be default");
        return;
    }
    core::require(state.prologue.version == currentPrologueStateVersion, "Unsupported prologue version");
    core::require(static_cast<std::uint8_t>(state.prologue.phase) <= static_cast<std::uint8_t>(ProloguePhase::Complete),
                  "Invalid prologue phase");
    if (state.prologue.phase != ProloguePhase::PreMission) {
        core::require(static_cast<bool>(state.prologue.origin), "Prologue phase>PreMission requires origin faction");
        core::require(state.world.factions.exists(state.prologue.origin), "Prologue origin faction missing");
    }
    if (state.prologue.phase >= ProloguePhase::MissionAvailable) {
        core::require(static_cast<bool>(state.prologue.firstDelivery), "Mission not created in prologue");
        core::require(state.missionSimulation.instances.exists(state.prologue.firstDelivery),
                      "Prologue mission instance missing");
    }
    if (state.prologue.phase >= ProloguePhase::MissionActive) {
        const auto& instance = state.missionSimulation.instances.get(state.prologue.firstDelivery);
        core::require(instance.templateId == "PRIMARY_001_FIRST_DELIVERY", "Prologue mission template mismatch");
    }
    if (state.prologue.phase >= ProloguePhase::DeliveryComplete && state.history.version != 0) {
        core::require(state.prologue.evidence.firstDeliveryCompleted, "Prologue evidence not committed");
        core::require(state.prologue.evidence.origin == state.history.prologue.origin, "History prologue evidence mismatch");
        core::require(state.prologue.evidence.rival == state.history.prologue.rival, "History prologue rival mismatch");
    }
    if (state.prologue.phase == ProloguePhase::Complete) {
        core::require(state.history.version != 0, "Prologue complete but history not started");
        core::require(state.history.months == historyMonths, "Prologue complete but history incomplete");
        core::require(state.story.version != 0, "Prologue complete but story graph not initialized");
    }
}

void appendPrologueStateHash(core::StateHash& hash, const GameState& state) {
    const auto& p = state.prologue;
    hash.u64(p.version);
    hash.u64(static_cast<std::uint64_t>(p.phase));
    hash.u64(p.firstDelivery.value());
    hash.u64(p.origin.value());
    hash.u64(p.giver.value());
    hash.u64(p.recipient.value());
    const auto& e = p.evidence;
    hash.u64(e.origin.value());
    hash.u64(e.rival.value());
    hash.u64(e.witness.value());
    hash.u64(e.location.value());
    hash.u64(static_cast<std::uint64_t>(e.incident));
    hash.u64(e.deliveryTick);
    hash.u64(e.discoveryTick);
    hash.u64(e.firstDeliveryCompleted ? 1 : 0);
}

void repairPrologueState(GameState& state) {
    if (state.prologue.phase == ProloguePhase::PreMission) {
        state.prologue = PrologueState{};
    }
}

}
