# Phase 21 verification

Phase 21 implements simple V1 cars and persistent vehicle identity, and stops
before Phase 22. Starting HEAD was `ed9be6f` ("Phase 20: Witness and Police") on
`master`; all vehicle work was uncommitted in the working tree at review time.

## Versions and fixtures

- GameVersion 0.1.0; GeneratorVersion 3 (unchanged).
- SaveFormatVersion 14; StateHash `FAULTLINE.state.v16`.
- VehicleSimulationVersion 1; all other domain versions remain 1.
- Save version 13 is the last format without semantic vehicles. Authentic v12
  (`tests/fixtures/save_v12_phase19.flsave`) is used to prove neutral vehicle
  migration; v13 saves follow the same migration path.
- Historical BaseWorldHash contracts remain v1 `2938f0ca42162bc9`, v2
  `5f3a446fe0d2f974`, and v3 `888c5dbd10c15886`. Vehicle fleets are runtime
  domain state and are not part of `GeneratedWorld` or `baseWorldHash`.

## Implementation summary

- `VehiclePersistentId` is the authoritative key of `VehicleStore`. It is a typed
  semantic ID distinct from `EntityId`; a body is an optional transient
  representation (`world_state.hpp`, `vehicle_types.hpp`).
- `VehicleState` persists category, archetype, ownership, condition, location,
  abstract position, yaw, speed and passenger capacity. V1 materials are cars:
  Sedan, Hatchback and PoliceCruiser. Motorcycle/Boat/Aircraft enum values exist
  only so future categories do not change identity or links; none is implemented.
- Physical components while materialized: `Transform`, `Health`, `VehicleBody`,
  `VehiclePhysics`, `VehicleLink`. Bodies are never saved.
- Fresh games seed a deterministic civilian + police fleet from the generated road
  graph using a named sub-seeded stream (`runtime.vehicles.spawn`); restored and
  migrated games never invent a fleet or consume that stream.
- `WorldStreamingSystem` materializes vehicles inside the Active radius and
  abstracts them outside the deactivation radius. The vehicle being driven is
  pinned; its bodies are transient and are rebuilt from semantic state.
- Enter/exit and driving run on the fixed 20 Hz / 50 ms simulation tick from
  `SimulationClock`; renderer input is only a `PlayerInputFrame`. Steering uses the
  materialized `VehiclePhysics.handling`, initialized from the archetype definition,
  so there is one runtime source of truth. Static collision reuses the shared
  swept `moveWithRadius` path and applies bounded condition damage.
- Residency leases preserve the allocated `EntityId` across save/load. Both
  `restoreVehicleResidency` and streaming reuse a persisted lease rather than
  minting a new handle, so semantic residency and physical allocation cannot
  diverge.

## Review repair (MUST FIX items)

1. Entity allocation: `materializeVehicle` now accepts an optional `EntityId`
   lease, `restoreVehicleResidency` delegates to it, and
   `WorldStreamingSystem::commit` accepts sorted `VehicleEntityLease` values and
   reuses them. A fresh `EntityId` is allocated only when no lease exists.
2. Incapacitated player: `updatePlayerMovement` clears `drivenVehicle` and still
   runs streaming residency collection when player health is zero, so a dead
   player no longer leaves a frozen driven vehicle.
3. `VehiclePhysics.handling` is now the steering source at runtime.
4. `tools/vehicles` skips seeds with no fleet/clear segment and reports
   `Skipped seeds`, instead of aborting the mass-seed gate.
5. Speed-adaptive prefetch is explicitly deferred (below).

## Additional code fixes (post-review repair)

6. **Player arrest while driving (A):** `resolvePlayerArrest` in
   `src/simulation/police/police_system.cpp` now clears
   `state.vehicleSimulation.drivenVehicle = {}` before player destruction, so the
   headless advance does not re-synch the player into a pinned vehicle.
7. **External fatal vehicle damage (B):** `updateVehicleControl` in
   `src/simulation/vehicles/vehicle_system.cpp` now clears `drivenVehicle` when the
   vehicle's health is <= 0, preventing the player from referencing an undrivable
   entity.
8. **Player death while driving (C):** Already handled in
   `src/simulation/player/player_system.cpp` `updatePlayerMovement` — when player
   health <= 0, `drivenVehicle` is cleared and residency is committed before
   returning early.
9. **Legacy wrecked vehicle migration (D):** `migrateLegacyVehicles` in
   `src/persistence/save.cpp` now sets `health.current = health.maximum` when a
   legacy vehicle had health <= 0 (wrecked), so the resident is eligible for
   streaming post-migration and passes post-load validation.
10. **Vehicle exit collision (E):** `exitDrivenVehicle` in
    `src/simulation/vehicles/vehicle_system.cpp` now computes both right-side and
    left-side exit positions using `moveWithRadius` swept collision checks, and
    selects the side that does not collide with solid geometry, with right-side as
    last-resort fallback.

## Explicit V1 deferrals

- **Speed-adaptive streaming prefetch (WORLD_SPATIAL_STREAMING_SPEC section 20) is
  deferred for V1.** Streaming uses the current player position with fixed
  activation/deactivation radii (512 m / 640 m) and pins the driven vehicle so it
  cannot despawn mid-drive. There is no velocity-based look-ahead or directional
  prefetch extension. This is a performance and visual-continuity enhancement, not
  a determinism or identity requirement; adding it later requires no persistent
  schema change.
- No inter-district vehicle travel/arrival schedule, traffic density simulation,
  motorcycle/boat/aircraft, advanced tuning, renderer physics, police response
  vehicles, or Phase 22 content is implemented.

## Focused evidence

Build of the touched targets succeeded after the repair:
`cmake --build build --target faultline_vehicle_tests faultline_vehicles
faultline_persistence_tests faultline_police_tests faultline_police`.

Focused gate:
`ctest --test-dir build -R "vehicle|persistence|police" -LE stress --output-on-failure`
-> **5/5 PASS** in 69.38 s:

- persistence_tests: PASS (8.53 s)
- police_tests: PASS (38.77 s)
- police_process_determinism: PASS (9.69 s)
- vehicle_tests: PASS (2.99 s)
- vehicle_process_determinism: PASS (9.39 s)

The baseline before the repair was also 5/5 PASS, so the repair introduces no
regression in the focused suites.

Vehicle mass-seed gate `faultline_vehicles --range 1 100 --ticks 240` ->
**Validation PASS**, 100/100 worlds ran, 0 skipped:

- Vehicles 4464 (1488 police, 2976 civilian), 304 materialized, 152 abstracted,
  100 entered, 100 exited.
- Save mismatches 0, continuation mismatches 0, StateHash mismatches 0,
  identity mismatches 0.
- Last-seed StateHash `6adeace4b5b4132b`; BatchHash `51e56c4d84beb3ec`.
- 5 deterministic collision tests pass: building/static, pre-save/load, post-continuation,
  cell-boundary, meaningful footprint.

Focused coverage includes seed-deterministic fleet identity, spawn/save byte
identity, materialize/abstractify without identity or allocator drift, enter/drive/
save/load/continuation/exit, police-car category/ownership/identity, v12 neutral
migration with byte-identical retained section payloads, and rejection of malformed
VehicleSimulation versions. Five additional deterministic collision tests exercise
building/static collision, collision immediately before save/load, collision after
continuation, collision near a cell boundary, and meaningful vehicle footprint - all
passing (49/49 total tests).

## Build environment note

A full-tree `cmake --build build` could not finish linking `faultline_history.exe`
because that executable was open in another running process (PID 3432). This is a
Windows file-lock, not a compiler/link error: every source file for the changed
targets compiled, and the focused targets above linked and passed. The full
canonical Release gate must be rerun once that process exits.

## Files

New: `src/simulation/vehicles/vehicle_state.hpp`, `vehicle_system.hpp/.cpp`,
`src/world/state/vehicle_types.hpp`, `tools/vehicles/main.cpp`,
`tests/vehicle_tests.cpp`, `tests/determinism/vehicle_process.cmake`,
`src/persistence/schema_v14.hpp`, `docs/SAVE_FORMAT_V14.md`, this report.

Modified: `CMakeLists.txt`; runtime `game_state.hpp`/`game.cpp`; world
`world_state.hpp/.cpp` and `streaming.hpp/.cpp`; `character_movement.hpp/.cpp`;
player `player_system.cpp`; persistence `save.hpp/.cpp`; persistence and Map2D
process tests; `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`.

No parent canonical specification was modified.
