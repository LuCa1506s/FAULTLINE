#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/missions/mission_system.hpp"
#include "simulation/runtime/game_state.hpp"
#include "simulation/runtime/game.hpp"
#include "core/id/id.hpp"
#include "simulation/story/story_system.hpp"
#include "simulation/history/history_system.hpp"
#include "world/generation/world_generator.hpp"
#include "world/spatial/world_position.hpp"
#include "persistence/save.hpp"
#include <optional>
#include <vector>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

// Helper to prepare a GameState with initialized story graph (similar to story_tests.cpp)
GameState preparedWithStory(std::uint64_t seed=12345) {
    world::WorldGenerationConfig c; c.width=512;c.depth=512;c.waterPercent=0;
    GameState s{WorldSeed{seed},c};
    auto p=historyPrologueCandidate(s,FactionId{1});
    p.firstDeliveryCompleted=true;
    beginStrategicHistory(s,p);
    while (s.history.months<48) advanceStrategicHistory(s);
    initializeStoryGraph(s);
    return s;
}

TEST_CASE("MissionState transitions") {
    // Test legal transitions
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Accepted));
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Active));
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Expired));
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Invalidated));
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::ResolvedByNpc));
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Failed));

    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::Active));
    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::Expired));
    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::Invalidated));
    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::ResolvedByNpc));
    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::Failed));

    CHECK(isValidMissionTransition(MissionState::Active, MissionState::Completed));
    CHECK(isValidMissionTransition(MissionState::Active, MissionState::Failed));
    CHECK(isValidMissionTransition(MissionState::Active, MissionState::Expired));
    CHECK(isValidMissionTransition(MissionState::Active, MissionState::Invalidated));
    CHECK(isValidMissionTransition(MissionState::Active, MissionState::ResolvedByNpc));

    // Test idempotent
    CHECK(isValidMissionTransition(MissionState::Available, MissionState::Available));
    CHECK(isValidMissionTransition(MissionState::Accepted, MissionState::Accepted));
    CHECK(isValidMissionTransition(MissionState::Active, MissionState::Active));

    // Test illegal transitions
    CHECK_FALSE(isValidMissionTransition(MissionState::Completed, MissionState::Available)); // terminal to non-terminal
    CHECK_FALSE(isValidMissionTransition(MissionState::Failed, MissionState::Available));
    CHECK_FALSE(isValidMissionTransition(MissionState::Expired, MissionState::Available));
    CHECK_FALSE(isValidMissionTransition(MissionState::Invalidated, MissionState::Available));
    CHECK_FALSE(isValidMissionTransition(MissionState::ResolvedByNpc, MissionState::Available));

    CHECK_FALSE(isValidMissionTransition(MissionState::Accepted, MissionState::Completed)); // skipping Active
    CHECK_FALSE(isValidMissionTransition(MissionState::Active, MissionState::Accepted)); // going backwards
}

TEST_CASE("MissionDefinition validation - empty title") {
    MissionDefinition def;
    def.templateId = "test";
    def.title = ""; // empty
    def.objectives = {
        {ObjectiveId(1), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false}
    };
    def.initialObjective = ObjectiveId(1);
    GameState state(WorldSeed(12345));
    CHECK_THROWS_AS(validateMissionDefinition(def, state), std::exception);
}

TEST_CASE("MissionDefinition validation - duplicate objective id") {
    MissionDefinition def;
    def.templateId = "test";
    def.title = "test";
    def.objectives = {
        {ObjectiveId(1), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false},
        {ObjectiveId(1), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false} // duplicate
    };
    def.initialObjective = ObjectiveId(1);
    GameState state(WorldSeed(12345));
    CHECK_THROWS_AS(validateMissionDefinition(def, state), std::exception);
}

TEST_CASE("MissionDefinition validation - missing initial objective") {
    MissionDefinition def;
    def.templateId = "test";
    def.title = "test";
    def.objectives = {
        {ObjectiveId(1), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false},
        {ObjectiveId(2), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false}
    };
    def.initialObjective = ObjectiveId(3); // not in objectives
    GameState state(WorldSeed(12345));
    CHECK_THROWS_AS(validateMissionDefinition(def, state), std::exception);
}

TEST_CASE("MissionDefinition validation - no terminal transition") {
    MissionDefinition def;
    def.templateId = "test";
    def.title = "test";
    def.objectives = {
        {ObjectiveId(1), ObjectiveType::GoTo, "", NpcId{}, FactionId{}, TerritoryId{}, DistrictId{}, WorldPosition{}, 15.0f, 0u, NpcId{}, ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{}, {}, false} // no transitions
    };
    def.initialObjective = ObjectiveId(1);
    GameState state(WorldSeed(12345));
    CHECK_THROWS_AS(validateMissionDefinition(def, state), std::exception);
}

TEST_CASE("StoryGraph bridge - mission completion resolves story node") {
    // Prepare a state with story graph initialized
    GameState s = preparedWithStory();
    
    // Find a Contact story node that is eligible
    std::optional<StoryNodeId> storyNodeId = std::nullopt;
    for (const auto& [id, node] : s.story.nodes.entries()) {
        if (node.kind == StoryKind::Contact && storyAvailability(s, id) == StoryAvailability::Eligible) {
            storyNodeId = id;
            break;
        }
    }
    // If we didn't find an eligible Contact node, try any eligible node (for robustness)
    if (!storyNodeId) {
        for (const auto& [id, node] : s.story.nodes.entries()) {
            if (storyAvailability(s, id) == StoryAvailability::Eligible) {
                storyNodeId = id;
                break;
            }
        }
    }
    REQUIRE(storyNodeId); // We must find an eligible story node for the test to be valid
    
    // Create a mission definition that originates from this story node
    MissionDefinition def;
    def.templateId = "test-story-bridge";
    def.title = "Test Story Bridge Mission";
    // Simple GoTo objective that we can easily complete
    def.objectives = {
        {
            ObjectiveId(1),
            ObjectiveType::GoTo,
            "Go to origin",
            NpcId{}, // targetNpc
            FactionId{}, // targetFaction
            TerritoryId{}, // targetTerritory
            DistrictId{}, // targetDistrict
            WorldPosition{}, // targetPosition
            1000.0f, // targetRadius
            0u, // payloadItemId
            NpcId{}, // recipientNpc
            ObjectiveCondition{}, // startCondition
            ObjectiveCondition{}, // successCondition
            ObjectiveCondition{}, // failureCondition
            std::vector<ObjectiveTransition>{{TransitionTrigger::OnSuccess, TransitionAction::CompleteMission, ObjectiveId(1)}},
            false // optional
        }
    };
    def.initialObjective = ObjectiveId(1);
    
    // Validate the definition (should pass)
    REQUIRE_NOTHROW(validateMissionDefinition(def, s));
    
    // Create mission issuer (faction issuer to avoid NPC death issues)
    MissionIssuer issuer;
    issuer.kind = IssuerKind::Faction;
    issuer.faction = FactionId{1}; // Assume faction 1 exists and is active in the prepared state
    
    // Create mission context (default)
    MissionContext ctx;
    
    // Create the mission, passing the story node ID as origin
    MissionId missionId = createMission(s, def, ctx, storyNodeId.value(), issuer);
    REQUIRE(missionId);
    
    // Accept and activate the mission
    MissionEventBuffer events;
    acceptMission(s, missionId, events);
    activateMission(s, missionId, events);
    
    // Complete the objective by simulating location reached
    // For the GoTo objective, we need to be within targetRadius of targetPosition
    // We set targetPosition to (0,0,0) and radius to 1000, so we can use (0,0,0)
    WorldPosition pos{};
    notifyLocationReached(s, missionId, ObjectiveId(1), pos, events);
    
    // The mission should now be completed
    REQUIRE(s.missionSimulation.instances.exists(missionId));
    const auto& instance = s.missionSimulation.instances.get(missionId);
    CHECK(instance.status == MissionState::Completed);
    
    // The story node should now be resolved as Succeeded
    const auto& storyNode = s.story.nodes.get(storyNodeId.value());
    CHECK(storyNode.outcome == StoryOutcome::Succeeded);
}

TEST_CASE("Save-mid-mission continuation") {
    // Prepare a state with story graph initialized
    GameState s = preparedWithStory();
    
    // Create a mission definition with two objectives: A then B
    MissionDefinition def;
    def.templateId = "test-save-mid";
    def.title = "Test Save Mid Mission";
    def.objectives = {
        {
            ObjectiveId(1),
            ObjectiveType::GoTo,
            "Go to A",
            NpcId{}, FactionId{}, TerritoryId{}, DistrictId{},
            WorldPosition{}, 1000.0f, 0u, NpcId{},
            ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{},
            std::vector<ObjectiveTransition>{{TransitionTrigger::OnSuccess, TransitionAction::ActivateObjective, ObjectiveId(2)}},
            false
        },
        {
            ObjectiveId(2),
            ObjectiveType::GoTo,
            "Go to B",
            NpcId{}, FactionId{}, TerritoryId{}, DistrictId{},
            WorldPosition{}, 1000.0f, 0u, NpcId{},
            ObjectiveCondition{}, ObjectiveCondition{}, ObjectiveCondition{},
            std::vector<ObjectiveTransition>{{TransitionTrigger::OnSuccess, TransitionAction::CompleteMission, ObjectiveId(2)}},
            false
        }
    };
    def.initialObjective = ObjectiveId(1);
    
    // Validate
    REQUIRE_NOTHROW(validateMissionDefinition(def, s));
    
    // Create mission
    MissionIssuer issuer;
    issuer.kind = IssuerKind::Faction;
    issuer.faction = FactionId{1};
    MissionContext ctx;
    MissionId missionId = createMission(s, def, ctx, StoryNodeId{}, issuer);
    REQUIRE(missionId);
    
    // Accept and activate
    MissionEventBuffer events;
    acceptMission(s, missionId, events);
    activateMission(s, missionId, events);
    
    // Complete objective A
    WorldPosition pos{};
    notifyLocationReached(s, missionId, ObjectiveId(1), pos, events);
    
    // At this point, objective A should be completed, objective B active, mission ACTIVE
    REQUIRE(s.missionSimulation.instances.exists(missionId));
    auto& instance = s.missionSimulation.instances.get(missionId);
    CHECK(instance.status == MissionState::Active);
    CHECK(instance.objectiveStates.at(ObjectiveId(1)).state == ObjectiveState::Completed);
    CHECK(instance.objectiveStates.at(ObjectiveId(2)).state == ObjectiveState::Active);
    
    // Verify mission simulation version
    CHECK(s.missionSimulation.version == currentMissionSimulationVersion);
    
    // Save the state
    auto savedBytes = saveGameState(s);
    
    // Load the state
    GameState s2 = loadGameState(savedBytes);
    
    // Debug: check mission store size
    CHECK(s2.missionSimulation.instances.entries().size() > 0);
    
    // Verify mission remains ACTIVE with objective A completed, B active
    REQUIRE(s2.missionSimulation.instances.exists(missionId));
    const auto& instance2 = s2.missionSimulation.instances.get(missionId);
    CHECK(instance2.status == MissionState::Active);
    CHECK(instance2.objectiveStates.at(ObjectiveId(1)).state == ObjectiveState::Completed);
    CHECK(instance2.objectiveStates.at(ObjectiveId(2)).state == ObjectiveState::Active);
    
    // Complete objective B
    MissionEventBuffer events2;
    notifyLocationReached(s2, missionId, ObjectiveId(2), WorldPosition{}, events2);
    
    // Mission should now be completed
    CHECK(s2.missionSimulation.instances.exists(missionId));
    const auto& instance3 = s2.missionSimulation.instances.get(missionId);
    CHECK(instance3.status == MissionState::Completed);
    CHECK(instance3.objectiveStates.at(ObjectiveId(1)).state == ObjectiveState::Completed);
    CHECK(instance3.objectiveStates.at(ObjectiveId(2)).state == ObjectiveState::Completed);
    
    // Run the same sequence uninterrupted on a fresh state
    GameState s3 = preparedWithStory();
    MissionId missionId3 = createMission(s3, def, ctx, StoryNodeId{}, issuer);
    REQUIRE(missionId3);
    MissionEventBuffer events3;
    acceptMission(s3, missionId3, events3);
    activateMission(s3, missionId3, events3);
    notifyLocationReached(s3, missionId3, ObjectiveId(1), WorldPosition{}, events3);
    notifyLocationReached(s3, missionId3, ObjectiveId(2), WorldPosition{}, events3);
    
    // Compare final states
    REQUIRE(s3.missionSimulation.instances.exists(missionId3));
    const auto& instanceUninterrupted = s3.missionSimulation.instances.get(missionId3);
    CHECK(instanceUninterrupted.status == MissionState::Completed);
    CHECK(instanceUninterrupted.objectiveStates.at(ObjectiveId(1)).state == ObjectiveState::Completed);
    CHECK(instanceUninterrupted.objectiveStates.at(ObjectiveId(2)).state == ObjectiveState::Completed);
    
    // Compare saved-then-completed bytes against uninterrupted final bytes
    auto bytesUninterrupted = saveGameState(s3);
    auto finalBytesFromSave = saveGameState(s2);
    CHECK(finalBytesFromSave == bytesUninterrupted);
}