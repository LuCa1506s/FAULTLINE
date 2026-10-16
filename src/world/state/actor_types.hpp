#pragma once
#include "world/spatial/world_position.hpp"
#include "core/id/id.hpp"
#include <array>
#include <string>
#include <vector>
#include <optional>
#include <limits>

namespace faultline::world {
enum class NpcLifecycle : std::uint8_t { Alive, Injured, Arrested, Missing, Dead };
enum class FactionLifecycle : std::uint8_t { Active, Collapsed, Dormant, Reformed };
// The four canonical GDD macro-families. Not an extensible five-slot faction count.
enum class FactionFamily : std::uint8_t { AfricanAmericanStreetGang, ItalianMafia, AsianCriminalOrganization, Cartel };
inline constexpr std::size_t mainFactionCount = 5;
inline constexpr std::array<const char*,4> familyNames{
    "African-American street gang", "Italian Mafia", "Asian criminal organization / Yakuza-like", "Cartel"};
enum class AuthorityTier : std::uint8_t { Outsider, Associate, Member, Senior, Command, Leader };
enum class NpcRole : std::uint16_t { FactionMember=1, FactionLeader=2, Dealer=4, MissionIssuer=8 };
struct NpcRoles {
    std::uint16_t bits{};
    void add(NpcRole role) { bits |= static_cast<std::uint16_t>(role); }
    [[nodiscard]] bool has(NpcRole role) const { return (bits & static_cast<std::uint16_t>(role)) != 0; }
    auto operator<=>(const NpcRoles&) const = default;
};
// Normalized values use exact basis points [0,10000]; Respect remains [0,100].
struct NpcTraits {
    std::uint16_t aggression{}, ambition{}, riskTolerance{}, loyaltyBias{}, greed{}, courage{};
    auto operator<=>(const NpcTraits&) const = default;
};
struct NpcCapabilities {
    std::uint16_t combat{}, driving{}, leadership{}, streetKnowledge{}, business{};
    auto operator<=>(const NpcCapabilities&) const = default;
};
enum class AgeBand : std::uint8_t { YoungAdult, Adult, MiddleAged, OlderAdult };
struct NpcIdentity {
    std::string name{};
    AgeBand age{};
    std::uint64_t appearanceSeed{};
    auto operator<=>(const NpcIdentity&) const = default;
};
struct WorldLocation {
    DistrictId district; TerritoryId territory; BuildingId building;
    auto operator<=>(const WorldLocation&) const = default;
};
struct Health {
    float current{100}, maximum{100};
    auto operator<=>(const Health&) const = default;
};
enum class NpcActivity : std::uint8_t { Idle, Working, Travelling, Meeting, Hiding, FactionOperation, Arrested, Recovering };
struct NpcTravelState {
    WorldLocation origin, destination;
    std::uint64_t departureTick{}, arrivalTick{};
    auto operator<=>(const NpcTravelState&) const = default;
};
// PERSISTENT_RUNTIME: deliberately excluded from frozen generated hashes/wire fields.
struct NpcSemanticState {
    NpcActivity activity{};
    WorldLocation home;
    std::optional<NpcTravelState> travel;
    std::uint64_t nextUpdateTick{std::numeric_limits<std::uint64_t>::max()};
    OperationId operation;
    auto operator<=>(const NpcSemanticState&) const = default;
};
struct NpcState {
    NpcLifecycle lifecycle{}; FactionId faction; WorldLocation location{}; EntityId activeEntity;
    WorldPosition position{};
    Health health{};
    NpcIdentity identity{};
    NpcRoles roles{};
    AuthorityTier authority{};
    std::uint8_t respect{};
    std::uint16_t loyalty{};
    NpcTraits traits{};
    NpcCapabilities capabilities{};
    bool major{};
    NpcSemanticState semantic{};
    auto operator<=>(const NpcState&) const = default;
};
struct FactionPersonality {
    std::uint16_t aggression{}, riskTolerance{}, marketFocus{}, territorialFocus{};
    auto operator<=>(const FactionPersonality&) const = default;
};
// Runtime strategy only: never part of the frozen generated-base field encoding.
struct FactionStrategicState {
    std::uint16_t confidence{5000};
    std::uint64_t evaluations{}, lastEvaluationTick{}, nextAttackTick{};
    TerritoryId lastTarget;
    std::int64_t lastUtility{};
    auto operator<=>(const FactionStrategicState&) const = default;
};
struct FactionState {
    FactionLifecycle lifecycle{}; NpcId leader; TerritoryId homeTerritory;
    std::string name{};
    FactionFamily family{};
    FactionPersonality personality{};
    std::int64_t treasury{};
    // Unallocated aggregate members only: excludes territorial garrisons and named NPCs.
    std::uint32_t reserveMembers{};
    std::uint16_t cohesion{};
    FactionStrategicState strategy{};
    auto operator<=>(const FactionState&) const = default;
};
// owner is the legacy derived control mirror; garrison affiliation lives in TerritoryRuntimeState.
struct TerritoryState { DistrictId district; FactionId owner{}; std::uint32_t residentMembers{}; };
struct GeneratedFaction {
    FactionId id; FactionState state;
    auto operator<=>(const GeneratedFaction&) const = default;
};
struct GeneratedNpc {
    NpcId id; NpcState state;
    auto operator<=>(const GeneratedNpc&) const = default;
};
struct TerritoryAssignment {
    TerritoryId territory; FactionId owner; std::uint32_t residentMembers{};
    auto operator<=>(const TerritoryAssignment&) const = default;
};
enum class FactionRelationContext : std::uint8_t { Border=1, SharedFamily=2, SimilarPriorities=4, Distant=8 };
struct FactionRelationship {
    FactionId from, to;
    std::uint16_t trust{}, hostility{}, fear{}, sharedInterest{}, historicalGrievance{};
    std::uint8_t context{};
    auto operator<=>(const FactionRelationship&) const = default;
};
enum class DiplomaticState : std::uint8_t { Friendly, Neutral, Tense, Hostile };
[[nodiscard]] inline DiplomaticState diplomaticState(const FactionRelationship& relation) {
    if (relation.hostility >= 6500) return DiplomaticState::Hostile;
    if (relation.hostility >= 4500) return DiplomaticState::Tense;
    return relation.trust >= 6000 ? DiplomaticState::Friendly : DiplomaticState::Neutral;
}
enum class NpcRelationContext : std::uint8_t { LeadershipTie, RivalLeadership, MeaningfulInteraction };
struct NpcRelationship {
    NpcId from, to;
    std::uint16_t trust{}, fear{}, hostility{}, personalLoyalty{}, debt{};
    NpcRelationContext context{};
    auto operator<=>(const NpcRelationship&) const = default;
};
struct FactionGenerationResult {
    std::vector<GeneratedFaction> factions;
    std::vector<TerritoryAssignment> assignments;
};
struct MajorNpcGenerationResult { std::vector<GeneratedNpc> npcs; };
struct RelationshipGenerationResult {
    std::vector<FactionRelationship> factions;
    std::vector<NpcRelationship> npcs;
};
}
