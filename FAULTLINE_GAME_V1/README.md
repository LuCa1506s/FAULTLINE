# FAULTLINE — Game and Simulation Engine

**Build a world that remembers.**

> **Current checkpoint:** Phase 21 — Vehicles implemented and focused-verified.
> GameVersion `0.1.0` · GeneratorVersion `3` · SaveFormatVersion `14` · StateHash `FAULTLINE.state.v16`.
> The full canonical Release build/test gate remains pending. Phase 22 has not started at this checkpoint.

FAULTLINE is a purpose-built C++20 engine for a procedural crime sandbox with a persistent, deterministic world. The engine separates authoritative simulation from rendering and platform input, allowing worlds, actors, factions, missions and vehicles to be exercised through headless tools and automated tests.

The current project is an engine and simulation prototype, not a finished playable game or a production-ready SDK. Its development history and the scope of verified implementations are documented in [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) and the [Phase 21 verification report](docs/PHASE21_VERIFICATION.md).

## Quick start

Run the following commands from this directory (`FAULTLINE_GAME_V1/`).

### Prerequisites

- CMake 3.20 or newer (as required by [CMakeLists.txt](CMakeLists.txt)).
- A C++20-capable compiler and a supported CMake build tool.
- Git to clone the project. Python and PowerShell are only needed for the optional Windows tooling below.

The runtime libraries have no external library dependencies. Tests use the vendored doctest 2.4.11 single header under the MIT license ([license](tests/vendor/LICENSE.txt), [upstream information](tests/vendor/README.md)).

### Clone, build and test

```sh
git clone https://github.com/LuCa1506s/FAULTLINE.git
cd FAULTLINE/FAULTLINE_GAME_V1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The commands use standard CMake tooling; a fresh clone and full cross-platform test run have not yet been verified at this checkpoint. Multi-configuration generators (such as Visual Studio) may place executables under `build/Debug/` or `build/Release/` instead of `build/`.

### Optional portable Windows toolchain

The Windows bootstrap installs development tools locally in `.tools/`, without requiring a system-wide compiler installation:

```powershell
python tools/bootstrap_windows.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
```

The bootstrap is documented to download CMake 4.4.4, Ninja 1.13.2 and w64devkit 2.10.1. The build script uses Ninja and g++, and runs the complete CTest suite after building. For Release:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

The execution policy option applies to that child PowerShell process only. For manual use, activate the portable toolchain in your shell with `./tools/activate_toolchain.ps1` (dot-source the script). Debug output goes to `build/`, Release output to `build-release/`.

## Architecture

The main library dependencies point inward:

```text
faultline_sim / gameplay tools
            |
            v
faultline_persistence
            |
            v
faultline_simulation
            |
            v
faultline_world
            |
            v
faultline_core
```

| Module | Responsibility |
| --- | --- |
| `faultline_core` | Typed IDs, deterministic RNG, canonical hashing, integer simulation time and diagnostics. |
| `faultline_world` | Entity and component stores, spatial indexing, streaming, versioned world generation and validation. |
| `faultline_simulation` | GameState, ordered commands, domain events, factions, NPCs, territories, markets, history, story, missions, player movement, combat, police and vehicles. |
| `faultline_persistence` | Versioned binary saves, validation, historical migrations and semantic-state restoration. |
| Headless tools | World generation, simulation, save inspection, history, story, prologue, player, characters, combat, police, vehicles and SVG map inspection. |

The authoritative simulation is single-threaded and independent of rendering and window/input devices. The fixed simulation tick is 50 ms (20 Hz). Renderer and platform code stay outside the simulation libraries; `FixedStepRunner` belongs to the application boundary. See [determinism contracts](docs/DETERMINISM.md) and the [source-file inventory](docs/FILE_INDEX.md).

## Implemented systems

| Area | Current scope |
| --- | --- |
| World generation | Versioned M1/M2/M3 generation: geography, roads, districts, parcels, buildings, faction territories, major NPCs and relationships. |
| Strategic simulation | Scheduled faction operations, territorial control, district markets, NPC activities and relationships, four-year strategic history. |
| Story and missions | Semantic story graph, generic mission lifecycle/objectives and the FIRST DELIVERY prologue. |
| Player and NPC movement | Fixed-tick physical player, static swept collision, active NPC movement and deterministic streaming residency. |
| Combat and police | Weapon definitions, ammo/cooldowns/reload, damage/death, witnesses, delayed crime reports, wanted-state logic and arrests. |
| Vehicles | V1 civilian and police cars, persistent identity, streaming bodies, enter/exit, driving, static collisions and save/load. |
| Persistence | Save format v14, StateHash v16, bounded encoding, historical migration and validation. |
| Visualization | Non-authoritative 2D SVG debug maps of generated worlds and saved state. |

The table describes implemented foundations, not complete gameplay systems. For example, full traffic scheduling, production rendering and Phase 22 content are not part of the current checkpoint.

## Run headless demos

Examples below use the portable Windows Debug build layout. For Linux or other Unix-like environments, omit `.exe`; for multi-configuration generators, adjust the executable directory as described above. From a standard Windows PowerShell shell:

### Generate and inspect a world

```powershell
./build/faultline_worldgen.exe --seed 12345 --validate
./build/faultline_worldgen.exe --seed 12345 --dump-factions --dump-npcs
./build/faultline_worldgen.exe --range 1 1000 --validate
```

GeneratorVersion 3 is the default. Versions 1 and 2 remain available with their historical BaseWorldHash contracts. M1 defines bounded geography and a connected road network; M2 adds parcels and urban geometry; M3 adds factions, NPCs and relationships. See [M2 generation](docs/WORLDGEN_M2.md) and [M3 generation](docs/WORLDGEN_M3.md).

### Simulate, save and resume

```powershell
./build/faultline_sim.exe --seed 12345 --days 30 --save ./build/seed12345.flsave
./build/faultline_sim.exe --load ./build/seed12345.flsave --days 335
./build/faultline_savecheck.exe --inspect ./build/seed12345.flsave
```

The headless simulator reports faction strategy, NPC activity, territory and market updates, and StateHash. Scheduled aggregate advancement skips only idle ticks; it uses the same authoritative tick pipeline at scheduled boundaries.

### History, prologue and player systems

```powershell
./build/faultline_history.exe --seed 12345 --fixture-prologue --replay --save ./build/history.flsave
./build/faultline_story.exe --load ./build/history.flsave
./build/faultline_prologue.exe --seed 12345 --verbose
./build/faultline_player.exe --seed 12345 --ticks 12000 --save ./build/player.flsave
./build/faultline_characters.exe --seed 12345 --ticks 1200
./build/faultline_combat.exe --seed 12345 --rounds 6
./build/faultline_police.exe --seed 12345 --ticks 240
```

The synthetic prologue fixture used by history/story tools is distinct from the runtime FIRST DELIVERY mission. These demos are headless and do not require a renderer.

### Phase 21: vehicles

```powershell
./build/faultline_vehicles.exe --seed 12345 --ticks 240 --save ./build/vehicles.flsave
./build/faultline_vehicles.exe --range 1 100 --ticks 240
```

Vehicles have a persistent `VehiclePersistentId` independent of their transient physical `EntityId`. Fresh games seed a deterministic fleet from the road graph; loading and migration do not generate a new fleet. Streaming materializes and abstracts vehicle bodies, preserving semantic state and residency leases. V1 supports Sedan, Hatchback and PoliceCruiser archetypes, enter/exit, fixed-tick driving, and static swept collision with bounded damage.

### 2D debug map viewer

```powershell
./build/faultline_map2d.exe --seed 12345 --output ./build/seed12345.svg
./build/faultline_map2d.exe --seed 12345 --runtime --output ./build/runtime12345.svg --labels
./build/faultline_map2d.exe --load ./build/player.flsave --output ./build/save.svg
./build/faultline_map2d.exe --seed 12345 --output ./build/seed12345.svg --diagnostics
```

Open the generated SVG in a browser. Map2D is a non-authoritative inspection tool: it does not advance simulation or change RNG, persistent schemas or state hashes. Raw seed mode has no player or physical NPCs. Runtime seed mode creates the canonical player and streaming residency without advancing a tick. See [Map2D documentation](docs/MAP2D_DEBUG_VIEWER.md).

## Testing and verification

After building, CTest provides fast, focused and stress suites:

```sh
ctest --test-dir build -C Debug -L fast --output-on-failure
ctest --test-dir build -C Debug -L persistence --output-on-failure
ctest --test-dir build -C Debug -R "vehicle|persistence|police" -LE stress --output-on-failure
ctest --test-dir build -C Debug -L stress --output-on-failure
```

Fast or focused tests do not replace the full canonical milestone gate. For a clean Release validation:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
ctest --test-dir build-release -C Release --output-on-failure
```

### Recorded Phase 21 evidence

The [Phase 21 report](docs/PHASE21_VERIFICATION.md) records the following results after the review repairs:

| Check | Recorded result |
| --- | --- |
| Focused vehicle, persistence and police suites | 5/5 passed |
| Mass-seed vehicle run | 100/100 worlds, zero skipped |
| Simulation ticks per world | 240 |
| Vehicles encountered | 4,464 (1,488 police; 2,976 civilian) |
| Save, continuation, StateHash and identity mismatches | 0 in the tested scenarios |
| Additional deterministic collision cases | 5 passed |
| Vehicle mass-run BatchHash | `51e56c4d84beb3ec` |

These results are historical recorded evidence, not a fresh independent test run of the current GitHub commit. The report states that the full-tree build could not finish linking `faultline_history.exe` because another Windows process held the executable open. The full canonical Release gate remains pending; no claim of full-suite or cross-platform verification is made here.

## Persistence and determinism

| Contract | Current version |
| --- | --- |
| GameVersion | `0.1.0` |
| GeneratorVersion | `3` |
| SaveFormatVersion | `14` |
| StateHash | `FAULTLINE.state.v16` |
| VehicleSimulationVersion | `1` |

Save v14 adds the VehicleSimulation section with semantic vehicle state, driving identity and residency leases. Physical vehicle bodies are transient and are rebuilt on load. Legacy migration initializes a neutral vehicle simulation without seeding a new fleet or consuming gameplay RNG. See [Save format v14](docs/SAVE_FORMAT_V14.md) for validation and migration rules.

Historical world-generation hash contracts remain unchanged: v1 `2938f0ca42162bc9`, v2 `5f3a446fe0d2f974`, v3 `888c5dbd10c15886`.

## Scope and limitations

- Phase 21 is a V1 car simulation foundation, not a complete traffic or vehicle-physics engine.
- Streaming uses fixed 512 m activation and 640 m deactivation radii; the driven vehicle is pinned. Velocity-based prefetch is deferred.
- No inter-district vehicle travel/arrival scheduling, traffic density simulation, motorcycle/boat/aircraft gameplay, advanced tuning or police response vehicles is implemented in this checkpoint.
- No production renderer or complete playable game is claimed. Map2D is a debug tool.
- The full Release gate and independent clean-clone validation remain open. Phase 22 is outside this checkpoint.

## Documentation and development history

| Topic | Reference |
| --- | --- |
| Current checkpoint and historical phases | [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) |
| Published architecture and specifications | [Technical index](../docs/TECHNICAL_INDEX.md) |
| Determinism and source layout | [Determinism](docs/DETERMINISM.md), [File index](docs/FILE_INDEX.md) |
| Strategic simulation | [Factions](docs/FACTION_SIMULATION_M1.md), [NPCs](docs/NPC_SEMANTIC_M1.md), [Territory](docs/TERRITORY_M1.md), [Market](docs/MARKET_M1.md) |
| History and story | [Strategic history](docs/STRATEGIC_HISTORY.md), [Story graph](docs/STORY_GRAPH.md), [First delivery](docs/FIRST_DELIVERY.md) |
| Player, AI and combat | [Player controller](docs/PLAYER_CONTROLLER.md), [Character AI](docs/CHARACTER_AI.md), [Combat](docs/COMBAT.md) |
| Police | [Witness and police](docs/WITNESS_POLICE.md), [Phase 20 verification](docs/PHASE20_VERIFICATION.md) |
| Vehicles | [Phase 21 verification](docs/PHASE21_VERIFICATION.md), [Save v14](docs/SAVE_FORMAT_V14.md) |

Earlier phase reports are historical checkpoints; older version numbers and deferred features in those reports must not be interpreted as the current state. The public technical specifications describe design goals and may include features not implemented yet. The current code and development status are the implementation reference.

## Contributing and license

See [CONTRIBUTING.md](../CONTRIBUTING.md) for contribution guidelines. Preserve deterministic behavior, persistent identity and historical save compatibility; add reproducible tests for changes to simulation contracts.

Original project code and technical documentation are licensed under [Apache License 2.0](../LICENSE), subject to the scope and exceptions in [LICENSING.md](../LICENSING.md). Third-party materials retain their own licenses; see [THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md). The overall third-party inventory is still pending audit.
