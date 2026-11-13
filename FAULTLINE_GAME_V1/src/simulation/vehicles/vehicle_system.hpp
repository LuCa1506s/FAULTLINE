#pragma once
#include "simulation/runtime/game.hpp"
#include "simulation/vehicles/vehicle_state.hpp"

namespace faultline::simulation {
void initializeVehicleSimulation(GameState& state);
// Fresh games only: deterministic fleet derived from the generated road graph.
void seedVehicleFleet(GameState& state);
void restoreVehicleResidency(GameState& state);
void collectVehicleResidency(GameState& state);
// Enter/exit and driven-vehicle movement. Returns true when the vehicle path
// consumed player movement for this tick; false leaves ordinary walking enabled.
bool updateVehicleControl(GameState& state, const PlayerInputFrame& input);
[[nodiscard]] VehiclePersistentId nearestActiveVehicle(const GameState& state,
    world::WorldPosition position, double rangeMeters);
void validateVehicleSimulation(const GameState& state);
void appendVehicleSimulationHash(core::StateHash& hash, const GameState& state);
} // namespace faultline::simulation
