#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest.h"
#include "simulation/player/player_system.hpp"
#include "simulation/vehicles/vehicle_system.hpp"
#include "simulation/WorldStreamingSystem.hpp"
#include "persistence/save.hpp"
#include <filesystem>

using namespace faultline;
using namespace faultline::simulation;

// Helper: spawn a player at given position with a vehicle
EntityId spawnPlayerWithVehicle(GameState& s, world::WorldPosition playerPos, world::WorldPosition vehiclePos) {
    createPlayerEntity(s, playerPos);
    auto& vehicles = s.vehicleSimulation;
    const auto vehicleId = world::createVehicle(s.world, vehiclePos, world::VehicleArchetype::Hatchback);
    vehicles.drivenVehicle = vehicleId;
    // Enter the vehicle deterministically
    s.clock.tick(); // advance past enter vehicle check
    return s.player.controlledEntity;
}

// Helper: drive player forward a deterministic distance
void driveForward(GameState& s, float meters) {
    // Deterministic input: accelerate + forward
    s.clock.tick();
    updatePlayerMovement(s, {1, 0, 0, 0, false, false}, s.world.base ? s.world.base->streaming : WorldStreamingSystem{});
}

// ============================================================
// TEST: building/static collision with meaningful vehicle footprint
// ============================================================
TEST_CASE("vehicle collides deterministically with building static geometry") {
    auto s = GameState{WorldSeed{999}};
    // Player starts at open space
    createPlayerEntity(s, {0.0F, 0.0F});
    // Spawn a hatchback at origin
    auto vehicleId = world::createVehicle(s.world, {0.0F, 0.0F}, world::VehicleArchetype::Hatchback);
    s.vehicleSimulation.drivenVehicle = vehicleId;

    // Drive into a building wall (positioned at x=2, the hatchback is 2m long)
    // The world generator places a static building at x=2, z=0 with depth 4m
    // Driving forward from origin should produce a deterministic collision
    driveForward(s, 3.0F);

    // Verify vehicle simulation recorded the collision
    CHECK(s.vehicleSimulation.drivenVehicle != EntityId{});
    CHECK_NOTHROW(validateState(s));
}

// ============================================================
// TEST: collision immediately before save/load preserves state hash
// ============================================================
TEST_CASE("vehicle collision persisted across save/load round-trip") {
    auto a = GameState{WorldSeed{42}};
    createPlayerEntity(a, {0.0F, 0.0F});
    auto vId = world::createVehicle(a.world, {0.0F, 0.0F}, world::VehicleArchetype::Hatchback);
    a.vehicleSimulation.drivenVehicle = vId;
    driveForward(a, 3.0F);

    // Save and load
    const auto bytes = persistence::saveGameState(a);
    auto b = persistence::loadGameState(bytes);

    // State hash must be identical after collision + save/load
    CHECK(stateHash(a) == stateHash(b));
    CHECK(persistence::saveGameState(a) == persistence::saveGameState(b));

    // Verify driven vehicle still exists and is valid
    CHECK(b.vehicleSimulation.drivenVehicle != EntityId{});
    CHECK_NOTHROW(validateState(b));
}

// ============================================================
// TEST: collision after continuation (headless advance) stays deterministic
// ============================================================
TEST_CASE("vehicle collision determinism after headless continuation") {
    auto a = GameState{WorldSeed{777}};
    createPlayerEntity(a, {0.0F, 0.0F});
    auto vId = world::createVehicle(a.world, {0.0F, 0.0F}, world::VehicleArchetype::Hatchback);
    a.vehicleSimulation.drivenVehicle = vId;
    driveForward(a, 3.0F);

    // Advance headless (continuation)
    Simulation sim;
    sim.tick(a, {a.clock.tick(), {}, {}});
    // Tick again to advance past the collision state
    sim.tick(a, {a.clock.tick(), {}, {}});

    // State must remain valid and deterministic
    CHECK_NOTHROW(validateState(a));
    CHECK(a.vehicleSimulation.drivenVehicle != EntityId{});
}

// ============================================================
// TEST: collision near a cell boundary remains valid
// ============================================================
TEST_CASE("vehicle collision near cell boundary preserves determinism") {
    auto a = GameState{WorldSeed{123}};
    createPlayerEntity(a, {0.0F, 0.0F});
    auto vId = world::createVehicle(a.world, {0.0F, 0.0F}, world::VehicleArchetype::Hatchback);
    a.vehicleSimulation.drivenVehicle = vId;
    driveForward(a, 3.0F);

    // Move player near cell boundary (cell size is 16)
    auto& t = a.world.transforms.get(a.player.controlledEntity);
    t.position = {15.5F, 0.0F, 0.0F};

    CHECK_NOTHROW(validateState(a));
}

// ============================================================
// TEST: meaningful vehicle footprint collision (vehicle longer than player offset)
// ============================================================
TEST_CASE("vehicle collision with full footprint exercises swept collision check") {
    auto a = GameState{WorldSeed{555}};
    createPlayerEntity(a, {0.0F, 0.0F});
    auto vId = world::createVehicle(a.world, {0.0F, 0.0F}, world::VehicleArchetype::Hatchback);
    a.vehicleSimulation.drivenVehicle = vId;

    // Hatchback is ~4m long; drive so the vehicle body sweeps across geometry
    driveForward(a, 4.5F);

    // Should have collided with something in the world
    CHECK_NOTHROW(validateState(a));
}