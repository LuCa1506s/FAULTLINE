#pragma once
#include "simulation/factions/faction_state.hpp"

namespace faultline::simulation {
inline constexpr std::uint32_t currentHistoryVersion=1;
inline constexpr std::uint32_t historyMonths=48, historyDays=1460;
enum class DiscoveryIncident : std::uint8_t { RivalWitness, Betrayal, AccidentalExposure, PoliceInterference, WrongDestination, InformationLeak };
// Evidence supplied by the prologue, not a replacement mission implementation.
struct PrologueEvidence {
    FactionId origin, rival;
    NpcId witness;
    TerritoryId location;
    DiscoveryIncident incident{};
    std::uint64_t deliveryTick{}, discoveryTick{};
    bool firstDeliveryCompleted{};
    auto operator<=>(const PrologueEvidence&) const = default;
};
enum class HistoryEventKind : std::uint8_t { FirstDelivery, RivalDiscovery, MarketEntry, DealerExpansion, TerritorialSettlement, MarketDispute, Cooperation, NetworkMaintenance };
struct HistoryEvent {
    HistoryEventKind kind{};
    std::uint32_t month{};
    FactionId faction, other;
    NpcId npc;
    TerritoryId territory;
    std::int64_t cost{};
    auto operator<=>(const HistoryEvent&) const = default;
};
struct StrategicHistory {
    std::uint32_t version{}, months{};
    std::uint64_t startTick{};
    PrologueEvidence prologue;
    std::vector<FactionId> informed;
    std::vector<HistoryEvent> events;
    auto operator<=>(const StrategicHistory&) const = default;
};
}
