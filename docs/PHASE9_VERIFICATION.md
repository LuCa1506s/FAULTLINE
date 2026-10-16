# Phase 9 / Headless Faction Simulation verification

Verified on 2026-10-16, Windows, GCC 16.2.0, Ninja, C++20 Debug. Canonical parent
documents were read and not modified. Phase 10 is not implemented.

## 1. Files created and modified

Created:

* `src/simulation/factions/faction_state.hpp`
* `src/simulation/factions/faction_system.hpp`
* `src/simulation/factions/faction_system.cpp`
* `src/persistence/schema_v2.hpp`
* `tests/faction_simulation_tests.cpp`
* `tests/legacy_save_v1.hpp`
* `tests/determinism/simulation_process.cmake`
* `tests/fixtures/create_v1_reference.ps1`
* `tests/fixtures/save_v1_reference.flsave` (730 bytes)
* `tests/fixtures/README.md`
* `docs/FACTION_SIMULATION_M1.md`
* `docs/SAVE_FORMAT_V2.md`
* this report.

Modified: `CMakeLists.txt`, `src/core/id/id.hpp`,
`src/world/state/actor_types.hpp`, `src/world/state/world_state.cpp`,
`src/simulation/runtime/game_state.hpp`, `game.hpp`, `game.cpp`,
`src/persistence/save.hpp`, `save.cpp`, `tests/persistence_tests.cpp`,
`tools/simulator/main.cpp`, `tools/savecheck/main.cpp`, `README.md`,
`DEVELOPMENT_STATUS.md`, `docs/DETERMINISM.md`, `docs/FILE_INDEX.md`.
No worldgen source or canonical parent document was changed. Verification logs,
the active-operation save and temporary build helpers stay inside `build/`.

## 2–3. Versioning and v1-save compatibility

SaveFormatVersion is **2**; GameVersion remains **0.1.0**, independently of the
save schema. GeneratorVersion remains **3**. V1 decoding and migration pass for
all three supported generator versions, plus an independent fixed historical-wire
fixture. No RNG is consumed, old resources/allocator marks are retained, generated
base hashes stay equal, no operations are invented and next OperationId is 1.
Default confidence is 5000; next evaluation is saved tick + 6400 (or the explicit
never sentinel at the time limit). V2 save/load/save bytes are identical.
See [SAVE_FORMAT_V2.md](SAVE_FORMAT_V2.md) for exact wire fields and fixture provenance.

## 4–11. Runtime model and algorithms

* Existing treasury, unallocated reserves, cohesion and territorial garrisons
  remain authoritative. Added strategic confidence, evaluation state/cooldown,
  one operation store and a deterministic scheduler. Manpower, commitments and
  operational capacity are derived, not duplicated in counters.
* Evaluation defaults to every four game hours (6400 ticks); preparation is two
  hours, active attack duration six hours, defense preparation one hour, attack
  cooldown twelve hours. All configurable, all integer, no wall-clock authority.
* Utility rewards strategic value, hostility, aggression, territorial focus,
  confidence/cohesion and relative strength; it charges cost/risk. Threatened home
  defense receives a strong preference. Invalid/unaffordable options are removed;
  stable IDs break ties. AI selection consumes no RNG.
* Operations progress Preparing → Active → Resolved/Failed, or cancel explicitly.
  Terminal states cannot restart or release resources twice.
* Attack targets must border existing controlled territory in the real semantic
  adjacency graph. Own/invalid/unreachable/duplicate live targets are rejected.
* Defense binds one actual rival attack on an owned territory. One force cannot
  defend two threats; safe territories do not create endless defense operations.
* Aggregate strength uses force, garrison, active defense, cohesion/confidence
  and a bounded 90–110% faction-RNG factor. Casualties are bounded aggregate-only
  losses; morale changes are clamped. Operation outcomes retain causal pressure
  evidence for the future TerritorySystem.
* Launch transfers members out of unallocated reserves into one commitment and
  pays mobilization. Survivors return on termination; spent money does not. Tests
  cover oversubscription, capacity, active/cancelled operations, exact release,
  total resource conservation against casualties and zero negative resources.

Exact model, initial tuning, source interpretation and current limits are in
[FACTION_SIMULATION_M1.md](FACTION_SIMULATION_M1.md).

## 12. Territory ownership boundary

**No ownership mutation:** the faction system only damages aggregate garrisons
and records operation outcomes. CLI/unit tests compare every owner before/after
simulation. An exhausted garrison leaves its old claim intact until Phase 11;
initial generated-state validation remains unchanged. No major NPC is killed,
no diplomacy values change, and no physical NPC entities/player are required.

## 13. Actual seed 12345 / 30-day output

Command: `./build/faultline_sim.exe --seed 12345 --days 30`.

```text
FAULTLINE HEADLESS SIM
Seed: 12345
Generator: 3
Simulated: 30 days
Tick: 1152000
Factions: 5
Faction #1 | Kuroda Syndicate
  Manpower: 76 | Reserve: 43 | Committed: 8 | Treasury: 131801
  Cohesion: 8481 | Confidence: 10000 | Capacity: 2 | Active Operations: 1 | Operations created: 60
Faction #2 | Eastside Brotherhood
  Manpower: 54 | Reserve: 24 | Committed: 14 | Treasury: 82402
  Cohesion: 6386 | Confidence: 4320 | Capacity: 2 | Active Operations: 2 | Operations created: 120
Faction #3 | Vitale Syndicate
  Manpower: 88 | Reserve: 52 | Committed: 16 | Treasury: 81903
  Cohesion: 10000 | Confidence: 10000 | Capacity: 2 | Active Operations: 2 | Operations created: 108
Faction #4 | Kuroda Consortium
  Manpower: 78 | Reserve: 20 | Committed: 14 | Treasury: 91604
  Cohesion: 8881 | Confidence: 6280 | Capacity: 2 | Active Operations: 2 | Operations created: 120
Faction #5 | Cumbre Cartel
  Manpower: 60 | Reserve: 30 | Committed: 8 | Treasury: 131705
  Cohesion: 4015 | Confidence: 0 | Capacity: 2 | Active Operations: 1 | Operations created: 60
Operations created: 468
Resolved (successful): 295
Failed: 165
Cancelled: 0
Active/preparing: 8
Attacks: 300
Defenses: 168
Manpower lost: 142
Treasury spent: 493200
Peak committed manpower: 60
Factions unable to mobilize (resources): 0
Operations/faction/30-days: 93.60
Validation: PASS
StateHash: 1129a254f905b856
```

## 14–15. 365-day result and final hashes

Command: `./build/faultline_sim.exe --seed 12345 --days 365`.
Tick 14,016,000, Validation PASS. 943 operations (650 attacks, 293 defenses),
650 successful, 293 failed, none cancelled/pending. All commitments released.
241 aggregate losses; 1,011,825 treasury consumed; peak commitments 60. Named
NPCs, relationships and territory owners remain unchanged. Four factions lack
resources for any minimum operation; the fifth can still afford a small defense,
but has no funded rival attack to respond to. No free revenue/recruitment is added.

| Run | Final StateHash |
|---|---|
| 30 days | `1129a254f905b856` |
| 365 days | `1a794fe51aff2621` |
| load day-30 save, continue 335 days | `1a794fe51aff2621` |

## 16–17. Determinism and save continuation

The CMake process test launches two distinct 30-day processes and compares complete
stdout, including operation counts, faction resources and final hash: PASS.
Different seed 12346 produces a different result/hash. Only the faction RNG stream
advances due to this system; the other six remain unchanged.

Unit tests save linked attack/defense operations while both are active, restore
their IDs/commitments/times/scheduler/RNG, resolve them, then compare StateHash,
save bytes and the next OperationId: PASS. Fixed-tick and coarse headless runs also
match hashes, bytes and event counts.

Actual file commands were executed as well:

```powershell
./build/faultline_sim.exe --seed 12345 --days 30 --save build/phase9-day30.flsave
./build/faultline_sim.exe --load build/phase9-day30.flsave --days 335
```

The saved month-end state has eight live operations and is 1,012,765 bytes. Its
continued final hash equals the uninterrupted 365-day run above. Save files and
CLI logs remain under build, not the source root or parent directory.

## 18–20. Batch and resource statistics

All selected seeds terminated, validated and demonstrated autonomous operations.
Final timing is recorded in DEVELOPMENT_STATUS and the full build log.

| Batch | Operations | Attacks | Defenses | Success | Failure | Pending at end |
|---|---:|---:|---:|---:|---:|---:|
| 1..250 × 30 days | 106232 | 73987 | 32245 | 72879 | 31896 | 1457 |
| 1..25 × 365 days | 21407 | 17472 | 3935 | 17472 | 3935 | 0 |

| Batch | Aggregate losses | Treasury spent | Maximum simultaneous commitment | Resource-exhausted factions |
|---|---:|---:|---:|---:|
| 1..250 × 30 days | 25999 | 113707425 | 69 | 131 of 1250 |
| 1..25 × 365 days | 3128 | 23580675 | 68 | 105 of 125 |

No cancelled operations arose autonomously in these batches; cancellation behavior
is tested explicitly. Pending month-end operations have valid future deadlines,
not leaked terminal commitments. Seed 12345 creates 93.60 operations/faction per
30-day month on average; individual counts are in the actual output above.

## 21–25. Build, tests and compatibility

Canonical command executed successfully:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`.
Project warnings are enabled; no unresolved compiler warnings/errors remain.
Full CTest includes ten tests: persistence, factions, simulation process
determinism, 365-day simulation, both simulation batches, existing foundation,
worldgen checkpoint, worldgen 1..1000, and worldgen process determinism.
Final full gate: **10/10 PASS, 249.83 s**. Fast-label total 16.13 s;
persistence-label total 3.94 s. No project warnings/errors in the final build log.

Doctest: 49 foundation cases / 210,838 assertions; 15 persistence cases / 3,419
assertions; 15 faction cases / 333 assertions. Total **79 cases / 214,590 assertions**
PASS. The original Phase 8 tests remain; the minimal-save truncation loop covers
the additional v2 bytes rather than dropping coverage. New tests include lifecycle,
resource/utility behavior, NPC/ownership/diplomacy preservation, active linked
operations, v1 migration, duplicate/invalid state and bounded cache reconstruction.
Labels retain fast/determinism/stress/persistence and add simulation/factions.

Worldgen seed 1..1000 regression: PASS. Exact seed 12345 hashes remain:

| Generator | BaseWorldHash |
|---|---|
| 1 | `2938f0ca42162bc9` |
| 2 | `5f3a446fe0d2f974` |
| 3 | `888c5dbd10c15886` |

Both independent-process comparison and the fixed regression vectors pass. No
generator increment or implicit generated-world identity migration occurred.

## 26–28. Performance, limits and next task

Headless advancement processes the same scheduled mutations as normal ticking,
without looping over all 1,152,000/14,016,000 idle fixed ticks for 30/365 days.
Standalone and batch wall-time measurements (including generation and validation)
are recorded in `build/phase9-cli-metrics.txt` and CTest logs. They are indicative
Debug samples, not a cross-machine benchmark. Timings never affect state/hash.

Final measurements: standalone 30-day CLI 447.730 ms; 365-day CLI 435.054 ms,
including process startup/generation/validation. The small inversion is measurement/
startup noise, not a claim that a longer simulation is intrinsically faster.
Simulation batch 1..250 x 30 days: 83.7945 s / 335.178 ms per seed;
1..25 x 365 days: 9.34377 s / 373.751 ms per seed. Worldgen regression 1..1000:
140.086 s. Logs: `build/phase9-build.log`, `build/Testing/Temporary/LastTest.log`,
`build/phase9-sim-30.log`, `phase9-sim-365.log`, `phase9-resume.log`.

Known limits: no revenue/recruitment, control reevaluation, runtime diplomacy,
collapse, hidden-information model, physical operation materialization or NPC
semantic simulation. Resources can therefore run down in long runs. The strategic
knowledge model uses public geometry/garrisons and stable sequential ID ordering.
Pressure is outcome evidence, never ownership authority. Save v2 retains v1
hardening limits and adds no checksum/atomic replacement. The unused temporary
legacy fixture executable was blocked by Application Control; independent wire
fixture/encoder coverage passed without security-policy changes.

Next roadmap task: **Phase 10 — NPC Semantic Simulation**, after review.
STOP at the completed Phase 9 checkpoint.
