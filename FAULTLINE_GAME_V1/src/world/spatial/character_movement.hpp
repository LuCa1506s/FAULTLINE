#pragma once
#include "world/state/world_state.hpp"

namespace faultline::world {
inline constexpr double characterCollisionRadius = 0.35;
struct MovementResult {
    WorldPosition position;
    bool collided{};
    auto operator<=>(const MovementResult&) const = default;
};
// Ground-plane kinematic movement. Generated buildings/water are semantic solids;
// bodies already inside a building can exit (interiors are not implemented).
// The radius is explicit so vehicles share the same deterministic swept collision.
MovementResult moveWithRadius(const WorldState& world, WorldPosition origin,
    float dx, float dz, double radius);
WorldPosition moveCharacter(const WorldState& world, WorldPosition origin, float dx, float dz);
// Static simulation visibility. This deliberately shares authoritative generated
// geometry with combat and witness/police perception; render visibility is irrelevant.
[[nodiscard]] bool hasStaticLineOfSight(
    const WorldState& world, WorldPosition from, WorldPosition to);
}
