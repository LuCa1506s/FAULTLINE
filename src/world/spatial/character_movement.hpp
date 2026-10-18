#pragma once
#include "world/state/world_state.hpp"

namespace faultline::world {
inline constexpr double characterCollisionRadius = 0.35;
// Ground-plane kinematic movement. Generated buildings/water are semantic solids;
// bodies already inside a building can exit (interiors are not implemented).
WorldPosition moveCharacter(const WorldState& world, WorldPosition origin, float dx, float dz);
}
