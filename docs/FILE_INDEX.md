# Session file inventory

The initial Phase 0 session started from an empty working directory. This index
lists the source files now present, including the Phase 10 additions.

Phase 10 added the three NPC simulation files, schema_v3.hpp, NPC tests, the real
Phase 9 v2 save fixture and NPC/v3/verification documentation listed below.
Modified existing files: CMakeLists.txt, actor_types.hpp, world_state.cpp,
streaming.cpp, runtime game_state.hpp/game.hpp/game.cpp, persistence save.hpp/
save.cpp, faction_system.cpp (accounting comment), faction_simulation_tests.cpp,
simulation_process.cmake, tools/simulator/main.cpp, fixture README, README,
DEVELOPMENT_STATUS, DETERMINISM and this index. Final CLI/stress verification passed; Phase 10 is completed and stops before
Phase 11 review.
No worldgen algorithm or canonical parent document was modified.

Phase 9 added the faction simulation state/system, v2 wire-schema extension,
faction tests, frozen v1 reference encoder/fixture/script, simulation process
test and M1/v2/verification documentation listed below. Modified existing files:
`CMakeLists.txt`, `src/core/id/id.hpp`, `src/world/state/actor_types.hpp`,
`src/world/state/world_state.cpp`, `src/simulation/runtime/game_state.hpp`,
both `game` files, `src/persistence/save.hpp`, `src/persistence/save.cpp`,
`tests/persistence_tests.cpp`, `tools/simulator/main.cpp`, `tools/savecheck/main.cpp`,
README, DEVELOPMENT_STATUS, DETERMINISM and this index. No worldgen source or
canonical parent document was changed during Phase 9.

Phase 8 added the four persistence implementation/header files, savecheck tool,
persistence tests, SAVE_FORMAT_V1.md and PHASE8_VERIFICATION.md listed below.
Modified existing files: `CMakeLists.txt`, `src/world/state/stores.hpp`, both
`world_state` files, `src/simulation/runtime/game_state.hpp`, both `game` files,
README, DEVELOPMENT_STATUS, DETERMINISM and this index. No worldgen source or
parent canonical document was changed during Phase 8.

Phase 7 added actor_types.hpp, actor_generation.hpp, actor_names.hpp, the faction,
NPC and relationship generators, actor hash/validation, actor tests, the separate
process CMake test and M3 documentation/verification files listed below.
Existing files modified in Phase 7: `CMakeLists.txt`, both `world_generator` files,
both `world_state` files, `src/simulation/runtime/game.cpp`,
`src/world/spatial/streaming.cpp`, `tests/store_tests.cpp`,
`tests/worldgen_tests.cpp`, `tools/worldgen/main.cpp`, README, DEVELOPMENT_STATUS,
DETERMINISM and this index.

Phase 6 added the urban stage/model/hash/validator files, rectangle geometry/index,
urban tests and WORLDGEN_M2.md listed below. Modified existing files:
`CMakeLists.txt`, `src/core/id/id.hpp`, both `world_generator` files,
`src/simulation/runtime/game.cpp`, `src/world/state/world_state.cpp`,
`tools/worldgen/main.cpp`, `tests/worldgen_tests.cpp`, `README.md`,
`DEVELOPMENT_STATUS.md`, `docs/DETERMINISM.md` and this index.

## Build and documentation

* `.gitignore`
* `CMakeLists.txt`
* `README.md`
* `DEVELOPMENT_STATUS.md`
* `data/README.md`
* `docs/DETERMINISM.md`
* `docs/FILE_INDEX.md`
* `docs/WORLDGEN_M2.md`
* `docs/WORLDGEN_M3.md`
* `docs/PHASE7_VERIFICATION.md`
* `docs/SAVE_FORMAT_V1.md`
* `docs/PHASE8_VERIFICATION.md`
* `docs/FACTION_SIMULATION_M1.md`
* `docs/SAVE_FORMAT_V2.md`
* `docs/PHASE9_VERIFICATION.md`
* `docs/SAVE_FORMAT_V3.md`
* `docs/NPC_SEMANTIC_M1.md`
* `docs/PHASE10_VERIFICATION.md`

## Engine

* `src/persistence/save.hpp`
* `src/persistence/binary.hpp`
* `src/persistence/schema_v1.hpp`
* `src/persistence/schema_v2.hpp`
* `src/persistence/schema_v3.hpp`
* `src/persistence/save.cpp`
* `src/simulation/factions/faction_state.hpp`
* `src/simulation/factions/faction_system.hpp`
* `src/simulation/factions/faction_system.cpp`
* `src/simulation/npc/npc_state.hpp`
* `src/simulation/npc/npc_system.hpp`
* `src/simulation/npc/npc_system.cpp`

* `src/core/id/id.hpp`
* `src/core/rng/stable_hash.hpp`
* `src/core/rng/deterministic_rng.hpp`
* `src/core/rng/deterministic_rng.cpp`
* `src/core/time/simulation_clock.hpp`
* `src/core/diagnostics/diagnostics.hpp`
* `src/core/events/ordered_buffer.hpp`
* `src/app/fixed_step_runner.hpp`
* `src/simulation/runtime/game_state.hpp`
* `src/simulation/runtime/game.hpp`
* `src/simulation/runtime/game.cpp`
* `src/world/state/stores.hpp`
* `src/world/state/world_state.hpp`
* `src/world/state/world_state.cpp`
* `src/world/state/actor_types.hpp`
* `src/world/spatial/world_position.hpp`
* `src/world/spatial/spatial_grid.hpp`
* `src/world/spatial/streaming.hpp`
* `src/world/spatial/streaming.cpp`
* `src/world/generation/world_generator.hpp`
* `src/world/generation/world_generator.cpp`
* `src/world/generation/urban_types.hpp`
* `src/world/generation/urban_generation.hpp`
* `src/world/generation/urban_generation.cpp`
* `src/world/generation/urban_validation.cpp`
* `src/world/generation/urban_hash.cpp`
* `src/world/generation/actor_generation.hpp`
* `src/world/generation/actor_names.hpp`
* `src/world/generation/faction_generation.cpp`
* `src/world/generation/npc_generation.cpp`
* `src/world/generation/relationship_generation.cpp`
* `src/world/generation/actor_validation.cpp`
* `src/world/generation/actor_hash.cpp`
* `src/world/geometry/rect.hpp`
* `src/world/geometry/rect_index.hpp`

## Tools and tests

* `tools/bootstrap_windows.py`
* `tools/activate_toolchain.ps1`
* `tools/build.ps1`
* `tools/simulator/main.cpp`
* `tools/worldgen/main.cpp`
* `tools/savecheck/main.cpp`
* `tests/core_tests.cpp`
* `tests/runtime_tests.cpp`
* `tests/store_tests.cpp`
* `tests/spatial_tests.cpp`
* `tests/worldgen_tests.cpp`
* `tests/urban_tests.cpp`
* `tests/actor_tests.cpp`
* `tests/persistence_tests.cpp`
* `tests/faction_simulation_tests.cpp`
* `tests/npc_simulation_tests.cpp`
* `tests/legacy_save_v1.hpp`
* `tests/fixtures/create_v1_reference.ps1`
* `tests/fixtures/save_v1_reference.flsave`
* `tests/fixtures/save_v2_phase9.flsave`
* `tests/fixtures/README.md`
* `tests/determinism/simulation_process.cmake`
* `tests/determinism/worldgen_process.cmake`
* `tests/vendor/doctest.h`
* `tests/vendor/LICENSE.txt`
* `tests/vendor/README.md`

Local generated artifacts are under `.tools/`, `build/` and `.git/` inside the
workspace. They are not source files. No canonical parent documents were edited.
