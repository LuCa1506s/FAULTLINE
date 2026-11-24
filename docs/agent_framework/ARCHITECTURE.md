# Engine reuse and preliminary SDK contract

## Qualified architecture inventory

The game CMake project lives in `FAULTLINE_GAME_V1`. The authoritative target
chain is `faultline_persistence` → `faultline_simulation` → `faultline_world` →
`faultline_core`. Its headless executables are consumers of these libraries.
CTest drives native doctest suites, multi-seed/long-run tools and independent
process comparisons. The host's graphics stack is outside the target chain.

| Area | Authority and direct reuse | SDK boundary |
| --- | --- | --- |
| Runtime | `GameState` owns persistent state. `Simulation::tick` runs the deterministic subsystem pipeline at 20 Hz. `advanceHeadless` skips idle fixed ticks only when there are no physical entities. | Own both objects per session. Use fixed ticks for player actions and idle advancement in player-controlled sessions. |
| World generation | `world::generateWorld`, typed geometry, road topology, M3 factions and NPCs, and `baseWorldHash`. Default is GeneratorVersion 3. | Seed and approved bounded configuration on reset. Expose projected static geography when needed by a scenario. |
| Factions and territory | Scheduled resource-backed operation evaluation/resolution, territory consumption and derived control. | Preserve engine scheduling and project only approved events. |
| NPCs | Semantic activities, deadlines, separate RNG stream and physical character AI for streamed bodies. | Export values without exposing semantic stores, residency leases or RNG state. |
| Markets | Scheduled district supply/demand, assignments and revenue. | Preserve engine updates. Scenario observation changes require an explicit contract. |
| Player and collision | `createPlayerEntity`, `PlayerInputFrame`, `updatePlayerMovement` and `moveCharacter` with the generated buildings, water and bounds. | Validate discrete actions and translate to input frames. Export player position through the authoritative transform. |
| Persistence | `persistence::saveGameState`/`loadGameState`, bounded binary v14, historical schema migrations and regenerated derived data. | Host-only save/load at completed tick boundaries. Replace the session only after checking restored state. |
| Hash and RNG | `stateHash` domain `FAULTLINE.state.v16`, named xoshiro256** streams, stable FNV-1a encoding and semantic snapshots. | Host-only fingerprint. The runtime owns RNG. |
| Rendering | `faultline_map2d` reads generated or saved state and writes SVG. | Optional external inspection tool. |

The current Windows CI uses the portable GCC toolchain and the Linux job uses
system GCC. Local verification evidence is tracked separately from hosted CI.

## Implemented native and Python surfaces

- An owning C++20 session with an opaque implementation and value-only public
  types, built as a CMake library above the existing engine targets.
- `reset(seed, config)`, `observe()`, `step(action)`, `advance(ticks)`,
  `save(path)`, `load(path)`, `state_hash()` and `close()`. Include bounded
  ordered event output and a status for terminal conditions. Start with idle and
  cardinal movement, validate before ticking and reject limits without mutation.
- Keep the first configuration surface to named world profiles. The standard
  profile uses the game's defaults. A compact, land-only profile can reuse the
  512 m generation size used by the existing history tool. Accept all unsigned
  64-bit seeds. A new physical session places the player at a deterministic
  point on generated terrain using the game's player creation function.
- Share engine snapshots between the C++ and Python host APIs. Validate the
  decoded state before replacement and document the SDK's narrower profile and
  player-control compatibility requirements separately from game save support.
- Expose a generated road topology projection for navigation scenarios. Only
  fields selected by the scenario enter an agent observation. Movement uses the
  same controller and collision tests regardless of road topology.
- Bound profile sizes, per-call ticks and total ticks. Keep scenario metadata,
  reward and agent state above the engine. Events are transient at completed
  fixed-tick boundaries and are not persisted by the engine.
- Agent callbacks receive only observations and return action values. Hashing,
  file I/O and episode configuration are capabilities owned by the host. A
  callback running in the same process as the host is trusted application code.
- Pin version and configuration in reproducibility metadata. Guarantee the
  underlying engine contract for matching builds, platforms, runtime conditions,
  initial semantic state and ordered inputs. Treat platform observations as
  separate test results.

The [C++ SDK](CPP_SDK.md) implements the native contract through `faultline::sdk`.
The [Python extension](PYTHON_SDK.md) binds the same target without moving
runtime logic into Python. The optional [Gymnasium extra](FL_NAVIGATION.md)
owns scenario target selection, observation projection and reward. It calls
native actions and uses native positions for outcomes.

See the original [engine determinism contract](../../FAULTLINE_GAME_V1/docs/DETERMINISM.md)
and [player controller](../../FAULTLINE_GAME_V1/docs/PLAYER_CONTROLLER.md).
