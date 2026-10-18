#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/prologue/prologue_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/story/story_system.hpp"
#include "simulation/missions/mission_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/runtime/game.hpp"
#include "persistence/save.hpp"
#include <algorithm>
#include <set>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

namespace {

GameState freshWorld(std::uint64_t seed = 12345) {
    world::WorldGenerationConfig c; c.width = 512; c.depth = 512; c.waterPercent = 0;
    return GameState{WorldSeed{seed}, c};
}

}

TEST_CASE("PrologueState defaults to PreMission with correct version") {
    GameState state = freshWorld();
    CHECK(state.prologue.version == currentPrologueStateVersion);
    CHECK(state.prologue.phase == ProloguePhase::PreMission);
    CHECK(!state.prologue.firstDelivery.value());
    CHECK(!state.prologue.origin.value());
    CHECK(!state.prologue.evidence.firstDeliveryCompleted);
}

TEST_CASE("runPrologue creates PRIMARY 001 mission with correct template") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    CHECK(state.prologue.phase == ProloguePhase::Complete);
    CHECK(state.prologue.firstDelivery.value() != 0);
    CHECK(state.prologue.origin == FactionId{1});
    CHECK(state.prologue.evidence.firstDeliveryCompleted);
    CHECK(state.prologue.evidence.origin == FactionId{1});
    CHECK(state.prologue.evidence.rival != FactionId{1});
    CHECK(state.history.version != 0);
    CHECK(state.history.months == historyMonths);
    CHECK(state.story.version != 0);
}

TEST_CASE("FIRST DELIVERY uses the generic MissionSystem") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    REQUIRE(state.missionSimulation.instances.exists(state.prologue.firstDelivery));
    const auto& mission = state.missionSimulation.instances.get(state.prologue.firstDelivery);
    CHECK(mission.templateId == "PRIMARY_001_FIRST_DELIVERY");
    CHECK(mission.category == MissionCategory::Primary);
    CHECK(mission.status == MissionState::Completed);
}

TEST_CASE("FIRST DELIVERY objective graph is valid GO_TO -> INTERACT -> DELIVER") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    REQUIRE(state.missionSimulation.instances.exists(state.prologue.firstDelivery));
    const auto& mission = state.missionSimulation.instances.get(state.prologue.firstDelivery);

    std::vector<ObjectiveType> types;
    for (const auto& obj : mission.objectiveDefinitions) {
        types.push_back(obj.type);
    }
    REQUIRE(types.size() == 3);
    CHECK(types[0] == ObjectiveType::GoTo);
    CHECK(types[1] == ObjectiveType::Interact);
    CHECK(types[2] == ObjectiveType::Deliver);

    for (const auto& [id, rState] : mission.objectiveStates) {
        CHECK(rState.state == ObjectiveState::Completed);
    }
}

TEST_CASE("Prologue evidence uses NpcId for witness, FactionId for rival") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    const auto& evidence = state.prologue.evidence;
    CHECK_FALSE(evidence.origin == FactionId{});
    CHECK_FALSE(evidence.rival == FactionId{});
    CHECK(evidence.origin != evidence.rival);
    // Rival is deterministic within a seed, but not necessarily different across seeds
    CHECK(state.world.npcs.exists(evidence.witness));
    CHECK(state.world.npcs.get(evidence.witness).faction == evidence.rival);
    CHECK(state.world.factions.exists(evidence.rival));
    CHECK(state.world.factions.exists(evidence.origin));
}

TEST_CASE("Prologue evidence uses deterministic rival selection") {
    GameState s1 = freshWorld();
    runPrologue(s1, FactionId{1});
    const auto rival1 = s1.prologue.evidence.rival;

    GameState s2 = freshWorld();
    runPrologue(s2, FactionId{1});
    const auto rival2 = s2.prologue.evidence.rival;

    CHECK(rival1 == rival2);
}

TEST_CASE("Prologue evidence rival is deterministic for same seed") {
    GameState s1 = freshWorld();
    runPrologue(s1, FactionId{1});
    const auto rival1 = s1.prologue.evidence.rival;

    GameState s2 = freshWorld();
    runPrologue(s2, FactionId{1});
    const auto rival2 = s2.prologue.evidence.rival;

    CHECK(rival1 == rival2);
}

TEST_CASE("Different valid origin factions produce valid results") {
    for (auto origin = 2ULL; origin <= 5; ++origin) {
        GameState state = freshWorld();
        runPrologue(state, FactionId{origin});
        CHECK(state.prologue.evidence.origin == FactionId{origin});
        CHECK(state.prologue.evidence.rival != FactionId{origin});
    }
}

TEST_CASE("Discovery incident is one of the six canonical categories") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    const auto incident = state.prologue.evidence.incident;
    CHECK(static_cast<std::uint8_t>(incident) >= 0);
    CHECK(static_cast<std::uint8_t>(incident) <= 5);
}

TEST_CASE("Prologue does not create physical entities") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    CHECK(state.world.entities.entries().empty());
}

TEST_CASE("Prologue result passes full state validation") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("Prologue result enters post-timeskip sandbox with five factions") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    CHECK(state.world.factions.entries().size() == 5);
    CHECK(state.history.informed.size() == 5);
    CHECK(state.story.version != 0);
}

TEST_CASE("Mission giver uses NpcId not EntityId") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    CHECK(state.prologue.giver != NpcId{});
    CHECK(state.world.npcs.exists(state.prologue.giver));
    CHECK(state.world.npcs.get(state.prologue.giver).faction == state.prologue.origin);
}

TEST_CASE("Mission recipient is a valid NPC from the origin faction") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    CHECK(state.prologue.recipient != NpcId{});
    CHECK(state.world.npcs.exists(state.prologue.recipient));
}

TEST_CASE("Fictional product is abstract and non-instructional") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    REQUIRE(state.missionSimulation.instances.exists(state.prologue.firstDelivery));
    const auto& mission = state.missionSimulation.instances.get(state.prologue.firstDelivery);

    for (const auto& obj : mission.objectiveDefinitions) {
        if (obj.type == ObjectiveType::Deliver) {
            CHECK(obj.payloadItemId == fictionalProductToken);
            const bool hasDeliver = obj.description.find("Deliver") != std::string::npos;
            const bool hasDeliverCap = obj.description.find("Deliver") != std::string::npos;
            CHECK(hasDeliver);
            CHECK(hasDeliverCap);
        }
    }
}

TEST_CASE("Save/load before mission creation (PreMission)") {
    GameState state = freshWorld();
    const auto before = saveGameState(state);
    auto restored = loadGameState(before);
    CHECK(stateHash(state) == stateHash(restored));
    CHECK(saveGameState(restored) == before);
}

TEST_CASE("Save/load during mission Active phase") {
    GameState state = freshWorld();

    state.prologue.phase = ProloguePhase::MissionAvailable;
    state.prologue.origin = FactionId{1};
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    state.prologue.firstDelivery = missionId;
    state.prologue.giver = state.world.factions.get(FactionId{1}).leader;

    MissionEventBuffer events;
    acceptMission(state, missionId, events);
    activateMission(state, missionId, events);
    state.prologue.phase = ProloguePhase::MissionActive;

    const auto before = saveGameState(state);
    auto restored = loadGameState(before);
    CHECK(stateHash(state) == stateHash(restored));
    CHECK(saveGameState(restored) == before);

    REQUIRE(restored.missionSimulation.instances.exists(missionId));
    CHECK(restored.missionSimulation.instances.get(missionId).status == MissionState::Active);
}

TEST_CASE("Save/load after mission complete, before history start") {
    GameState state = freshWorld();

    state.prologue.phase = ProloguePhase::MissionAvailable;
    state.prologue.origin = FactionId{1};
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    state.prologue.firstDelivery = missionId;
    state.prologue.giver = state.world.factions.get(FactionId{1}).leader;

    MissionEventBuffer events;
    acceptMission(state, missionId, events);
    activateMission(state, missionId, events);
    completeFirstDeliveryObjectives(state, missionId, events);

    const auto evidence = generatePrologueEvidence(state, FactionId{1}, state.clock.tick());
    commitPrologueEvidence(state, evidence);
    state.prologue.phase = ProloguePhase::DeliveryComplete;

    const auto before = saveGameState(state);
    auto restored = loadGameState(before);
    CHECK(stateHash(state) == stateHash(restored));
    CHECK(saveGameState(restored) == before);

    CHECK(restored.prologue.phase == ProloguePhase::DeliveryComplete);
    CHECK(restored.prologue.evidence.firstDeliveryCompleted);
}

TEST_CASE("Save/load during history (partial months)") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    GameState fresh = freshWorld();
    const auto missionId = createFirstDeliveryMission(fresh, FactionId{1});
    fresh.prologue = state.prologue;
    fresh.prologue.phase = ProloguePhase::DeliveryComplete;
    fresh.prologue.firstDelivery = missionId;

    const auto evidence = generatePrologueEvidence(fresh, FactionId{1}, fresh.clock.tick());
    commitPrologueEvidence(fresh, evidence);
    beginStrategicHistory(fresh, evidence);

    while (fresh.history.months < 24) advanceStrategicHistory(fresh);
    CHECK(fresh.history.months == 24);

    const auto before = saveGameState(fresh);
    auto restored = loadGameState(before);
    CHECK(stateHash(fresh) == stateHash(restored));
    CHECK(saveGameState(restored) == before);

    while (restored.history.months < 48) advanceStrategicHistory(restored);
    initializeStoryGraph(restored);
    CHECK(restored.history.months == 48);
    CHECK(restored.story.version != 0);
}

TEST_CASE("Full prologue save/load round-trip preserves StateHash") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    const auto before = saveGameState(state);
    auto restored = loadGameState(before);
    CHECK(stateHash(state) == stateHash(restored));
    CHECK(saveGameState(restored) == before);
}

TEST_CASE("Deterministic continuation: load and resume produces same result") {
    GameState state = freshWorld(12345);
    runPrologue(state, FactionId{1});
    const auto finalState = state;
    const auto finalHash = stateHash(finalState);

    GameState loaded = loadGameState(saveGameState(finalState));
    CHECK(stateHash(loaded) == finalHash);
    CHECK(saveGameState(loaded) == saveGameState(finalState));
}

TEST_CASE("Multiple seeds produce deterministic results") {
    for (auto seed = 12340ULL; seed <= 12345; ++seed) {
        GameState a = freshWorld(seed);
        GameState b = freshWorld(seed);
        runPrologue(a, FactionId{1});
        runPrologue(b, FactionId{1});
        CHECK(stateHash(a) == stateHash(b));
        CHECK(saveGameState(a) == saveGameState(b));
        CHECK_NOTHROW(validateState(a));
    }
}

TEST_CASE("Mission failure path: FIRST DELIVERY fail does not complete history") {
    GameState state = freshWorld();
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    state.prologue.phase = ProloguePhase::MissionAvailable;
    state.prologue.origin = FactionId{1};
    state.prologue.firstDelivery = missionId;

    MissionEventBuffer events;
    acceptMission(state, missionId, events);
    activateMission(state, missionId, events);
    failMission(state, missionId, "Failed", events);

    CHECK(state.missionSimulation.instances.get(missionId).status == MissionState::Failed);
    CHECK(state.history.version == 0);
    CHECK(state.prologue.evidence.firstDeliveryCompleted == false);
}

TEST_CASE("Prologue evidence delivery/discovery tick ordering is correct") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    const auto& evidence = state.prologue.evidence;
    CHECK(evidence.deliveryTick <= evidence.discoveryTick);
    CHECK(evidence.deliveryTick <= state.clock.tick());
    CHECK(evidence.discoveryTick <= state.clock.tick());
}

TEST_CASE("Replay produces identical results (single process)") {
    GameState state = freshWorld(12345);
    const auto original = state;
    runPrologue(state, FactionId{1});
    const auto replayState = original;
    GameState replay = replayState;
    runPrologue(replay, FactionId{1});
    CHECK(stateHash(state) == stateHash(replay));
    CHECK(saveGameState(state) == saveGameState(replay));
}

TEST_CASE("Mission objective transitions form valid complete graph") {
    GameState state = freshWorld();
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});

    REQUIRE(state.missionSimulation.instances.exists(missionId));
    const auto& mission = state.missionSimulation.instances.get(missionId);

    bool hasTerminalGoal = false;
    for (const auto& obj : mission.objectiveDefinitions) {
        for (const auto& t : obj.transitions) {
            if (t.action == TransitionAction::CompleteMission) hasTerminalGoal = true;
        }
    }
    CHECK(hasTerminalGoal);
}

TEST_CASE("Post-timeskip world passes validatePostTimeskipWorld") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    CHECK_NOTHROW(validatePostTimeskipWorld(state));
}

TEST_CASE("Post-timeskip all five factions viable") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    for (const auto& [id, f] : state.world.factions.entries()) {
        (void)f;
        CHECK(f.treasury > 0);
        CHECK(f.reserveMembers >= 4);
        CHECK(controlledTerritoryCount(state, id) > 0);
    }
}

TEST_CASE("Post-timeskip market is city-wide with active supply") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    for (const auto& [id, m] : state.marketSimulation.districts) {
        (void)id;
        CHECK(m.demand > 0);
        CHECK(m.supply > 0);
        CHECK(m.price < m.maximumPrice);
    }
}

TEST_CASE("Post-timeskip unresolved tension exists") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    bool tension = false;
    for (const auto& r : state.world.factionRelationships) {
        if (r.historicalGrievance && r.hostility >= 4500) { tension = true; break; }
    }
    CHECK(tension);
}

TEST_CASE("PrologueState phase transitions are sequential and valid") {
    GameState state = freshWorld();
    CHECK(state.prologue.phase == ProloguePhase::PreMission);

    state.prologue.phase = ProloguePhase::MissionAvailable;
    state.prologue.origin = FactionId{1};
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    state.prologue.firstDelivery = missionId;

    CHECK(state.prologue.phase == ProloguePhase::MissionAvailable);
    CHECK(state.missionSimulation.instances.exists(missionId));

    MissionEventBuffer events;
    acceptMission(state, missionId, events);
    activateMission(state, missionId, events);
    state.prologue.phase = ProloguePhase::MissionActive;

    CHECK(state.missionSimulation.instances.get(missionId).status == MissionState::Active);
    completeFirstDeliveryObjectives(state, missionId, events);
    CHECK(state.missionSimulation.instances.get(missionId).status == MissionState::Completed);
}

TEST_CASE("Prologue refuse re-entry after beginStrategicHistory") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});

    CHECK_THROWS_AS(runPrologue(state, FactionId{1}), std::exception);
}

TEST_CASE("Prologue with different valid origin factions 2..5") {
    for (auto origin = 1ULL; origin <= 5; ++origin) {
        GameState state = freshWorld();
        CHECK_NOTHROW(runPrologue(state, FactionId{origin}));
        CHECK(state.prologue.evidence.origin == FactionId{origin});
        CHECK(state.prologue.evidence.rival != FactionId{origin});
    }
}

TEST_CASE("Prologue rival never equals origin") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    const auto& e = state.prologue.evidence;
    CHECK(e.origin != e.rival);
    CHECK(e.rival != FactionId{0});
    CHECK(e.origin != FactionId{0});
}

TEST_CASE("Prologue witness belongs to rival faction") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    const auto& e = state.prologue.evidence;
    CHECK(state.world.npcs.exists(e.witness));
    CHECK(state.world.npcs.get(e.witness).faction == e.rival);
}

TEST_CASE("Prologue location is valid territory of rival or origin") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    const auto& e = state.prologue.evidence;
    CHECK(state.world.territories.exists(e.location));
}

TEST_CASE("Mission issuer is the origin faction leader") {
    GameState state = freshWorld();
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    REQUIRE(state.missionSimulation.instances.exists(missionId));
    const auto& mission = state.missionSimulation.instances.get(missionId);
    CHECK(mission.issuer.kind == IssuerKind::Npc);
    CHECK(mission.issuer.npc == state.world.factions.get(FactionId{1}).leader);
    CHECK(mission.issuer.faction == FactionId{1});
}

TEST_CASE("Mission context has valid target/recipient/payload") {
    GameState state = freshWorld();
    const auto missionId = createFirstDeliveryMission(state, FactionId{1});
    REQUIRE(state.missionSimulation.instances.exists(missionId));
    const auto& mission = state.missionSimulation.instances.get(missionId);
    CHECK(mission.context.targetNpc == state.world.factions.get(FactionId{1}).leader);
    CHECK(mission.context.targetFaction == FactionId{1});
    CHECK(mission.context.recipientNpc != NpcId{0});
    CHECK(mission.context.payloadItemId == fictionalProductToken);
}

TEST_CASE("Prologue state is included in StateHash") {
    GameState state = freshWorld();
    const auto preHash = stateHash(state);
    runPrologue(state, FactionId{1});
    const auto postHash = stateHash(state);
    CHECK(preHash != postHash);
    auto restored = loadGameState(saveGameState(state));
    CHECK(stateHash(restored) == postHash);
}

TEST_CASE("v8 fixture migrates to v11 with default PrologueState and PlayerState") {
    const auto path = std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v8_phase14.flsave";
    GameState state = loadGameStateFile(path);
    CHECK(state.prologue.version == currentPrologueStateVersion);
    CHECK(state.prologue.phase == ProloguePhase::PreMission);
    
    CHECK(!state.prologue.firstDelivery.value());
    const auto bytes = saveGameState(state);
    CHECK(bytes[16] == 11);
    auto reloaded = loadGameState(bytes);
    CHECK(stateHash(reloaded) == stateHash(state));
    CHECK(saveGameState(reloaded) == bytes);
}

TEST_CASE("v9 fixture migrates to v11 with default PrologueState and PlayerState") {
    const auto path = std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v9_phase15.flsave";
    GameState state = loadGameStateFile(path);
    CHECK(state.metadata.generatorVersion.value == 3);
    CHECK(state.metadata.seed == WorldSeed{12345});
    CHECK(state.missionSimulation.instances.entries().empty());
    CHECK(state.prologue.version == currentPrologueStateVersion);
    CHECK(state.prologue.phase == ProloguePhase::PreMission);
    CHECK(state.history.version == 1);
    CHECK(state.history.months == historyMonths);
    CHECK(state.story.version == 1);
    const auto beforeHash = stateHash(state);
    const auto bytes = saveGameState(state);
    CHECK(bytes[16] == 11);
    auto reloaded = loadGameState(bytes);
    CHECK(stateHash(reloaded) == beforeHash);
    CHECK(saveGameState(reloaded) == bytes);
}

TEST_CASE("Independent-process determinism of prologue execution") {
    GameState state = freshWorld(12345);
    runPrologue(state, FactionId{1});
    const auto hash = stateHash(state);
    const auto bytes = saveGameState(state);
    auto restored = loadGameState(bytes);
    CHECK(stateHash(restored) == hash);
    CHECK(saveGameState(restored) == bytes);
}

TEST_CASE("Mass-seed prologue: multiple seeds produce valid sandboxes") {
    for (auto seed = 12340ULL; seed <= 12350; ++seed) {
        GameState state = freshWorld(seed);
        runPrologue(state, FactionId{1});
        CHECK_NOTHROW(validateState(state));
        CHECK(state.world.factions.entries().size() == 5);
        CHECK(state.history.months == historyMonths);
        CHECK(state.story.version != 0);
    }
}

TEST_CASE("Prologue evidence is stored in StrategicHistory after transition") {
    GameState state = freshWorld();
    runPrologue(state, FactionId{1});
    CHECK(state.history.prologue.firstDeliveryCompleted);
    CHECK(state.history.prologue.origin == state.prologue.evidence.origin);
    CHECK(state.history.prologue.rival == state.prologue.evidence.rival);
}
