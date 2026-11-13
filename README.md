# FAULTLINE

**Build a world that remembers.**

[![FAULTLINE CI](https://github.com/LuCa1506s/FAULTLINE/actions/workflows/ci.yml/badge.svg)](https://github.com/LuCa1506s/FAULTLINE/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](FAULTLINE_GAME_V1/CMakeLists.txt)

FAULTLINE is an open-source C++20 game and world simulation
engine for a procedural crime sandbox.

Its goal is to build a persistent, systemic world where factions,
characters, territories, markets and player actions produce
lasting consequences instead of scripted resets.

The authoritative simulation is independent of rendering,
designed around reproducible behavior, persistent identity,
versioned saves and modular systems.

> **Development status:** Phase 21 - Vehicles implemented.
> FAULTLINE is an active engine prototype with headless tools,
> not a finished playable game.

## Live simulation evidence

Explore the [reproducible 365-day world demo](demo/README.md).

![FAULTLINE procedural city](demo/maps/day030.svg)

Five factions, 41 territories, 8,007 buildings and one
persistent world evolving through autonomous simulation.

- 883 faction operations over 365 simulated days.
- 141 territorial control changes.
- 385,418 NPC semantic updates.
- Identical final StateHash and SHA-256 save digests
  for uninterrupted and save/resume execution.

[Day 0](demo/maps/day000.svg) |
[Day 30](demo/maps/day030.svg) |
[Day 365](demo/maps/day365.svg)

[Reproduce the experiment and inspect the evidence](demo/README.md).

## What FAULTLINE does

| System | Implemented foundation |
| --- | --- |
| Procedural worlds | Versioned geography, roads, districts, parcels, buildings and territories |
| Persistent characters | Semantic NPC identities, relationships, activities and physical residency |
| Factions | Resource-backed operations, territorial conflict and strategic decisions |
| Economy | District markets, income, supply and demand |
| World history | Deterministic four-year strategic simulation |
| Story and missions | Semantic story graph, objective graphs, mission lifecycle and prologue |
| Player and NPC movement | Fixed-tick movement, static collision and active-world streaming |
| Combat | Weapons, ammunition, damage, death and cooldowns |
| Police | Witnesses, crime reports, wanted states, pursuit and arrest |
| Vehicles | Persistent cars, driving, enter/exit, collision and streaming |
| Save system | Versioned binary persistence, historical migrations and state validation |
| Visualization | SVG debug maps for generated worlds and saved runtime state |

These are implemented foundations, not promises that every
gameplay system is complete.

## Why deterministic simulation?

FAULTLINE separates semantic world state from transient physical
entities.

A character or vehicle can retain its persistent identity when
its physical body is removed from the active simulation.
World streaming can later materialize that body again.

The engine uses:

- Versioned procedural generation.
- Explicit deterministic random-number streams.
- A fixed 20 Hz authoritative simulation tick.
- Ordered commands and domain events.
- Canonical state hashing.
- Versioned save formats and validated migration paths.
- A separate persistence layer.

This architecture is intended to support reproducible testing,
persistent worlds and future extensions without tying simulation
logic to a particular renderer.

Read the [determinism contracts](FAULTLINE_GAME_V1/docs/DETERMINISM.md)
and the [technical architecture](docs/ENGINE_ARCHITECTURE_SPEC.md).

## Quick start

### Requirements

- Git
- CMake 3.20 or newer
- A C++20-capable compiler
- Ninja or another supported CMake build tool

The engine has no declared external runtime library dependencies.
Tests use vendored doctest 2.4.11.

### Clone and build

```sh
git clone https://github.com/LuCa1506s/FAULTLINE.git
cd FAULTLINE/FAULTLINE_GAME_V1

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure --no-tests=error
```

### Windows portable toolchain

On Windows, the optional bootstrap can install the compiler,
CMake and Ninja inside the project without requiring a
system-wide compiler installation.

Python 3 and PowerShell are required for this workflow.

```powershell
git clone https://github.com/LuCa1506s/FAULTLINE.git
cd FAULTLINE/FAULTLINE_GAME_V1

python tools/bootstrap_windows.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

See the [engine README](FAULTLINE_GAME_V1/README.md)
for detailed build instructions.

## Try the simulation

The following examples use a Linux or other Unix-like shell.
On Windows, use the corresponding `.exe` executable.

### Generate a world

```sh
./build-release/faultline_worldgen --seed 12345 --validate
```

### Run 30 simulated days

```sh
./build-release/faultline_sim --seed 12345 --days 30 --save build-release/world.flsave
```

### Resume the persistent world

```sh
./build-release/faultline_sim --load build-release/world.flsave --days 335
```

### Validate vehicle simulation

```sh
./build-release/faultline_vehicles --range 1 100 --ticks 240
```

### Visualize a generated world

```sh
./build-release/faultline_map2d --seed 12345 --output build-release/world.svg
```

Open `build-release/world.svg` in a browser.

The map viewer is a non-authoritative development tool:
it does not advance simulation time or modify persistent state.

See the [Map2D documentation](FAULTLINE_GAME_V1/docs/MAP2D_DEBUG_VIEWER.md)
for available modes and visualization options.

## Architecture

```text
Headless tools / future game application
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
| core | Typed IDs, RNG, hashing, clock and diagnostics |
| world | World generation, entity stores, spatial indexing and streaming |
| simulation | Authoritative game state and domain systems |
| persistence | Serialization, validation and historical save migration |
| tools | Headless simulation, inspection and debugging |

## Verification

FAULTLINE has automated tests covering world generation,
simulation, persistence, determinism and implemented
gameplay foundations.

### Recorded Release results

| Environment | Compiler | Result |
| --- | --- | --- |
| Windows clean GitHub clone | GCC 16.2.0 | 42/42 CTest tests passed |
| Ubuntu 24.04 GitHub Actions | GCC 13.3.0 | 42/42 CTest tests passed |
| Windows GitHub Actions | GCC 16.2.0 | 42/42 CTest tests passed |

All three runs include the complete 14-test stress group.

The Windows clean-clone run was performed on a Surface Laptop 4
and completed all 94 build steps.

Both hosted Windows and Linux Release jobs passed the complete suite in GitHub Actions at commit 434acac.

- [Windows and Phase 21 verification report](FAULTLINE_GAME_V1/docs/PHASE21_VERIFICATION.md)
- [Linux CI test results](https://github.com/LuCa1506s/FAULTLINE/actions/runs/43418877514/job/131863580026)
- [Continuous integration](https://github.com/LuCa1506s/FAULTLINE/actions)

### Phase 21 vehicle evidence

- 100/100 worlds executed in the recorded vehicle stress run.
- 4,464 vehicles encountered across the tested worlds.
- Zero recorded save, continuation, StateHash or identity mismatches.
- Five additional deterministic collision cases passed.

The results apply to the documented test scenarios and revisions.
Passing on multiple platforms does not, by itself, establish
universal bit-for-bit equivalence across all platforms or compilers.

## Development status

| Contract | Current version |
| --- | --- |
| GameVersion | 0.1.0 |
| GeneratorVersion | 3 |
| SaveFormatVersion | 14 |
| StateHash | FAULTLINE.state.v16 |
| VehicleSimulationVersion | 1 |
| Implementation checkpoint | Phase 21 |

Current limitations include:

- No production graphical renderer or complete playable release.
- No full traffic scheduling or advanced vehicle physics.
- No implemented motorcycle, boat or aircraft gameplay.
- Fixed-distance streaming; velocity-adaptive prefetch is deferred.
- Phase 22 and later systems are outside the current checkpoint.

See [DEVELOPMENT_STATUS.md](FAULTLINE_GAME_V1/DEVELOPMENT_STATUS.md)
for the implementation history and known limitations.

## Repository structure

```text
FAULTLINE/
  .github/workflows/        Continuous integration
  docs/                     Public technical specifications
  FAULTLINE_GAME_V1/
    src/                    Engine and simulation source
    tests/                  Automated tests and historical fixtures
    tools/                  Headless tools and development utilities
    docs/                   Implementation and verification documents
    CMakeLists.txt          C++20 build configuration
  CONTRIBUTING.md
  LICENSE
  LICENSING.md
  NOTICE
  THIRD_PARTY_LICENSES.md
```

## Documentation

- [Engine README](FAULTLINE_GAME_V1/README.md)
- [Technical specification index](docs/TECHNICAL_INDEX.md)
- [Engine architecture](docs/ENGINE_ARCHITECTURE_SPEC.md)
- [Development status](FAULTLINE_GAME_V1/DEVELOPMENT_STATUS.md)
- [Determinism](FAULTLINE_GAME_V1/docs/DETERMINISM.md)
- [Save format v14](FAULTLINE_GAME_V1/docs/SAVE_FORMAT_V14.md)
- [Phase 21 verification](FAULTLINE_GAME_V1/docs/PHASE21_VERIFICATION.md)

## Contributing

Contributions, reproducible bug reports and technical discussions
are welcome.

Useful areas include:

- Determinism and cross-platform verification.
- Save compatibility and malformed-input testing.
- Simulation correctness and performance.
- World generation and streaming.
- Development tools and documentation.

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes.

Changes to simulation behavior should include reproducible tests
and preserve existing persistence and identity contracts,
or explicitly document a versioned migration.

## License

Original source code and technical documentation are licensed
under the [Apache License 2.0](LICENSE).

Creative game materials and branding are subject to the
separate licensing scope described in [LICENSING.md](LICENSING.md).

Third-party materials retain their applicable upstream licenses.
See [NOTICE](NOTICE) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

---

**FAULTLINE - Build a world that remembers.**
