#pragma once
#include "core/id/id.hpp"
#include "core/time/simulation_clock.hpp"
#include "world/spatial/world_position.hpp"
#include <cstdint>
#include <vector>

namespace faultline::simulation {
inline constexpr std::uint32_t currentPlayerStateVersion = 1;
inline constexpr float playerMovementSpeedMetersPerSec = 5.0F;
inline constexpr float playerStanceHeightMeters = 1.75F;
inline constexpr float playerTickMovementMeters =
    playerMovementSpeedMetersPerSec * static_cast<float>(core::SimulationClock::tickDuration.count()) / 1000.0F;

// Logical residency preserves hysteresis and already allocated handles across load.
// Bodies, component caches and spatial buckets are rebuilt from NpcState.
struct NpcResidency {
    NpcId npc;
    EntityId entity;
    auto operator<=>(const NpcResidency&) const = default;
};

struct PlayerState {
    std::uint32_t version{currentPlayerStateVersion};
    EntityId controlledEntity{};
    world::WorldPosition spawnPosition{};
    float yaw{};
    std::vector<NpcResidency> residents;
    auto operator<=>(const PlayerState&) const = default;
};
}
