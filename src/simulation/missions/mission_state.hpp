#pragma once
#include "world/state/stores.hpp"
#include "world/spatial/world_position.hpp"
#include "core/time/simulation_clock.hpp"
#include "core/events/ordered_buffer.hpp"
#include "simulation/story/story_state.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

using faultline::world::WorldPosition;

namespace faultline::simulation {

inline constexpr std::uint32_t currentMissionSimulationVersion = 1;

// Canonical Mission States
enum class MissionState : std::uint8_t {
    Available = 0,
    Accepted = 1,
    Active = 2,
    Completed = 3,
    Failed = 4,
    Expired = 5,
    Invalidated = 6,
    ResolvedByNpc = 7,

    AVAILABLE = Available,
    ACCEPTED = Accepted,
    ACTIVE = Active,
    COMPLETED = Completed,
    FAILED = Failed,
    EXPIRED = Expired,
    INVALIDATED = Invalidated,
    RESOLVED_BY_NPC = ResolvedByNpc
};

[[nodiscard]] inline constexpr bool isTerminalMissionState(MissionState state) noexcept {
    return state == MissionState::Completed ||
           state == MissionState::Failed ||
           state == MissionState::Expired ||
           state == MissionState::Invalidated ||
           state == MissionState::ResolvedByNpc;
}

// Canonical Objective States
enum class ObjectiveState : std::uint8_t {
    Locked = 0,
    Available = 1,
    Active = 2,
    Completed = 3,
    Failed = 4,
    Skipped = 5,
    Invalidated = 6,

    LOCKED = Locked,
    AVAILABLE = Available,
    ACTIVE = Active,
    COMPLETED = Completed,
    FAILED = Failed,
    SKIPPED = Skipped,
    INVALIDATED = Invalidated
};

// Canonical Objective Types
enum class ObjectiveType : std::uint8_t {
    GoTo = 0,
    Interact = 1,
    Deliver = 2,
    Protect = 3,
    Attack = 4,
    Eliminate = 5,
    Escort = 6,
    Escape = 7,
    Survive = 8,
    Follow = 9,
    Wait = 10,
    Return = 11,
    Talk = 12,
    Negotiate = 13,
    Threaten = 14,
    Pay = 15,
    Steal = 16,

    GO_TO = GoTo,
    INTERACT = Interact,
    DELIVER = Deliver,
    PROTECT = Protect,
    ATTACK = Attack
};

// Canonical Mission Categories
enum class MissionCategory : std::uint8_t {
    Primary = 0,
    Side = 1,
    Dynamic = 2,
    Opportunity = 3,

    PRIMARY = Primary,
    SIDE = Side,
    DYNAMIC = Dynamic,
    OPPORTUNITY = Opportunity
};

// Issuer representation
enum class IssuerKind : std::uint8_t {
    Npc = 0,
    Faction = 1,
    Systemic = 2,
    Unknown = 3
};

struct MissionIssuer {
    IssuerKind kind{IssuerKind::Systemic};
    NpcId npc;
    FactionId faction;
    bool hidden{false};
    auto operator<=>(const MissionIssuer&) const = default;
};

// Abstract Rewards
struct MissionReward {
    std::int64_t cash{0};
    std::int32_t respect{0};
    FactionId factionFavor;
    std::int32_t favorAmount{0};
    auto operator<=>(const MissionReward&) const = default;
};

// Bound Context
struct MissionContext {
    NpcId targetNpc;
    FactionId targetFaction;
    TerritoryId targetTerritory;
    DistrictId targetDistrict;
    WorldPosition targetPosition;
    std::uint32_t payloadItemId{0};
    NpcId recipientNpc;
    auto operator<=>(const MissionContext&) const = default;
};

// Typed Objective Condition
enum class ConditionType : std::uint8_t {
    AlwaysTrue = 0,
    NpcAlive,
    NpcDead,
    PlayerInTerritory,
    PlayerNearLocation,
    FactionControlsTerritory,
    TerritoryContested,
    RelationshipHostilityAbove,
    RelationshipTrustAbove,
    MarketDemandExceedsSupply,
    StoryNodeOutcome,
    ObjectiveCompleted,
    SimulationTimePassed,
    LocationReached,
    InteractionOccurred,
    ItemDelivered,
    TargetProtected,
    TargetAttacked
};

struct ObjectiveCondition {
    ConditionType type{ConditionType::AlwaysTrue};
    NpcId npc;
    FactionId faction;
    TerritoryId territory;
    DistrictId district;
    WorldPosition position;
    float radius{15.0f};
    StoryNodeId storyNode;
    StoryOutcome storyOutcome{StoryOutcome::Succeeded};
    ObjectiveId objective;
    std::int32_t threshold{0};
    core::GameTime time;
    auto operator<=>(const ObjectiveCondition&) const = default;
};

// Objective Transitions
enum class TransitionTrigger : std::uint8_t {
    OnSuccess = 0,
    OnFailure = 1
};

enum class TransitionAction : std::uint8_t {
    ActivateObjective = 0,
    SkipObjective = 1,
    CompleteMission = 2,
    FailMission = 3
};

struct ObjectiveTransition {
    TransitionTrigger trigger{TransitionTrigger::OnSuccess};
    TransitionAction action{TransitionAction::ActivateObjective};
    ObjectiveId targetObjective{};
    auto operator<=>(const ObjectiveTransition&) const = default;
};

// Objective Definition
struct ObjectiveDefinition {
    ObjectiveId id;
    ObjectiveType type{ObjectiveType::GoTo};
    std::string description;

    NpcId targetNpc;
    FactionId targetFaction;
    TerritoryId targetTerritory;
    DistrictId targetDistrict;
    WorldPosition targetPosition;
    float targetRadius{15.0f};
    std::uint32_t payloadItemId{0};
    NpcId recipientNpc;

    ObjectiveCondition startCondition;
    ObjectiveCondition successCondition;
    ObjectiveCondition failureCondition;

    std::vector<ObjectiveTransition> transitions;
    bool optional{false};
    auto operator<=>(const ObjectiveDefinition&) const = default;
};

// Objective Runtime State
struct ObjectiveRuntimeState {
    ObjectiveState state{ObjectiveState::Locked};
    core::GameTime startedAt{};
    core::GameTime completedAt{};
    bool locationReached{false};
    bool interactionOccurred{false};
    bool deliveryCompleted{false};
    bool protectResolved{false};
    bool attackResolved{false};
    auto operator<=>(const ObjectiveRuntimeState&) const = default;
};

// Mission Definition / Template
struct MissionDefinition {
    std::string templateId;
    MissionCategory category{MissionCategory::Side};
    std::string title;
    std::vector<ObjectiveDefinition> objectives;
    ObjectiveId initialObjective;
    std::uint64_t durationTicks{0}; // 0 = no expiration
    MissionReward reward;
    std::vector<ObjectiveCondition> invalidationConditions;
    std::vector<ObjectiveCondition> failureConditions;
    auto operator<=>(const MissionDefinition&) const = default;
};

// Mission Instance
struct MissionInstance {
    MissionId id;
    std::string templateId;
    MissionCategory category{MissionCategory::Side};
    MissionState status{MissionState::Available};

    MissionIssuer issuer;
    StoryNodeId originStoryNode{};

    MissionContext context;
    core::GameTime createdAt{};
    std::optional<core::GameTime> expiresAt;

    std::vector<ObjectiveDefinition> objectiveDefinitions;
    std::map<ObjectiveId, ObjectiveRuntimeState> objectiveStates;
    MissionReward reward;
    auto operator<=>(const MissionInstance&) const = default;
};

// Mission Simulation State
using MissionStore = world::DomainStore<MissionId, MissionInstance>;

struct MissionSimulationConfig {
    std::uint32_t maxActiveMissions{16};
    std::uint32_t maxAvailableMissions{32};
    auto operator<=>(const MissionSimulationConfig&) const = default;
};

struct MissionSimulationStatistics {
    std::uint64_t missionsCreated{0};
    std::uint64_t missionsAccepted{0};
    std::uint64_t missionsActive{0};
    std::uint64_t missionsCompleted{0};
    std::uint64_t missionsFailed{0};
    std::uint64_t missionsExpired{0};
    std::uint64_t missionsInvalidated{0};
    std::uint64_t missionsResolvedByNpc{0};
    auto operator<=>(const MissionSimulationStatistics&) const = default;
};

struct MissionSimulationState {
    std::uint32_t version{currentMissionSimulationVersion};
    MissionSimulationConfig config;
    MissionSimulationStatistics statistics;
    MissionStore instances;
};

// Mission Events
enum class MissionEventKind : std::uint8_t {
    Created,
    Accepted,
    Activated,
    Completed,
    Failed,
    Expired,
    Invalidated,
    ResolvedByNpc,
    ObjectiveCompleted,
    ObjectiveFailed
};

struct MissionEvent {
    MissionEventKind kind{};
    MissionId mission;
    ObjectiveId objective;
    MissionState state{};
    std::uint64_t tick{};
};

using MissionEventBuffer = core::OrderedBuffer<MissionEvent>;

} // namespace faultline::simulation
