#pragma once
#include "world/state/world_state.hpp"
#include "world/generation/actor_generation.hpp"
#include "core/events/ordered_buffer.hpp"
#include <limits>

namespace faultline::simulation {
inline constexpr std::uint64_t gameHourTicks=1600; // 3,600,000 / 2250, exactly.
inline constexpr std::uint64_t gameDayTicks=24*gameHourTicks;
inline constexpr std::uint64_t neverTick=std::numeric_limits<std::uint64_t>::max();
enum class OperationType : std::uint8_t { AttackTerritory, DefendTerritory };
enum class OperationStatus : std::uint8_t { Preparing, Active, Resolved, Failed, Cancelled };
[[nodiscard]] inline bool pending(OperationStatus status) {
    return status==OperationStatus::Preparing || status==OperationStatus::Active;
}
struct ResourceCommitment {
    std::uint32_t manpower{};
    std::int64_t paidCost{}; // Mobilization is paid once; cancellation does not refund consumed money.
    auto operator<=>(const ResourceCommitment&) const = default;
};
struct OperationResult {
    std::uint32_t ownLosses{}, opposingLosses{}, garrisonLosses{};
    std::uint16_t strategicPressure{}; // Outcome evidence only. Never territory control authority.
    auto operator<=>(const OperationResult&) const = default;
};
struct FactionOperation {
    FactionId faction, opponent;
    OperationType type{};
    OperationStatus status{OperationStatus::Preparing};
    TerritoryId target;
    OperationId threat; // Defense binds one attack, so its resources cannot defend two fights.
    ResourceCommitment commitment;
    std::uint64_t createdTick{}, activeTick{}, resolutionTick{}, finishedTick{};
    OperationResult result;
    auto operator<=>(const FactionOperation&) const = default;
};
using FactionOperationStore=world::DomainStore<OperationId,FactionOperation>;
struct FactionSimulationConfig {
    std::uint64_t evaluationIntervalTicks{4*gameHourTicks};
    std::uint64_t preparationTicks{2*gameHourTicks}, activeDurationTicks{6*gameHourTicks};
    std::uint64_t defensePreparationTicks{gameHourTicks}, attackCooldownTicks{12*gameHourTicks};
    std::uint32_t minForce{4}, maxForce{12}, maxConcurrentOperations{2};
    std::int64_t attackBaseCost{600}, defenseBaseCost{300}, costPerMember{75}, minimumAttackUtility{1500};
    auto operator<=>(const FactionSimulationConfig&) const = default;
};
struct FactionSimulationState {
    FactionSimulationConfig config;
    std::uint64_t nextEvaluationTick{4*gameHourTicks};
    FactionOperationStore operations;
    // DERIVED: rebuilt on load; all authoritative times/resources live above/in operations.
    std::set<OperationId> pendingOperations;
    std::set<std::pair<std::uint64_t,OperationId>> deadlines;
    world::TerritoryAdjacency adjacency;
};
enum class OperationEventKind : std::uint8_t { Created, Started, Resolved, Failed, Cancelled };
struct FactionOperationEvent {
    OperationEventKind kind{};
    OperationId operation;
    FactionId faction, opponent;
    TerritoryId territory;
    std::uint64_t tick{};
    OperationResult result;
};
using FactionEventBuffer=core::OrderedBuffer<FactionOperationEvent>;
struct FactionResources {
    std::uint64_t totalManpower{}, committedManpower{}, availableManpower{};
    std::uint32_t deployableManpower{}, activeOperations{}, operationalCapacity{};
};
struct OperationCandidate {
    OperationType type{};
    TerritoryId target;
    OperationId threat;
    std::uint32_t manpower{};
    std::int64_t utility{};
};
enum class OperationRejection : std::uint8_t {
    None, InvalidFaction, InvalidTarget, InvalidType, NotAdjacent, OwnTerritory,
    InvalidDefender, InvalidThreat, DuplicateTarget, Capacity, Manpower, Treasury, Cooldown, TimeLimit
};
struct OperationLaunchResult {
    OperationId operation;
    OperationRejection rejection{};
    [[nodiscard]] explicit operator bool() const { return static_cast<bool>(operation); }
};
}
