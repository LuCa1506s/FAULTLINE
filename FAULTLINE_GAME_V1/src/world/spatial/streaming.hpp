#pragma once
#include "world/state/world_state.hpp"
#include <optional>
#include <span>

namespace faultline::world {
enum class SimulationLevel : std::uint8_t { Active, Simulated, Abstract, Dormant };
struct StreamingConfig { double activationRadius{512}, deactivationRadius{640}; };
// A persisted residency lease. Streaming must reuse the lease allocation rather
// than mint a fresh EntityId, or the persisted residency diverges from the body.
struct VehicleEntityLease {
    VehiclePersistentId vehicle;
    EntityId entity;
    auto operator<=>(const VehicleEntityLease&) const = default;
};
class WorldStreamingSystem {
public:
    explicit WorldStreamingSystem(StreamingConfig config = {});
    // Call at a structural synchronization point, never during component iteration.
    // A pinned vehicle remains materialized regardless of distance (driven bodies).
    // Leases let a restored vehicle reuse its persisted body allocation.
    void commit(WorldState& world, WorldPosition focus,
        std::optional<VehiclePersistentId> pinnedVehicle = {},
        std::span<const VehicleEntityLease> vehicleLeases = {}) const;
    [[nodiscard]] SimulationLevel desiredLevel(bool active, double distance) const;
private:
    StreamingConfig config_;
};
EntityId materializeNpc(WorldState& world, NpcId id);
// A non-null lease restores that exact allocation; otherwise a fresh EntityId is minted.
EntityId materializeVehicle(WorldState& world, VehiclePersistentId id, EntityId lease = {});
void rebuildSpatialIndex(WorldState& world);
}
