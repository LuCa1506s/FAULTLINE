#pragma once
#include "simulation/factions/faction_state.hpp"

namespace faultline::simulation {
struct NpcSimulationConfig {
    std::uint64_t updateIntervalTicks{gameHourTicks/2};
    std::uint64_t minimumTravelTicks{80};
    std::uint32_t metersPerGameMinute{120};
    auto operator<=>(const NpcSimulationConfig&) const = default;
};
// Bounded diagnostic counters, not a growing actor/event history.
struct NpcStatistics {
    std::uint64_t updates{}, travelsStarted{}, travelsCompleted{}, travelsCancelled{};
    std::uint64_t participations{}, relationshipChanges{}, promotions{};
    auto operator<=>(const NpcStatistics&) const = default;
};
struct NpcSimulationState {
    NpcSimulationConfig config;
    NpcStatistics statistics;
    std::uint64_t nextDueTick{neverTick}; // DERIVED from per-NPC deadlines; never serialized.
};
enum class NpcEventKind : std::uint8_t { ActivityChanged, TravelStarted, TravelCompleted, TravelCancelled, Promoted, RelationshipChanged, OperationAssigned };
struct NpcEvent {
    NpcEventKind kind{}; NpcId npc, other; OperationId operation;
    std::uint64_t tick{};
};
using NpcEventBuffer=core::OrderedBuffer<NpcEvent>;
enum class RelationshipEventKind : std::uint8_t { Helped, Threatened, WorkedWith, Abandoned };
struct RelationshipEvent { RelationshipEventKind kind{}; NpcId from, to; };

// TRANSIENT: no persistent identity until promotion. A token cannot be copied or
// promoted twice; an existing body is linked in place, never destroyed/respawned.
struct EphemeralActor {
    world::NpcState state;
    bool fromFactionReserve{};
    NpcId promotedTo;
    EphemeralActor() = default;
    EphemeralActor(const EphemeralActor&) = delete;
    EphemeralActor& operator=(const EphemeralActor&) = delete;
};
}
