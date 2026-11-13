#pragma once
#include "world/state/world_state.hpp"

namespace faultline::world {
enum class SimulationLevel : std::uint8_t { Active, Simulated, Abstract, Dormant };
struct StreamingConfig { double activationRadius{512}, deactivationRadius{640}; };
class WorldStreamingSystem {
public:
    explicit WorldStreamingSystem(StreamingConfig config = {});
    // Call at a structural synchronization point, never during component iteration.
    void commit(WorldState& world, WorldPosition focus) const;
    [[nodiscard]] SimulationLevel desiredLevel(bool active, double distance) const;
private:
    StreamingConfig config_;
};
EntityId materializeNpc(WorldState& world, NpcId id);
void rebuildSpatialIndex(WorldState& world);
}
