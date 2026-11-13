#pragma once

#include "world/state/stores.hpp"
#include <map>
#include <optional>
#include <vector>

namespace faultline::simulation {

inline constexpr std::uint32_t currentPoliceStateVersion = 1;

enum class CrimeType : std::uint8_t {
    Assault, Murder, WeaponDischarge, PoliceAttack
};
enum class CrimeActorKind : std::uint8_t { Unknown, Player, Npc };
struct CrimeActorRef {
    CrimeActorKind kind{};
    NpcId npc;
    auto operator<=>(const CrimeActorRef&) const = default;
};
enum class CrimeObservability : std::uint8_t { Visual, Audible, VisualAndAudible };
struct CrimeEvent {
    CrimeEventId id;
    CrimeType type{};
    CrimeActorRef actor, victim;
    world::WorldPosition position;
    std::uint64_t tick{};
    std::uint16_t severityBps{};
    CrimeObservability observability{};
    auto operator<=>(const CrimeEvent&) const = default;
};

enum class ReportStatus : std::uint8_t { Pending, Delivered, Interrupted };
struct WitnessObservation {
    CrimeEventId crime;
    NpcId witness;
    std::uint64_t tick{};
    auto operator<=>(const WitnessObservation&) const = default;
};
struct CrimeReport {
    CrimeReportId id;
    CrimeEventId crime;
    NpcId witness;
    std::uint64_t observedTick{}, completionTick{};
    ReportStatus status{ReportStatus::Pending};
    auto operator<=>(const CrimeReport&) const = default;
};
struct PoliceKnowledge {
    CrimeEventId crime;
    bool suspectIdentified{};
    CrimeActorRef suspect;
    world::WorldPosition reportedPosition;
    std::uint64_t knownTick{};
    bool directObservation{};
    std::uint16_t deliveredReports{};
    auto operator<=>(const PoliceKnowledge&) const = default;
};

enum class WantedState : std::uint8_t {
    Clear, Suspicion, Reported, Searching, Located, Pursuit,
    EscapedPending, Cooldown
};
struct WantedContext {
    WantedState state{WantedState::Clear};
    std::uint16_t heatBps{};
    bool suspectIdentified{};
    std::optional<world::WorldPosition> lastKnownPosition;
    std::uint64_t lastConfirmedTick{}, deadlineTick{};
    std::uint32_t confidenceRadiusMeters{};
    auto operator<=>(const WantedContext&) const = default;
};

enum class PoliceResponderStatus : std::uint8_t { Dispatching, Active, Released };
struct PoliceResponder {
    NpcId officer;
    PoliceResponderStatus status{PoliceResponderStatus::Dispatching};
    std::uint64_t dispatchTick{}, arrivalTick{};
    world::WorldPosition target;
    auto operator<=>(const PoliceResponder&) const = default;
};

struct PoliceRules {
    std::uint64_t reportDelayTicks{60};
    std::uint64_t reportedTicks{1};
    std::uint64_t lostContactTicks{40};
    std::uint64_t escapedPendingTicks{100};
    std::uint64_t searchTicks{600};
    std::uint64_t cooldownTicks{5400};
    std::uint64_t suspicionTicks{200};
    std::uint64_t responseTravelTicks{80};
    std::uint64_t arrestAdvanceTicks{120ULL*24ULL*1600ULL};
    std::uint32_t visualRangeMeters{35}, audibleRangeMeters{50};
    std::uint32_t initialSearchRadiusMeters{40}, searchExpansionMeters{5};
    std::uint16_t arrestHealthThreshold{25};
    auto operator<=>(const PoliceRules&) const = default;
};

struct PoliceStatistics {
    std::uint64_t crimes{}, observations{}, reportsScheduled{}, reportsDelivered{};
    std::uint64_t reportsInterrupted{}, directObservations{}, wantedTransitions{};
    std::uint64_t searchTransitions{}, pursuitTransitions{};
    std::uint64_t responses{}, reacquisitions{}, arrests{}, duplicateFacts{};
    auto operator<=>(const PoliceStatistics&) const = default;
};

// Stable source identity makes Combat -> Crime ingestion replay-safe.
struct ProcessedCombatFact {
    std::uint64_t tick{}, eventIndex{};
    auto operator<=>(const ProcessedCombatFact&) const = default;
};

struct PoliceSimulationState {
    std::uint32_t version{currentPoliceStateVersion};
    PoliceRules rules;
    std::uint64_t nextCrimeId{1}, nextReportId{1};
    std::uint64_t witnessProcessedCount{};
    std::vector<CrimeEvent> crimes;
    std::vector<WitnessObservation> observations;
    std::vector<CrimeReport> reports;
    std::vector<PoliceKnowledge> knowledge;
    std::vector<ProcessedCombatFact> processedCombatFacts;
    WantedContext wanted;
    std::vector<PoliceResponder> responders;
    std::map<DistrictId,std::uint16_t> districtPressureBps;
    bool pendingArrest{};
    world::WorldPosition arrestPosition;
    // Non-authoritative process diagnostics: deliberately excluded from save/hash.
    PoliceStatistics statistics;
};

} // namespace faultline::simulation
