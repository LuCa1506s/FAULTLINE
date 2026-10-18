# Session file inventory

The initial Phase 0 session started from an empty working directory. This index
lists the source files now present, including the Phase 12 additions.

Completed and verified Phase 12 adds district market runtime, save v6 / StateHash v8 and market
regression/multi-year gates. Algorithms: docs/MARKET_M1.md; encoding:
docs/SAVE_FORMAT_V6.md; results: docs/PHASE12_VERIFICATION.md. The Phase 12
inventory appears at the end; preceding entries retain historical provenance.

Phase 11 revalidation on 2026-10-17 preserved the existing implementation and
added two integration cases in tests/territory_simulation_tests.cpp: shuffled
and duplicate simultaneous outcomes (including replay after load), and custom
territory rules through contested control and future paid attacks. This session
also updates DEVELOPMENT_STATUS.md and docs/PHASE11_VERIFICATION.md. Fresh CLI
logs and continuation saves use build/phase11-recheck-*; they are generated
artifacts. Pre-existing automation changes remain outside this task.

Phase 11 added the three territory simulation files, schema_v4.hpp, territory
tests, a frozen real Phase 10 v3 save fixture and three territory/v4/verification
documents listed below. Modified files are enumerated in PHASE11_VERIFICATION.md:
runtime/faction orchestration, persistence, foundational world validation,
simulator statistics, regression tests and technical documentation. All gates
passed; STOP before Phase 12 review. Generated algorithms/parent canonical files
and unrelated README_FIRST.txt, install.ps1 and payload/ were left untouched.

Phase 10 added the three NPC simulation files, schema_v3.hpp, NPC tests, the real
Phase 9 v2 save fixture and NPC/v3/verification documentation listed below.
Modified existing files: CMakeLists.txt, actor_types.hpp, world_state.cpp,
streaming.cpp, runtime game_state.hpp/game.hpp/game.cpp, persistence save.hpp/
save.cpp, faction_system.cpp (accounting comment), faction_simulation_tests.cpp,
simulation_process.cmake, tools/simulator/main.cpp, fixture README, README,
DEVELOPMENT_STATUS, DETERMINISM and this index. No worldgen algorithm or
canonical parent document was modified during Phase 10.

Phase 9 added the faction simulation state/system, v2 wire-schema extension,
faction tests, frozen v1 reference encoder/fixture/script, simulation process
test and M1/v2/verification documentation listed below. Modified existing files:
`CMakeLists.txt`, `src/core/id/id.hpp`, `src/world/state/actor_types.hpp`,
`src/world/state/world_state.cpp`,
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
`src/world/state/world_state.cpp`,
`tools/worldgen/main.cpp`, `tests/worldgen_tests.cpp`, `README.md`,
`DEVELOPMENT_STATUS.md`, `docs/DETERMINISM.md` and this index.

Phase 11 local-resistance repair adds docs/SAVE_FORMAT_V5.md and the frozen
pre-repair tests/fixtures/save_v4_phase11.flsave. Modified territory_state.hpp,
territory_system.hpp/.cpp, runtime/game.cpp, persistence/save.hpp/.cpp and
territory_simulation_tests.cpp implement and verify causal resistance, save v5,
StateHash v7, and v1..v4 migrations. Updated README.md, docs/TERRITORY_M1.md,
docs/DETERMINISM.md, tests/fixtures/README.md, DEVELOPMENT_STATUS.md,
docs/PHASE11_VERIFICATION.md and this index. Generated repair logs/saves are
build/phase11-repair-*. No parent canonical source or worldgen algorithm changed.

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
* `docs/TERRITORY_M1.md`
* `docs/SAVE_FORMAT_V4.md`
* `docs/SAVE_FORMAT_V5.md`
* `docs/PHASE11_VERIFICATION.md`

## Engine

* `src/persistence/save.hpp`
* `src/persistence/binary.hpp`
* `src/persistence/schema_v1.hpp`
* `src/persistence/schema_v2.hpp`
* `src/persistence/schema_v3.hpp`
* `src/persistence/schema_v4.hpp`
* `src/persistence/save.cpp`
* `src/simulation/factions/faction_state.hpp`
* `src/simulation/factions/faction_system.hpp`
* `src/simulation/factions/faction_system.cpp`
* `src/simulation/npc/npc_state.hpp`
* `src/simulation/npc/npc_system.hpp`
* `src/simulation/npc/npc_system.cpp`
* `src/simulation/territory/territory_state.hpp`
* `src/simulation/territory/territory_system.hpp`
* `src/simulation/territory/territory_system.cpp`

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
* `tests/territory_simulation_tests.cpp`
* `tests/mission_tests.cpp`
* `tests/legacy_save_v1.hpp`
* `tests/fixtures/create_v1_reference.ps1`
* `tests/fixtures/save_v1_reference.flsave`
* `tests/fixtures/save_v2_phase9.flsave`
* `tests/fixtures/save_v3_phase10.flsave`
* `tests/fixtures/save_v4_phase11.flsave`
* `tests/fixtures/README.md`
* `tests/determinism/simulation_process.cmake`
* `tests/determinism/worldgen_process.cmake`
* `tests/vendor/doctest.h`
* `tests/vendor/LICENSE.txt`
* `tests/vendor/README.md`

Local generated artifacts are under `.tools/`, `build/` and `.git/` inside the
workspace. They are not source files. No canonical parent documents were edited.

## Phase 12 / District Market M1

New source and verification files:

* `src/simulation/market/market_state.hpp`
* `src/simulation/market/market_system.hpp`
* `src/simulation/market/market_system.cpp`
* `src/persistence/schema_v6.hpp`
* `tests/market_simulation_tests.cpp`
* `tests/fixtures/save_v5_phase11.flsave`
* `docs/MARKET_M1.md`
* `docs/SAVE_FORMAT_V6.md`
* `docs/PHASE12_VERIFICATION.md`

Modified existing files: CMakeLists.txt; runtime/game_state.hpp and game.cpp;
persistence/save.hpp and save.cpp; tools/simulator/main.cpp;
tests/faction_simulation_tests.cpp, persistence_tests.cpp,
territory_simulation_tests.cpp, determinism/simulation_process.cmake;
tests/fixtures/README.md; README.md; docs/DETERMINISM.md;
DEVELOPMENT_STATUS.md and this index. Existing territory corruption tests now
locate section 13's actual end after section 14 was appended; all original
resistance assertions remain. Synthetic persistence fixtures explicitly initialize
their new district markets. Faction accounting now requires exact treasury
conservation including real income and operation spending.

## Phase 13 / Four-Year Strategic History

New source and verification files:

* `src/simulation/history/history_state.hpp` - versioned prologue evidence and compressed records.
* `src/simulation/history/history_system.hpp` - explicit transition and validation API.
* `src/simulation/history/history_system.cpp` - monthly causal strategy / daily market history.
* `src/persistence/schema_v7.hpp` - history-only section field encoding.
* `tools/history/main.cpp` - generated transition, checkpoints, replay and mass-seed tool.
* `tests/history_tests.cpp` - prologue, conservation, persistence, corruption and continuation;
   custom territory presence limits, preserved entrenchment and monthly checkpoint resume.
* `tests/determinism/history_process.cmake` - independent-process equality gate.
* `tests/fixtures/save_v6_phase12.flsave` - frozen real Phase 12 day-30 save.
* `docs/STRATEGIC_HISTORY.md` - algorithm, invariants, commands and scope.
* `docs/SAVE_FORMAT_V7.md` - exact appended wire schema and real v6 migration.
* `docs/PHASE13_VERIFICATION.md` - commands, actual results and limitations.

Modified existing files: CMakeLists.txt; runtime/game_state.hpp and game.cpp;
market/market_system.hpp and market_system.cpp; persistence/save.hpp and save.cpp;
README.md; tests/fixtures/README.md; docs/DETERMINISM.md; DEVELOPMENT_STATUS.md
and this index. Existing tests and canonical BaseWorldHash fixtures are preserved.
Generated logs/saves use `build/phase13-*`. No parent canonical file is changed.
Stop after Phase 13; Phase 14 is not implemented.

## Phase 14 / Story Graph Foundation

Existing partial files inspected and completed in place:

* `src/simulation/story/story_state.hpp` - typed semantic possibility records and graph store.
* `src/simulation/story/story_system.hpp` - initialization, live availability, repair, outcome, validation/hash API.
* `src/simulation/story/story_system.cpp` - deterministic world-derived projection and contact repair.

New files:

* `src/persistence/schema_v8.hpp` - explicit story node/prerequisite encoding.
* `tools/story/main.cpp` - post-history graph inspection, replay, persistence and mutation batch.
* `tests/story_tests.cpp` - focused graph/world mutation, death, migration and continuation tests.
* `tests/determinism/story_process.cmake` - independent-process stdout and save SHA equality.
* `tests/fixtures/save_v7_phase13.flsave` - preserved previously untracked real v7 day-one fixture.
* `docs/STORY_GRAPH.md` - actual authority, lifecycle, conditions, repair and limitations.
* `docs/SAVE_FORMAT_V8.md` - appended wire schema and v1..v7 defaults.
* `docs/PHASE14_VERIFICATION.md` - executed commands and measured results.

Modified existing files: CMakeLists.txt; core/id/id.hpp; runtime/game_state.hpp
and game.cpp; persistence/save.hpp and save.cpp; tests/fixtures/README.md;
README.md; docs/DETERMINISM.md; DEVELOPMENT_STATUS.md and this index. Generated logs, saves and CTest
results remain in build/. No worldgen/history algorithm or canonical parent file
is modified. No Phase 15 Mission Core implementation is included.

## Phase 15 / Mission Core

Existing partial files inspected and completed in place:

* `src/simulation/missions/mission_state.hpp` - mission states, objective states, mission definition and instance structures.
* `src/simulation/missions/mission_system.hpp` - mission system interface: initialization, lifecycle, notification functions, event observation, validation, hashing.
* `src/simulation/missions/mission_system.cpp` - mission system implementation.

New files:

* None (all mission core files were created during Phase 15).

Modified existing files: CMakeLists.txt; core/id/id.hpp (added MissionId, ObjectiveId); runtime/game_state.hpp (added missionSimulation); runtime/game.cpp and game.hpp (added mission simulation initialization, validation, event observation, and state hash); persistence/save.hpp and save.cpp (added mission simulation to save format, increased SaveFormatVersion to 9); simulation/story/story_system.cpp (added call to repairStoryGraph in the tick function); simulation/npc/npc_state.hpp (added NpcEventKind::Died); simulation/npc/npc_system.cpp (added event generation on NPC death).

## Phase 16 / Prologue (PRIMARY 001)

New files:

* `src/simulation/prologue/prologue_state.hpp` — ProloguePhase enum, PrologueState struct.
* `src/simulation/prologue/prologue_system.hpp` — Phase 16 public API.
* `src/simulation/prologue/prologue_system.cpp` — Implementation of mission creation, headless execution, evidence generation, prologue transition.
* `tools/prologue/main.cpp` — Headless prologue tool.
* `tests/prologue_tests.cpp` — 43 end-to-end deterministic test cases.

Modified existing files: CMakeLists.txt (added prologue system, tool, tests); runtime/game_state.hpp (added `prologue` member); runtime/game.cpp (initializePrologueState in constructors, validatePrologueState in validateState, appendPrologueStateHash in stateHash — StateHash v12); persistence/save.hpp (SaveFormatVersion 10); persistence/save.cpp (Section::Prologue, v9→v10 migration); persistence/schema_v10.hpp (PrologueState serialization); tests/persistence_tests.cpp (updated v8 fixture migration test).


## Phase 17 / Player Physical Controller recovery

New or completed inherited files:

- `src/simulation/player/player_state.hpp` ? PlayerState v1 and NPC residency leases.
- `src/simulation/player/player_system.hpp/.cpp` ? creation, movement, residency reconstruction, validation/hash.
- `src/simulation/player/player_script.hpp` ? deterministic semantic verification command stream.
- `src/world/spatial/character_movement.hpp/.cpp` ? shared ground-plane swept static collision/sliding.
- `src/persistence/schema_v11.hpp` ? CharacterBody, PlayerState and residency encoding.
- `tools/player/main.cpp` ? real movement/replay/checkpoint headless tool.
- `tests/player_controller_tests.cpp` ? focused movement, collision, streaming, persistence and continuation tests.
- `tests/determinism/player_process.cmake` ? actual moving-player process/save comparison.
- `tests/save_compatibility.hpp` ? exact old payload comparison with explicit v11 Entities projection.
- `tests/fixtures/save_v10_phase16.flsave` ? authentic recovered Phase 16 writer output.
- `tests/fixtures/save_v7_completed_history.flsave` ? authentic historical v7 writer output.
- `tests/fixtures/phase16_writer.patch`, `tools/recover_phase16_fixture.py` ? reproducible frozen Phase 16 source recovery.
- `docs/PLAYER_CONTROLLER.md`, `docs/SAVE_FORMAT_V11.md`, `docs/PHASE17_VERIFICATION.md` ? architecture/encoding/evidence.

Modified: CMakeLists; runtime game/state; world actor_types/world_state; save reader/writer;
history/story/market/NPC/territory/persistence tests; fixtures README; README;
DEVELOPMENT_STATUS; DETERMINISM. Inherited Phase 16 prologue/schema/tool/tests/docs
remain present and uncommitted; prologue creates no physical entity. The inherited
`.flsave.tmp` remains unused and unchanged, not a historical fixture.

## Phase 18 / Basic Character AI

- `src/simulation/npc/character_ai.hpp/.cpp` - transient local intent, perception,
  stable active-set decision order and shared physical movement execution.
- `tools/characters/main.cpp` - real headless AI movement, residency transition,
  save/continuation and multi-seed diagnostics.
- `tests/character_ai_tests.cpp` - focused physical AI, streaming, collision,
  persistence, signed crossing and long deterministic execution tests.
- `tests/determinism/character_ai_process.cmake` - independent process stdout and
  canonical save SHA-256 comparison.
- `docs/CHARACTER_AI.md` - authority, intent, movement, streaming and scope.
- `docs/PHASE18_VERIFICATION.md` - commands and measured gates.

Modified: CMakeLists.txt; runtime game tick/event access; NPC promotion;
player residency reconstruction; streaming materialization; world validation;
the player diagnostic body count; DEVELOPMENT_STATUS, README and DETERMINISM.
No save schema, StateHash schema, generator or canonical parent document changed.
