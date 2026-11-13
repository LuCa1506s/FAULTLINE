#pragma once
#include "simulation/runtime/game_state.hpp"

namespace faultline::simulation {
void initializeNpcSimulation(GameState& state);
void initializeNpc(GameState& state, NpcId npc);
void rebuildNpcSchedule(GameState& state);
void validateNpcSimulation(const GameState& state);
void appendNpcSimulationHash(core::StateHash& hash, const GameState& state);
[[nodiscard]] bool validNpcLocation(const world::WorldState& world, world::WorldLocation location);
[[nodiscard]] std::uint64_t nextNpcDueTick(const GameState& state);
[[nodiscard]] std::uint64_t npcTravelDuration(const GameState& state, world::WorldLocation origin, world::WorldLocation destination);
void startNpcTravel(GameState& state, NpcId npc, world::WorldLocation destination, NpcEventBuffer& events);
void cancelNpcTravel(GameState& state, NpcId npc, NpcEventBuffer& events);
void setNpcLifecycle(GameState& state, NpcId npc, world::NpcLifecycle lifecycle, NpcEventBuffer& events);
// Sole authority for runtime personal relationship changes. Directed sparse edges.
class RelationshipSystem {
public:
    static void apply(GameState& state, RelationshipEvent event, NpcEventBuffer& events);
};
[[nodiscard]] NpcId promoteNpc(GameState& state, EphemeralActor& actor, NpcEventBuffer& events);
void updateNpcs(GameState& state, NpcEventBuffer& events, bool operationsChanged = false);
}
