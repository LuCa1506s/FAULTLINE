#pragma once

#include "simulation/runtime/game_state.hpp"
#include "simulation/combat/combat_system.hpp"
#include "simulation/npc/character_ai.hpp"
#include "core/events/ordered_buffer.hpp"

namespace faultline::simulation {

enum class PoliceEventKind : std::uint8_t {
    CrimeRecorded, WitnessObserved, ReportScheduled, ReportInterrupted,
    ReportDelivered, DirectObservation, KnowledgeCreated, WantedTransition,
    ResponseDispatched, SuspectLocated, ContactLost, Arrested
};
struct PoliceEvent {
    PoliceEventKind kind{};
    CrimeEventId crime;
    CrimeReportId report;
    NpcId npc;
    WantedState from{}, to{};
    std::uint64_t tick{};
};
using PoliceEventBuffer = core::OrderedBuffer<PoliceEvent>;

void initializePoliceSimulation(GameState& state);
[[nodiscard]] CrimeEventId recordCrime(GameState& state, CrimeType type,
    CrimeActorRef actor, CrimeActorRef victim, world::WorldPosition position,
    std::uint16_t severityBps, CrimeObservability observability,
    PoliceEventBuffer& events);
void ingestCombatCrimes(GameState& state, const CombatEventBuffer& combat,
                        PoliceEventBuffer& events);
void observeNewCrimes(GameState& state, PoliceEventBuffer& events);
void updateReportsAndWanted(GameState& state, PoliceEventBuffer& events);
void updatePoliceResponse(GameState& state, CharacterAiEventBuffer& aiEvents,
                          PoliceEventBuffer& events);
[[nodiscard]] std::uint64_t nextPoliceDueTick(const GameState& state);
[[nodiscard]] bool isAllowedWantedTransition(WantedState from, WantedState to);
void transitionWanted(GameState& state, WantedState to, std::uint64_t deadline,
                      PoliceEventBuffer& events);
[[nodiscard]] NpcId createPoliceOfficer(GameState& state,
    world::WorldPosition position, world::WorldLocation location);
[[nodiscard]] bool arrestPending(const GameState& state);
void resolvePlayerArrest(GameState& state);
void validatePoliceSimulation(const GameState& state);
void appendPoliceSimulationHash(core::StateHash& hash, const GameState& state);

} // namespace faultline::simulation
