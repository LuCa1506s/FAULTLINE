#pragma once
#include "core/rng/deterministic_rng.hpp"
#include "core/rng/stable_hash.hpp"
#include "core/time/simulation_clock.hpp"
#include <array>
#include "world/state/world_state.hpp"
#include "simulation/factions/faction_state.hpp"
#include "simulation/npc/npc_state.hpp"
#include "simulation/territory/territory_state.hpp"
#include "simulation/market/market_state.hpp"
#include "simulation/history/history_state.hpp"
#include "simulation/story/story_state.hpp"
#include "simulation/missions/mission_state.hpp"

namespace faultline {
enum class RandomDomain : std::size_t { Combat, Npc, Missions, Factions, Market, Police, Story, Count };
struct RuntimeRandomState {
    std::array<core::DeterministicRng, static_cast<std::size_t>(RandomDomain::Count)> streams;
    explicit RuntimeRandomState(WorldSeed seed);
    core::DeterministicRng& get(RandomDomain domain) { return streams.at(static_cast<std::size_t>(domain)); }
};
struct GameState {
    WorldMetadata metadata;
    core::SimulationClock clock;
    RuntimeRandomState randomness;
    world::WorldState world;
    simulation::FactionSimulationState factionSimulation;
    simulation::NpcSimulationState npcSimulation;
    simulation::TerritorySimulationState territorySimulation;
    simulation::MarketSimulationState marketSimulation;
    simulation::StrategicHistory history;
    simulation::StoryGraph story;
    simulation::MissionSimulationState missionSimulation;
    explicit GameState(WorldSeed seed, world::WorldGenerationConfig config = {}, GeneratorVersion version = currentGeneratorVersion);
    // Assemble restored data without rerunning world generation or resetting RNG/allocators.
    GameState(WorldMetadata meta, core::SimulationClock time, RuntimeRandomState random, world::WorldState restored);
};
[[nodiscard]] std::uint64_t stateHash(const GameState& state);
void validateState(const GameState& state);
}
