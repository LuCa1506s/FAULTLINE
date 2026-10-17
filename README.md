# FAULTLINE

Purpose-built C++20 engine for the FAULTLINE procedural crime sandbox. Canonical
design sources are the GDD, technical specifications and
`../V1_IMPLEMENTATION_ROADMAP.md` in the parent directory. See
[DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) for the verified checkpoint.
The session's source-file inventory is in [docs/FILE_INDEX.md](docs/FILE_INDEX.md).

## Build and tests

With CMake and a C++20 compiler on PATH:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For this Windows machine, an optional portable toolchain stays entirely inside
`.tools/` (no system installation):

```powershell
python tools/bootstrap_windows.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
# Optional optimized build and the same tests:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

The portable bootstrap downloads CMake 4.4.4, Ninja 1.13.2 and w64devkit 2.10.1;
runtime libraries have no external dependencies. Tests use vendored doctest.
The PowerShell policy option applies only to that child process. For manual
commands, open a process with the same option and dot-source
`./tools/activate_toolchain.ps1` to activate the local tools.

## Architecture

Dependencies point inward:

```text
faultline_sim -> faultline_persistence -> faultline_simulation -> faultline_world -> faultline_core
faultline_worldgen -------------------------------------------> faultline_world
faultline_savecheck -> faultline_persistence
faultline_tests / faultline_persistence_tests (doctest only in tests)
```

`faultline_core` owns typed IDs, deterministic RNG, canonical hashing, integer
time and diagnostics. `faultline_world` owns domain/component stores, spatial
coordinates, indexing, basic streaming and M1/M2/M3 generation/validation.
`faultline_simulation` owns GameState, Game, tick input, ordered commands,
domain events, typed structural commit, scheduled aggregate faction operations, runtime territorial presence/control and
semantic NPC activity/travel/relationships/promotion and district markets. FixedStepRunner belongs to the app
boundary. Renderer/platform code cannot enter these libraries.
Authoritative simulation is single-threaded and independent of windows or input
devices. Algorithm and encoding contracts are in
[docs/DETERMINISM.md](docs/DETERMINISM.md).

## Headless tools

```powershell
./build/faultline_worldgen.exe --seed 12345 --validate
./build/faultline_worldgen.exe --seed 12345 --version 1 --validate
./build/faultline_worldgen.exe --seed 12345 --version 2 --validate
./build/faultline_worldgen.exe --seed 12345 --dump-factions --dump-npcs
./build/faultline_worldgen.exe --range 1 1000 --validate
./build/faultline_worldgen.exe --seed 42 --width 512 --depth 512 --water-percent 0
./build/faultline_sim.exe
./build/faultline_sim.exe --seed 12345 --days 30
./build/faultline_sim.exe --seed 12345 --days 365
./build/faultline_sim.exe --range 1 250 --days 30
./build/faultline_sim.exe --seed 12345 --days 30 --save ./build/seed12345.flsave
./build/faultline_sim.exe --load ./build/seed12345.flsave --days 335
./build/faultline_savecheck.exe --seed 12345
./build/faultline_savecheck.exe --inspect ./build/example.flsave
```

M1 includes bounds, flat buildable geography, an optional west/east coast,
5–10 contiguous rectangular macro-zones, a connected road grid with arterial,
major and local roads, and 1–3 districts per macro-zone. The configuration
supports nonzero origins, micro-worlds and larger maps. Current explicit M1
resource limits are 512–1,000,000 meters per axis, not a permanent engine limit.

Generation always validates before returning; `--validate` is also accepted for
explicit tool invocations. Unsupported versions and invalid options fail with
nonzero exit codes and seed/version diagnostics. `BaseWorldHash` covers all
generated fields of the selected version and their configuration in canonical order.

`faultline_sim` runs playerless aggregate faction strategy (default seed 12345,
30 game days). It reports resources, attack/defense outcomes, NPC semantic updates,
travel, operation participation, relationship changes, promotions, territorial
transitions, per-faction control counts, market updates, income and abstract
supply/demand, plus StateHash.
`--log-operations` exposes individual transitions. It uses the normal tick pipeline
at scheduled boundaries and skips only idle ticks; no wall-time sleep is involved.
Territory control follows current presence and attack/defense consequences;
district markets earn income from real Dealer participation; diplomacy, named
NPC lifecycle and police behavior remain unchanged. Details and current limitations are in
[docs/FACTION_SIMULATION_M1.md](docs/FACTION_SIMULATION_M1.md) and
[docs/NPC_SEMANTIC_M1.md](docs/NPC_SEMANTIC_M1.md), with territorial rules in
[docs/TERRITORY_M1.md](docs/TERRITORY_M1.md) and
[docs/MARKET_M1.md](docs/MARKET_M1.md).
Player input is abstract data; movement/combat systems are later roadmap work.
The streaming API commits at explicit structural boundaries; game-driven
streaming focus arrives with the player controller.

Worldgen now defaults to generator version 3 / M3: five main factions from the four
canonical families, 15–25 semantic major NPCs, initial connected territorial
presence and directed political/personal relationships. Reports include `Factions`
and `MajorNPCs`; optional actor dumps expose identities and home references.
`--version 1` and `--version 2` preserve prior worlds/hashes. The mass range command
validates every selected world and reports duplication/NPC/share/diplomacy statistics
plus tool-only timing. The 1000-seed Debug batch has a 900-second CTest timeout.

The full build script still runs all tests. For iteration only, after building:

```powershell
ctest --test-dir build -L fast --output-on-failure
ctest --test-dir build -L stress --output-on-failure
```

The stress gate remains required for a milestone; it is not replaced by fast tests.

Algorithms, configuration and geometric limits are documented in
[docs/WORLDGEN_M2.md](docs/WORLDGEN_M2.md). Verified results and the roadmap
checkpoint are recorded in DEVELOPMENT_STATUS. Actor generation and its limits are
documented in [docs/WORLDGEN_M3.md](docs/WORLDGEN_M3.md).

Save Foundation uses a separate `faultline_persistence` target, full semantic
binary saves with versioned headers and bounded little-endian encoding. The
memory/file API, snapshot policy and actual format are documented in
[docs/SAVE_FORMAT_V8.md](docs/SAVE_FORMAT_V8.md), with real v1..v7 migration. The
[v1 document](docs/SAVE_FORMAT_V1.md) remains historical reference. The savecheck tool verifies
round-trip hash/bytes and reports size/timing; it generates no files by default.
Use `ctest --test-dir build -L persistence --output-on-failure` for save tests.


Phase 13 adds `faultline_history`, a headless four-year structural transition:

```powershell
build/faultline_history.exe --seed 12345 --fixture-prologue --replay --save build/history.flsave
build/faultline_sim.exe --load build/history.flsave --days 365
```

Fresh tool generation explicitly uses synthetic prologue evidence; the real FIRST
DELIVERY mission remains Phase 16. HistoryVersion 1 uses 48 monthly strategic
steps, daily coarse market accounting, paid dealer spread, actual garrison
settlement and causal political/personal relationships. All five factions enter
a viable, city-wide market with unresolved tensions. Monthly checkpoints save
and resume exactly. See [STRATEGIC_HISTORY.md](docs/STRATEGIC_HISTORY.md) and
[PHASE13_VERIFICATION.md](docs/PHASE13_VERIFICATION.md).

Phase 14 adds an explicitly initialized post-history semantic story graph:

```powershell
build/faultline_story.exe --seed 12345 --fixture-prologue --save build/story.flsave
build/faultline_story.exe --load build/history.flsave
ctest --test-dir build -L story --output-on-failure
```

The graph projects high-level possibilities and success/failure branches, queries
actual world eligibility and repairs invalid contacts using existing semantic
actors. It never forces world outcomes. Save v8 / StateHash v10 persist graph
identity, dependencies and outcomes. Details: [STORY_GRAPH.md](docs/STORY_GRAPH.md).
This includes no Mission Core or runtime FIRST DELIVERY; stop before Phase 15.

Phase 15 adds the Mission Core: generic mission system with MissionState lifecycle
(AVAILABLE, ACCEPTED, ACTIVE, COMPLETED, FAILED, EXPIRED, INVALIDATED, RESOLVED_BY_NPC),
objective graph (GO_TO, INTERACT, DELIVER, PROTECT, ATTACK), typed IDs (MissionId,
ObjectiveId), MissionDefinition/MissionInstance separation, event-driven architecture
via semantic events (LocationReached, InteractionOccurred, DeliveryOccurred,
ProtectedTargetResolved, AttackTargetResolved), StoryGraph bridge, persistence
(SaveFormatVersion 9, StateHash v11), validation, and deterministic save/load.
All tests pass, including persistence, story, core, faction, NPC, territory, and
market tests. Historical BaseWorldHash fixtures remain unchanged: v1 2938f0ca42162bc9,
v2 5f3a446fe0d2f974, v3 888c5dbd10c15886.

This includes no runtime FIRST DELIVERY; stop before Phase 16.
