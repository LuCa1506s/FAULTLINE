# Phase 17 verification — recovered Player Physical Controller

Status: VERIFIED COMPLETE. Stop before Phase 18.

## Handoff recovery

Executed `git status --short`, `git diff --stat`, `git diff`, `git diff --cached`,
`git log -15 --oneline --decorate`, `git worktree list` and inspected untracked
source/test/document files plus fixture headers. HEAD is **e60833b (Phase 15)**;
there is no committed final Phase 16 boundary. Cached diff was empty. The dirty
tree combines completed Phase 16 prologue work and interrupted Phase 17 work.
No reset, clean, commit or discard was performed.

Correct inherited work retained: PlayerState/body/component stores, typed entity
identity, explicit input pipeline, movement/facing foundation, save-v11/schema-v13
integration, CharacterBody version-gated old-save reading, and the Phase 16 code,
tests, tools, historical fixture and documents.

Incorrect/incomplete work corrected:

- Physical movement used accelerated game-time 2250 ms rather than physical 50 ms.
- Saving omitted active NPC bodies but lost logical residency and allocation
  reservations needed by hysteresis/semantic activity and future entity allocation.
- The purported player process gate invoked StoryGraph with no movement.
- Cell-crossing fixture sent motion on the wrong axis for its spawn position.
- Old history/story byte assertions had been removed. They were restored as exact
  payload comparisons accounting ONLY for the explicit new empty CharacterBody field.
- A completed-history migration test had been replaced by unrelated last-byte
  corruption. It now loads an authentic completed-history v7 writer fixture.
- Inherited automatic player creation inside `runPrologue` broke the valid Phase 16
  no-physical-entity regression. Removed that coupling; explicit physical session
  setup creates the player after the prologue. The older test was not weakened.
- Basic collision and usable interaction intent were missing; both are implemented.
- Inherited aggregate-initializer warnings in Prologue conditions were corrected
  by zero initialization followed by explicit type assignment (unchanged semantics).

Existing external worktrees were inspected only. The prunable Phase 14 entry and
unfinished external Phase 15/v10 worktree were neither reused as trusted v10 writers
nor changed/deleted. All new recovery source/builds/artifacts stay inside this repo.

## Proven root cause, first divergence and actual fix

The inherited continuation test could pass accidentally: after 100 ticks at the
incorrect 45x speed, the player had left NPC streaming range. A stronger fixture
spawns at an actual NPC and saves after 10 movement ticks, retaining active bodies.
Before continuation, hash and canonical bytes match and both allocators are 22.

**First divergent completed tick: 11**. After streaming commit:

| Field | Original | Loaded continuation |
| --- | --- | --- |
| EntityRegistry.nextId | 22 | 35 |
| live entities | 21 | 14 |
| NPC #1 active body | Entity #2 | Entity #22 |

The original retained active NPCs and the 640 m deactivation hysteresis. The loaded
world had abstract NPCs and recreated 13 bodies using the 512 m activation radius,
consuming IDs 22..34. This changes authoritative allocator future immediately and
can subsequently change NPC decision/RNG behavior at semantic deadlines because
the NPC system treats physical and abstract actors differently. Neither deleting
the hash assertion nor ignoring EntityRegistry.nextId would fix it.

Fix: persist ordered logical `(NpcId, reserved EntityId)` residency leases in the
player's streaming state. Save still synchronizes NPC Transform/Health into semantic
NpcState and omits transient bodies. Load leaves bodies abstract. Before any next
tick subsystem runs, reconstruct existing leases from those semantic records,
without calling/rewinding the allocator. End-of-tick streaming synchronizes leases.
This retains hysteresis and semantic scheduling as well as entity allocation.
Leases are explicitly validated, hashed and serialized; body caches remain derived.
No transient ID namespace, allocator exclusion, per-tick destroy/reallocate loop,
mutating save boundary or physical-NPC serialization was introduced.

Focused continuation now runs **10 ticks before save + 1700 after load**, comparing
StateHash, allocator, live IDs and all RNG states after EVERY tick, crossing NPC
and faction semantic deadlines. Final canonical save equality is also required.
An additional 550 m hysteresis-ring fixture proves exact handle restoration with
no allocation. The standalone tool exercises **6000 + 6000 ticks** per script.

## Player model and scope

PlayerStateVersion 1 identifies one persistent controlled EntityId. The entity's
Transform/WorldPosition owns position; CharacterBody owns facing/speed/stance height.
PlayerState.yaw is a synchronized cache; validation rejects divergent values.
Default speed 5 m/s, stance height 1.75 m, physical step 50 ms => 0.25 m full-speed
cardinal motion per tick. Analog magnitude is retained below one, diagonal input
is clamped to unit length. Aim controls facing when nonzero, otherwise movement
controls facing, otherwise heading remains. Range [-pi,pi].

Positive/negative and diagonal cell crossings, multiple-cell movement, 16000-command
stress normalization, idle/forward/back/strafe and direction reversals are tested.
WorldPosition remains cell + local; no giant global float position replaces it.
Kinematic swept expanded-rectangle collision clips/slides against buildings/water;
existing inside-building actors may exit. Bounds constrain actors already inside.
Interaction input produces scoped one-tick semantic intent without mission rewards
or implicit completion. There are no keycodes or renderer-owned movement rules.

## Versions and migration

| Version | Current |
| --- | --- |
| SaveFormatVersion | 11 |
| StateHash | FAULTLINE.state.v13 |
| PlayerStateVersion | 1 |
| GameVersion | 0.1.0 |
| GeneratorVersion | 3 |
| HistoryVersion / StoryGraphVersion / PrologueStateVersion | 1 / 1 / 1 |

The intended inherited Phase 17 versions were kept, not bumped again.
V1..v10 migration creates default PlayerState and no CharacterBodies. The v11
reader reads CharacterBody only for v11+, preserving historical layouts. V10
Prologue state is preserved, as are earlier migrations. Old tests retain every
historical payload byte after projecting out the new required-empty body collection
from v11 Entities; v4 also retains its already-existing versioned resistance suffix
exception. No StateHash-only replacement of wire/payload assertions was made.

### Authentic v10 writer and fixture

HEAD e60833b + frozen recovered Phase 16 additions in `tests/fixtures/phase16_writer.patch`.
This excludes only identified Phase 17 additions in an isolated source copy.
Actual v10 C++ writer is built and executed; no header patching, section stripping,
renaming of v11 output or truncation is used. Recovery is independently reproducible:

```powershell
python tools/recover_phase16_fixture.py
# Activate the repository toolchain, then:
cmake -S build/phase16-writer-reproduction -B build/phase16-writer-reproduction/out -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTING=OFF
cmake --build build/phase16-writer-reproduction/out --target faultline_prologue
build/phase16-writer-reproduction/out/faultline_prologue.exe --seed 12345 --save build/phase16-writer-reproduction/reproduced.flsave --verbose
```

Executed in addition against the first isolated `build/phase16-writer` copy:

```powershell
build/phase16-writer/out/faultline_savecheck.exe --inspect build/phase16-writer/authentic-v10.flsave
```

Both builds emit identical **1007684-byte** files, SHA-256
`86faa3bdbea5a9950590a0617f2f824c9cb5d765a52d910d0823c2d7dfa5fecd`.
Writer reports Save v10, generator 3, seed 12345, tick 56064000, 48 months, completed
prologue/story, Validation PASS and the exact verified v12 hash **2517a20c0ec2a76e**.
Fixture is `tests/fixtures/save_v10_phase16.flsave`; current v11 migration validation
and exact old-payload/default-player tests PASS. Under v13 its migrated StateHash is
`f7d8d0deb4fb889e` (hash schemas legitimately differ).

The inherited .flsave was valid v10 but undocumented/different checkpoint; preserved
under build before replacing explicitly. Its `.tmp` is actually v11 and remains
unchanged/unused. Full original hashes/provenance are in tests/fixtures/README.md.
The frozen historical source copies emit old aggregate-initializer warnings;
current source corrects those warnings without altering state or fixture bytes.

An authentic v7 completed-history fixture was generated by the unmodified historical
writer at **1d5c2e7**, isolated under `build/phase13-writer`, replacing the invalid
synthetic-new-save-truncation method. Exact commands/checksum are in fixture README.
All preexisting v1..v9 fixtures remain unchanged, including the separate v7 day-one
fixture. Binary `.gitattributes` policy remains intact.

## Commands and measured results

Focused iterations built and ran `faultline_player_tests`, the exact continuation
case, `faultline_persistence_tests`, `faultline_history_tests`, `faultline_story_tests`,
the player process gate, and then all fast CTest gates. Initial failures were
reproduced before correction; the final fresh run below is the completion authority.

```powershell
ctest --test-dir build-release -L fast --output-on-failure -j 4
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

Fast iteration: **18/18 PASS, 5.95 s** before the last two additional focused cases.
Final canonical build: **exit 0, 28/28 CTest PASS, 97.21 s**; total configure/build/test
stopwatch **111.2679781 s**. C++20 / GCC 16.2.0 / Ninja / Release. Four independent
test processes, each authoritative simulation single-threaded. No current project
compiler warning/error in the canonical log. Git diff --check passes; line-ending
conversion notices are Git notices, not compiler diagnostics.

### Doctest counts from the final CTest log

| Suite | Cases | Assertions |
| --- | ---: | ---: |
| foundation | 49 | 210838 |
| story | 12 | 6932 |
| history | 9 | 9254 |
| persistence | 17 | 5758 |
| prologue | 44 | 281 |
| territory | 27 | 3687 |
| player | 32 | 49531 |
| market fast | 15 | 1309 |
| NPC | 20 | 511 |
| mission | 7 | 58 |
| faction | 15 | 403 |
| market five-year stress | 1 | 262860 |
| **Full total** | **248** | **551422** |
| **Fast-only total** | **247** | **288562** |

No test removed; all 25 baseline CTest gates retained plus three player gates.
The 1000-seed worldgen/history, 250 standard-history, 100 story, 250 x 30-day and
25 x 365-day simulation batches, both multi-year market gates and all original
process/persistence/allocator/RNG/mission/prologue regressions PASS.
Full logs: `build-release/phase17-canonical.log`, `build-release/phase17-ctest.log`.

### Movement checkpoint and independent processes

```powershell
build-release/faultline_player.exe --seed 12345 --ticks 12000 --save build-release/phase17-player12345.flsave
ctest --test-dir build-release -R player_process_determinism --output-on-failure
```

CTest executes two independent moving-player processes, compares complete stdout
and save SHA-256 (not StoryGraph/prologue substitutes). Final output:

```text
Seed: 12345
Entity: 1
Cell: 10,21
Local: 27.65,0,26.5201
Yaw: -2.35619
Speed: 5
Stance height: 1.75
StateHash: 4c0523b02894026c
Worlds: 1
Players initialized: 1
Movement scripts: 1
Ticks per script: 12000
Cell crossings: 24
Invalid positions/duplicate players/lost components: 0/0/0
Save/continuation/hash mismatches: 0/0/0
BatchHash: fc3a74752c7c47f3
Validation: PASS
```

Save size **971355 bytes**. Independent process A, B and manual save all SHA-256:
`97f570983eeb07d19dbc0325838190e056a580160818700e7c90cc7aa036cad1`.
Manual shell+tool stopwatch 1.5893569 s under concurrent stress. CTest process gate
1.82 s for both runs. Printed coordinates are diagnostic-rounded; byte comparison
and StateHash compare actual persisted values. Process mismatch count: zero for
the explicitly tested seed-12345 pair, not a claim of 25 independent-process pairs.

### Multi-seed / long movement

```powershell
build-release/faultline_player.exe --range 1 25 --ticks 12000
```

Executed as `player_batch_25`: **25 worlds, 25 initialized players, 25 scripts,
300000 logical command ticks**; continuation replica adds 150000 actual tick calls.
**593 cell crossings**, zero invalid positions, duplicate players, lost bodies/
components, save mismatches, continuation mismatches or StateHash mismatches.
BatchHash **2e768b07d7cd5861**, Validation PASS, **20.05 s**, about **802 ms/world**
under concurrent CTest stress, including generation, save/load and replica checks.
Focused tests additionally execute the 16000-command signed/crossing script,
negative boundaries, cardinal/diagonal/analog motion, aims in all quadrants, collision
sliding, tunneling prevention, multiple-cell moves, one-tick intent, malformed state,
bounded/truncated Player payload rejection, duplicate-player rejection, exact
mid-movement continuation and a live hysteresis-ring save/load.

### Worldgen hashes (direct commands executed)

```powershell
build-release/faultline_worldgen.exe --seed 12345 --version 1 --validate
build-release/faultline_worldgen.exe --seed 12345 --version 2 --validate
build-release/faultline_worldgen.exe --seed 12345 --version 3 --validate
```

All Validation PASS, exactly:

| Generator | BaseWorldHash |
| --- | --- |
| 1 | 2938f0ca42162bc9 |
| 2 | 5f3a446fe0d2f974 |
| 3 | 888c5dbd10c15886 |

## Limitations and stop

Ground-plane conservative static collision only; radius constant, no vertical
physics, interiors/doors, character-to-character collisions or physical NPC AI.
The linear footprint scan is intentionally simple; measured player batches remain
cheap, and a future derived index can replace the scan without changing truth.
NPC materialization still uses the preexisting semantic-versus-physical activity
boundary. Logical residency is now preserved so save/load does not change it.
Interaction emits intent; target-specific actions/pickup/dialogue consumers are
not invented in this phase. Explicit session setup creates the body.

No tests were weakened merely to pass; historical payloads were not fabricated
from new encodings. No hardware keycodes in gameplay, renderer authority, Phase 18,
Character AI, combat, weapons, police, vehicles or HUD were added. Canonical parent
specifications were read only. No unresolved errors/blockers; stop for review.


## Final git status --short

Uncommitted Phase 16 and Phase 17 work intentionally retained:

```text
 M CMakeLists.txt
 M DEVELOPMENT_STATUS.md
 M README.md
 M docs/DETERMINISM.md
 M docs/FILE_INDEX.md
 M docs/STORY_GRAPH.md
 M src/persistence/save.cpp
 M src/persistence/save.hpp
 M src/simulation/runtime/game.cpp
 M src/simulation/runtime/game.hpp
 M src/simulation/runtime/game_state.hpp
 M src/world/state/actor_types.hpp
 M src/world/state/world_state.cpp
 M src/world/state/world_state.hpp
 M tests/fixtures/README.md
 M tests/history_tests.cpp
 M tests/market_simulation_tests.cpp
 M tests/npc_simulation_tests.cpp
 M tests/persistence_tests.cpp
 M tests/story_tests.cpp
 M tests/territory_simulation_tests.cpp
?? docs/FIRST_DELIVERY.md
?? docs/PHASE16_VERIFICATION.md
?? docs/PHASE17_VERIFICATION.md
?? docs/PLAYER_CONTROLLER.md
?? docs/SAVE_FORMAT_V10.md
?? docs/SAVE_FORMAT_V11.md
?? src/persistence/schema_v10.hpp
?? src/persistence/schema_v11.hpp
?? src/simulation/player/
?? src/simulation/prologue/
?? src/world/spatial/character_movement.cpp
?? src/world/spatial/character_movement.hpp
?? tests/determinism/player_process.cmake
?? tests/determinism/prologue_process.cmake
?? tests/fixtures/phase16_writer.patch
?? tests/fixtures/save_v10_phase16.flsave
?? tests/fixtures/save_v10_phase16.flsave.tmp
?? tests/fixtures/save_v7_completed_history.flsave
?? tests/fixtures/save_v9_phase15.flsave
?? tests/player_controller_tests.cpp
?? tests/prologue_tests.cpp
?? tests/save_compatibility.hpp
?? tools/player/
?? tools/prologue/
?? tools/recover_phase16_fixture.py
```
