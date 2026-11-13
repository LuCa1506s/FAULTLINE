# Phase 11 verification - TerritorySystem

Status: **completed and verified**. STOP before Phase 12 review.
Date: 2026-10-17. C++20 / GCC 16.2.0 / Ninja / Debug.
Canonical parent documents and unrelated repository files were not modified.

## Independent-review repair - local resistance (2026-10-17)

This section supersedes the historical measurements/schema statements below.
The review correctly identified a missing roadmap section-15 / GDD section-54
requirement. Read the phase contract, AGENTS.md, current status/report, canonical
spatial/domain/faction/save/roadmap specs and relevant source/tests. No canonical
conflict was found. No future-phase implementation or canonical edit was made.

Implemented one bounded local-resistance value per runtime territory. Actual
attack casualties raise opposition (5 units per casualty by default); surviving
successful defense relieves it (2 units per survivor). Linked defense records do
not count the same casualties twice. Cancelled operations and failed defenses
provide no relief. Opposing strength is max(rival presence, resistance), compared
against the ordinary gain/hold margins. Resistance can prevent acquisition or
turn control into contest without creating faction presence or manpower. Empty
regions stay uncontrolled. Initialization starts at zero and preserves base maps.
Sorted operation processing, target batching and the existing persisted consumed
ledger also govern resistance, including duplicate/reordered event delivery.

SaveFormatVersion **5**, runtime StateHash **v7**, GeneratorVersion **3**,
GameVersion **0.1.0**. The new rules and resistance are authoritative, persisted,
validated and hashed. V5 appends to section 13's unchanged v4 payload; sections
1..12 keep their exact wire encodings. Real v1/v2/v3 migrations remain tested.
A real pre-repair v4 day-30 fixture now verifies prior territory authority/bytes,
zero default resistance, unchanged RNG and deterministic pending-op continuation.
Fixture provenance/SHA-256: tests/fixtures/README.md. Encoding: SAVE_FORMAT_V5.md.

Five added cases verify resistance-only opposition boundaries, no empty-territory
claim, causal battle disruption including a failed attack with no rival foothold,
saturation, successful paid defense relief without double-counted casualties,
nonzero/custom-rule save continuation, real v4 migration, hashing and malformed
extensions. The failed-attack fixture initially assumed no garrison casualties;
its assertion exposed that incorrect assumption. The corrected case checks the
exact garrison loss and proves that loss alone retains control, while resistance
changes it to Contested. Existing assertions/coverage were retained.

Canonical gate actually executed:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`
**PASS, exit 0; 12/12 CTest entries in 327.12 s**, no project warnings/errors.
Log: build/phase11-repair-build.log; detailed suite/batch output retained in
build/phase11-repair-full-ctest-detail.log. Fast labels total 23.53 s; persistence
labels 13.47 s. No assertions were removed or weakened.

| Gate | Result | Seconds |
|---|---|---:|
| Persistence, faction, NPC, territory, foundation | PASS | 2.54 / 3.23 / 2.69 / 5.02 / 7.57 |
| Simulation process determinism | PASS | 1.17 |
| Seed 12345 / 365 days | PASS | 1.96 |
| Seeds 1..250 / 30 days | PASS | 128.88 |
| Seeds 1..25 / 365 days | PASS | 34.52 |
| Worldgen checkpoint / 1000 seeds / process determinism | PASS | 0.26 / 138.21 / 1.05 |

Doctest: **126 cases / 219571 assertions PASS**. Foundation 49/210838,
persistence 15/4156, faction 15/403, NPC 20/511, territory **27/3663**.
Persistence's additional assertions follow its existing exhaustive truncated-save
checks over the extended schema. Batch control changes: mean 33.404 (range
1..77) for 30 days; mean 80.44 (37..133) for a year. No fully static worlds.
Largest final shares 84.61% and 100%; dominance is reported without artificial
caps. Full batch metrics are in the retained detailed log.

After a comment-only header cleanup, activated the existing toolchain and ran
`cmake --build build` then
`ctest --test-dir build --output-on-failure -L fast`: **exit 0, 8/8 PASS in
23.63 s**, no warnings. Final rebuild/fast log:
build/phase11-repair-final-fast.log. The full stress run above used identical
production logic. Final `git diff --check` also passed.

Standalone commands actually executed, exit 0:

```powershell
./build/faultline_sim.exe --seed 12345 --days 30 --save build/phase11-repair-day30.flsave
./build/faultline_sim.exe --seed 12345 --days 365 --save build/phase11-repair-year.flsave
./build/faultline_sim.exe --load build/phase11-repair-day30.flsave --days 335 --save build/phase11-repair-resumed.flsave
./build/faultline_savecheck.exe --seed 12345
1..3 | ForEach-Object { ./build/faultline_worldgen.exe --seed 12345 --version $_ --validate }
Get-FileHash build/phase11-repair-year.flsave,build/phase11-repair-resumed.flsave -Algorithm SHA256
```

Day 30 validates: StateHash **2e0033b8ca5379b9**, 508 operations, 49 controller
changes, 15 contested transitions; control counts **17/0/18/5/0**, one contested.
18 territories have nonzero resistance (max 35, total 424); save 1024117 bytes.
Day 365 validates: StateHash **fa5c761eb581b41d**, 648 operations, 60 controller
changes, 20 contested transitions; control counts **21/0/19/1/0**, no contests.
24 territories have nonzero resistance (max 75, total 579); save 1039329 bytes.
These aggregate counts happen to match the pre-repair seed-12345 run; dedicated
behavior tests prove resistance changes control at its causal boundaries.

Day-30 + 335 days matches the uninterrupted year StateHash and every save byte
(Python byte comparison also executed). Both year-end SHA-256 values:
`12615f4f1c5312b5417b970f1d287b89f21f4dbb19b9359897fc3633cc0a14e0`.
Savecheck: canonical bytes PASS, initial save **969005 bytes**, before/after
StateHash **d942e2aaa4d51869**. Direct generator checks all validate and preserve
seed-12345 BaseWorldHashes: v1 **2938f0ca42162bc9**, v2 **5f3a446fe0d2f974**,
v3 **888c5dbd10c15886**. Logs/saves: build/phase11-repair-*.

Repair changes: src/simulation/territory/territory_state.hpp,
territory_system.hpp/.cpp; src/simulation/runtime/game.cpp;
src/persistence/save.hpp/.cpp; tests/territory_simulation_tests.cpp;
tests/fixtures/save_v4_phase11.flsave and README.md; docs/SAVE_FORMAT_V5.md,
TERRITORY_M1.md, DETERMINISM.md, FILE_INDEX.md, this report; repository README.md
and DEVELOPMENT_STATUS.md. Pre-existing revalidation changes were preserved.

Limit: resistance is an aggregate response to local territorial violence with
operation-driven relief; it has no passive decay or detailed civilian simulation.
No civilian gangs, economy, market or Phase 12 logic was added. Existing M1
resource-exhaustion, residual-garrison and ledger-growth limits remain.

## Historical revalidation session - 2026-10-17

The requested phase was already implemented at session start. Read AGENTS.md,
the phase contract, DEVELOPMENT_STATUS.md, all five named canonical specs,
save/version documentation, and the relevant runtime/faction/territory/save code
and tests. Preserved that implementation; no canonical conflict was found.
The numbered sections below retain the original implementation-session report.
This revalidation changes only tests/territory_simulation_tests.cpp,
DEVELOPMENT_STATUS.md, docs/FILE_INDEX.md and this report. Existing unrelated
automation edits and helper files were left untouched.

Two additional integration cases verify actual paid attack/defense outcomes:

* Reversing simultaneous result delivery and duplicating each result preserves
  StateHash, every save byte, and the one nonempty control-transition event.
  Replaying the same outcomes after save/load leaves state and events unchanged.
* All ten non-default territory rules survive repeated save/load during a
  resource-backed A-controlled -> contested -> B-controlled sequence. Future
  OperationIds, hashes and bytes match at every step. Rules affect StateHash.

The initial ordering-test setup kept control unchanged, so its required nonempty
event assertion failed. Changed the fixture to a weakened, still controlled
foothold that actually crosses a control boundary; retained the assertion.
The expanded territory suite then passed **22 cases / 3564 assertions**.

Fresh standalone CLI commands (all exited 0):

```powershell
./build/faultline_sim.exe --seed 12345 --days 30 --save build/phase11-recheck-day30.flsave
./build/faultline_sim.exe --seed 12345 --days 365 --save build/phase11-recheck-year.flsave
./build/faultline_sim.exe --load build/phase11-recheck-day30.flsave --days 335 --save build/phase11-recheck-resumed.flsave
./build/faultline_worldgen.exe --seed 12345 --version 1 --validate
./build/faultline_worldgen.exe --seed 12345 --version 2 --validate
./build/faultline_worldgen.exe --seed 12345 --version 3 --validate
./build/faultline_savecheck.exe --seed 12345
```

Day 30: 508 operations, 49 controller changes, 15 contested transitions,
StateHash c5c174fb61dc193. Day 365: 648 operations, 60 controller changes,
20 contested transitions, StateHash 48c3ef5c0b1eecea. Both validate. Uninterrupted
and resumed year-end saves have identical SHA-256
5a10f2a6333918e1e974bb8858cd8ba660dd0a3a41e85f3a47177dae84d3d92d.
CLI stdout is retained in build/phase11-recheck-day30.log, -year.log and
-resumed.log. Direct generator checks preserve all three historical hashes
listed in section 30. Savecheck passes canonical bytes and equal before/after
StateHash 91d411fb99846223; initial v4 save remains 968833 bytes.

No production behavior, schema, generator/version fixtures or Phase 12 logic
was changed by this revalidation.

Fresh canonical command:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`
exited **0**, configure/build successful, **12/12 CTest entries PASS** in
**340.75 s**. The existing targets initially required no rebuild. This run
includes foundation, persistence, faction, NPC, territory, both process
determinism gates, seed 12345 x 365 days, seeds 1..250 x 30 days (**129.21 s**),
seeds 1..25 x 365 days (**34.74 s**) and worldgen 1..1000 (**151.54 s**).
Fast labels total 23.35 s; persistence labels 12.21 s. The full-run log is
preserved at build/phase11-recheck-full-ctest.log.

The two added cases were compiled during the stress gates with
`cmake --build build --target faultline_territory_tests` (after activating
tools/activate_toolchain.ps1); compilation/link exited 0 without project
warnings. `./build/faultline_territory_tests.exe` passed the expanded 22-case
suite. Other suites were unchanged, giving **121 cases / 219440 assertions**
across the five doctest suites. The full canonical run tested the original
20-case territory executable before the additions; the expanded executable
was verified separately. No unchanged stress gate needed a second execution.
Final `ctest --test-dir build -R '^territory_tests$' --output-on-failure`
also passed the expanded suite, exit 0, 1/1 in 3.97 s.
`git diff --check` passes for all four files changed by this session.

## 1. Files created/modified

Created (9):

* src/simulation/territory/territory_state.hpp
* src/simulation/territory/territory_system.hpp
* src/simulation/territory/territory_system.cpp
* src/persistence/schema_v4.hpp
* tests/territory_simulation_tests.cpp
* tests/fixtures/save_v3_phase10.flsave
* docs/TERRITORY_M1.md
* docs/SAVE_FORMAT_V4.md
* docs/PHASE11_VERIFICATION.md

Modified (20):

* CMakeLists.txt
* README.md
* DEVELOPMENT_STATUS.md
* docs/FILE_INDEX.md
* docs/DETERMINISM.md
* src/world/state/actor_types.hpp
* src/world/state/world_state.cpp
* src/simulation/runtime/game_state.hpp
* src/simulation/runtime/game.hpp
* src/simulation/runtime/game.cpp
* src/simulation/factions/faction_system.hpp
* src/simulation/factions/faction_system.cpp
* src/persistence/save.hpp
* src/persistence/save.cpp
* tools/simulator/main.cpp
* tests/faction_simulation_tests.cpp
* tests/npc_simulation_tests.cpp
* tests/persistence_tests.cpp
* tests/determinism/simulation_process.cmake
* tests/fixtures/README.md

The inventory covers the whole phase, not only the final Git diff. At initial
inspection the project appeared untracked; an external baseline checkpoint
18f3e2f (2026-10-17 00:27:19) captured implementation work during the session.
This agent created no commits and preserved that checkpoint. Existing
README_FIRST.txt, install.ps1 and payload/ were left untouched. Generated logs,
measurement scripts and saves stay inside build/ and are not source inventory.

## 2. SaveFormatVersion

**4**, independently of GeneratorVersion **3** and GameVersion **0.1.0**.
StateHash schema is FAULTLINE.state.v6. No generator increment or worldgen change.

## 3. Migration v1/v2/v3

**PASS** actual frozen v1 reference, real Phase 9 v2 day-30 and real Phase 10 v3
day-30 saves. Generated-world v1 migration additionally covers generators 1..3.
All historical section payloads remain byte-identical after migration (excluding
new header/section count and appended sections). Existing RNG, clock, resources,
IDs, NPC runtime and active operations survive. Repeated migration gives identical
state/bytes; resumed simulation is deterministic. Historical Resolved/Failed
results are acknowledged, never replayed, so existing garrison losses are not
applied twice. Preparing/Active operations remain pending for future consumption.

## 4. Runtime territory model

GameState.territorySimulation owns a sparse ordered map per TerritoryId, rules,
consumption ledger and counters. Each territory records per-faction entrenchment,
actual garrison affiliation and an incumbent hysteresis latch. Geometry/districts
and immutable generated assignment stay in the existing generated/domain layers.
Territories are not entities or streaming cells.

## 5. Presence scale

Default integer strategic units 0..10000, with zero represented by absence.
Effective presence = clamped(entrenchment + own garrison members *20).
Entrenchment is causal local foothold/influence, not additional manpower.
Rules/bounds are configurable and persisted. No downstream RNG, passive decay,
market, dealer economics, civilian or police contribution.

## 6. Control authority

Presence inputs, rules and the explicit historical hysteresis latch determine
control. CONTROLLED has one valid controller; CONTESTED/UNCONTROLLED have NONE.
The control enum is not separately persisted. The control-cache map is derived
and rebuilt. Legacy TerritoryState.owner remains a validated wire/debug mirror,
not an independent authority; FactionSystem never reads it for strategy.
Persisting the minimal latch is intentional: temporal hysteresis needs history,
so presence alone cannot distinguish retention from reacquisition thresholds.

## 7. Garrison versus presence

residentMembers remains the one concrete troop count. garrisonFaction identifies
who owns those people even if control changes. Capturing a territory never
transfers soldiers or grants free manpower. Faction resource totals use this
membership, and a battle counts only its selected defender's garrison. Garrison
casualties reduce effective presence through the existing count exactly once.

## 8. Initialization from Phase 7

No regeneration. Import current generated/saved ownership, initialize 120
entrenchment (+100 on the owning faction's home), preserve actual garrison counts
and derive the identical initial control map. Initialization asserts map equality
for every runtime world, including all simulation-batch seeds. Dedicated tests
cover 50 micro-world seeds, canonical seed 12345 and supported generator versions.
Neutral regions start UNCONTROLLED; initial homes are robust but not immortal.

## 9. Control thresholds

Top presence <100 -> UNCONTROLLED. Otherwise a rival >=60 is meaningful: if the
largest-minus-second presence is below the applicable dominance margin, the area
is CONTESTED. Otherwise the top faction controls it. Stable FactionId ordering
handles equal inputs; equal meaningful rivals remain contested.

## 10. Hysteresis

Acquisition requires margin 80; retaining incumbent control requires 60. Loss
clears the incumbent. A lost 59-unit advantage cannot regain control merely by
returning to 60; it must reach 80. Exact threshold-1/threshold/threshold+1 tests
cover minimum presence, contest threshold and both margins. Save/load preserves
this history, and validation recomputes control/mirror/cache consistency.

## 11. Attack integration

Success adds floor(floor(strategicPressure * survivingForce / committedForce)/20)
to attacker entrenchment and removes it from defender entrenchment. Failed attack
only removes attacker foothold proportional to actual losses, without an arbitrary
defender bonus. A paid operation on neutral land may require repeated pressure.
One success is not a forced owner flip; repeated successful attacks are tested
through CONTROLLED A -> CONTESTED -> CONTROLLED B.

## 12. Defense integration

Survivors contribute 5 entrenchment units each, removing that much attacker
foothold on success and half on failure. This is grounded in paid surviving
reinforcement. Linked attack/defense effects are applied together before one
reevaluation, avoiding false intermediate capture/contest events. Strong defense
can preserve control or recover a contested region; tests cover both.

## 13. Events and scheduling

One scoped TerritoryControlEvent per real state/controller transition contains
TerritoryId, old/new state/controller and tick. Identical reevaluation or result
replay produces no duplicate. Operation effects use stable OperationId order,
then changed targets use TerritoryId order. Consumption is persistent/idempotent.
Pipeline: resolve operations -> consume territory effects -> cancel stale pending
operations/release commitments -> only scheduled faction evaluation -> NPC update
-> structural commit. No recursive strategic evaluation from control events.

## 14. FactionSystem never writes ownership

**PASS** dedicated staged-resolution test proves FactionSystem alone does not
change controller/foothold; only subsequent TerritorySystem application does.
Source search finds no .owner access in faction_system.cpp. Strategy, adjacency,
owned-target rejection, primary defender and defense utility use current runtime
control/presence. A third-party regression ensures a residual foreign garrison
cannot hide the new controller. Old connected geometry is unchanged; runtime
enclaves are permitted. No collapse/deletion or zero-territory teleport expansion.

## 15. Seed 12345 / 30 days

**PASS** Validation; tick 1152000; StateHash **c5c174fb61dc193**.
Actual CLI output:

```text
FAULTLINE HEADLESS SIM

Seed: 12345

Generator: 3

Simulated: 30 days

Tick: 1152000

Factions: 5

Faction #1 | Kuroda Syndicate

  Manpower: 58 | Reserve: 25 | Committed: 8 | Treasury: 82601

  Cohesion: 9101 | Confidence: 10000 | Capacity: 2 | Active Operations: 1 | Controlled territories: 17 | Operations created: 115

Faction #2 | Eastside Brotherhood

  Manpower: 42 | Reserve: 34 | Committed: 0 | Treasury: 128377

  Cohesion: 5096 | Confidence: 0 | Capacity: 2 | Active Operations: 0 | Controlled territories: 0 | Operations created: 73

Faction #3 | Vitale Syndicate

  Manpower: 66 | Reserve: 33 | Committed: 16 | Treasury: 57903

  Cohesion: 9970 | Confidence: 9840 | Capacity: 2 | Active Operations: 2 | Controlled territories: 18 | Operations created: 135

Faction #4 | Kuroda Consortium

  Manpower: 36 | Reserve: 1 | Committed: 7 | Treasury: 104054

  Cohesion: 6301 | Confidence: 0 | Capacity: 2 | Active Operations: 1 | Controlled territories: 5 | Operations created: 106

Faction #5 | Cumbre Cartel

  Manpower: 46 | Reserve: 37 | Committed: 0 | Treasury: 117905

  Cohesion: 3555 | Confidence: 0 | Capacity: 2 | Active Operations: 0 | Controlled territories: 0 | Operations created: 79

Current contested: 1

Current uncontrolled: 0

Control changes: 49

Contested transitions: 15

Uncontrolled transitions: 9

Territory transitions: 50

Factions reaching zero controlled territories: 2

Largest final faction share (basis points): 4390

NPC semantic updates: 31658

Travels started: 2070

Travels completed: 2070

Travels cancelled: 0

NPC operation participations: 1749

Relationship changes: 1736

Promotions: 0

Operations created: 508

Resolved (successful): 258

Failed: 245

Cancelled: 1

Active/preparing: 4

Attacks: 261

Defenses: 247

Manpower lost: 250

Treasury spent: 521775

Peak committed manpower: 54

Factions unable to mobilize (resources): 1

Operations/faction/30-days: 101.60

Validation: PASS

StateHash: c5c174fb61dc193
```

## 16. Final distribution / 30 days

FactionIds 1..5 control **17 / 0 / 18 / 5 / 0** territories; **1 CONTESTED**,
**0 UNCONTROLLED**, total 41. Counts are derived from CONTROLLED/controller.

## 17. Control/contest statistics / 30 days

49 controller changes (including transitions to/from NONE), 15 contested
transitions, nine uncontrolled transitions, 50 total logical state/controller
transitions. Two factions reached zero controlled territories but remain present.
Largest final share is 43.90% by territory count, not polygon area.

## 18. Operation count after integration

**508 operations**, compared with historical 468: +40. Breakdown: 261 attacks,
247 defenses, 258 successful, 245 failed, one cancelled and four pending.
Costs, force bounds, morale rules and four-hour evaluation interval were not
rebalanced. Current changing control, contested defense opportunities, target
availability, selected defenders and stale-target cancellations naturally change
operation selection/RNG consumption. No artificial 468-operation constraint.

## 19. Seed 12345 / 365 days

**PASS** Validation; tick 14016000; StateHash **48c3ef5c0b1eecea**.
648 stored operations, no pending operations or commitments; all 22 named NPCs
remain ALIVE (also inspected directly in the actual year-end save, unique IDs,
next NpcId 23). No overdue journeys, negative resources or invalid control.
Actual output:

```text
FAULTLINE HEADLESS SIM

Seed: 12345

Generator: 3

Simulated: 365 days

Tick: 14016000

Factions: 5

Faction #1 | Kuroda Syndicate

  Manpower: 32 | Reserve: 7 | Committed: 0 | Treasury: 251

  Cohesion: 8661 | Confidence: 7000 | Capacity: 0 | Active Operations: 0 | Controlled territories: 21 | Operations created: 191

Faction #2 | Eastside Brotherhood

  Manpower: 42 | Reserve: 34 | Committed: 0 | Treasury: 128377

  Cohesion: 5096 | Confidence: 0 | Capacity: 2 | Active Operations: 0 | Controlled territories: 0 | Operations created: 73

Faction #3 | Vitale Syndicate

  Manpower: 55 | Reserve: 38 | Committed: 0 | Treasury: 3

  Cohesion: 9610 | Confidence: 7920 | Capacity: 0 | Active Operations: 0 | Controlled territories: 19 | Operations created: 190

Faction #4 | Kuroda Consortium

  Manpower: 22 | Reserve: 3 | Committed: 0 | Treasury: 97079

  Cohesion: 5821 | Confidence: 0 | Capacity: 0 | Active Operations: 0 | Controlled territories: 1 | Operations created: 114

Faction #5 | Cumbre Cartel

  Manpower: 45 | Reserve: 36 | Committed: 0 | Treasury: 117005

  Cohesion: 3525 | Confidence: 0 | Capacity: 2 | Active Operations: 0 | Controlled territories: 0 | Operations created: 80

Current contested: 0

Current uncontrolled: 0

Control changes: 60

Contested transitions: 20

Uncontrolled transitions: 9

Territory transitions: 61

Factions reaching zero controlled territories: 2

Largest final faction share (basis points): 5121

NPC semantic updates: 385418

Travels started: 44189

Travels completed: 44189

Travels cancelled: 0

NPC operation participations: 2344

Relationship changes: 2240

Promotions: 0

Operations created: 648

Resolved (successful): 340

Failed: 307

Cancelled: 1

Active/preparing: 0

Attacks: 341

Defenses: 307

Manpower lost: 302

Treasury spent: 669900

Peak committed manpower: 54

Factions unable to mobilize (resources): 3

Operations/faction/30-days: 10.65

Validation: PASS

StateHash: 48c3ef5c0b1eecea
```

## 20. Final distribution / 365 days

FactionIds 1..5 control **21 / 0 / 19 / 1 / 0**; no contested/uncontrolled regions.
The largest faction controls 51.21% by count. Two factions have zero control,
retain money/NPCs/home identity, and are not collapsed. Sixty controller changes,
20 contested and nine uncontrolled transitions occurred; 61 total transitions.

## 21. Batch results

| Batch | Result | Tool seconds | ms/seed |
|---|---|---:|---:|
| 1..250 x 30 days | PASS | 124.988 | 499.953 |
| 1..25 x 365 days | PASS | 32.912 | 1316.48 |
| Worldgen 1..1000 | PASS | 137.624 | 137.624 |

All terminate and validate. Initial runtime map preservation is checked before
simulation; runtime control/garrisons, operations, resources, NPCs and references
validate after the run. Ongoing journeys at cutoffs have valid future deadlines.

## 22. Territorial batch statistics

| Statistic | 250 x 30 days | 25 x 365 days |
|---|---:|---:|
| Controller changes | 8351 | 2011 |
| Average / world over tested duration | 33.404 | 80.44 |
| Average / world normalized to 30 days | 33.404 | 6.61151 |
| Min/max controller changes | 1..77 | 37..133 |
| Contested transitions | 3015 | 788 |
| Average contested transitions / world | 12.06 | 31.52 |
| Uncontrolled transitions | 831 | 221 |
| Total logical transitions | 8516 | 2076 |
| Factions ever reaching zero control | 147 | 62 |
| Largest final faction share by count | 84.61% | 100% |
| Fully static worlds | 0 | 0 |
| Operations | 121598 | 19229 |
| Cancelled operations | 305 | 38 |

**Observed dominance:** an annual batch world reaches 100% single-faction control.
This is allowed and reported, not suppressed by ownership caps or rubber-banding.
Other factions are not deleted. No world is completely static; the seed-12345
checkpoint does not show immediate total conquest. Final economic/strategic balance
is deliberately incomplete without recruitment, revenue or recovery systems.

## 23. Save-mid-contest continuation

**PASS** save a real contested fixture, load, continue paid repeated operations
through acquisition; future IDs, control/presence, StateHash and canonical bytes
match. Incumbent hysteresis remains cleared while contested until acquisition.

## 24. Save after control change

**PASS** controller, exact per-faction presence, garrison membership, latch and
consumption history round-trip. Replayed result events cannot apply pressure
twice. Current-saved control caches are rebuilt and validated, not silently fixed.

## 25. Cross-process and long continuation

**PASS** CTest independent 30-day process comparison and additional manual pair
compare full output exactly. Actual CLI day-30 save + 335 more days has the same
StateHash, territory map and every serialized byte as uninterrupted 365 days.
Shared year-end save SHA-256:

`5a10f2a6333918e1e974bb8858cd8ba660dd0a3a41e85f3a47177dae84d3d92d`

## 26. Build

Actually executed final canonical command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
```

**PASS**, exit 0; configure/build/link and test gate successful. Project warnings
are enabled, with zero unresolved warnings/errors. Earlier missing-field test
initializer warnings were fixed. A malformed-value test initially changed a byte
to a still-valid value; it was corrected to exercise an out-of-range u32. The
residual-garrison defender bug found in review has a dedicated regression.

## 27. CTest

**12/12 PASS**, **319.46 seconds**. Fast-labeled portion **22.05 s**; persistence
**11.86 s**. No coverage removed. New territory label joins existing labels.
Preliminary full gate passed before the final defender correction; the complete
current gate was rerun and its results above are authoritative.
Logs: build/phase11-build.log and build/Testing/Temporary/LastTest.log.

## 28. Doctest

| Suite | Cases | Assertions | Result |
|---|---:|---:|---|
| Foundation | 49 | 210838 | PASS |
| Persistence | 15 | 4124 | PASS |
| Factions | 15 | 403 | PASS |
| NPC semantics | 20 | 511 | PASS |
| Territory | 20 | 3522 | PASS |
| Total | 119 | 219398 | PASS |

Historical no-control-change assumptions were adapted to the new stage while
retaining resource/state-machine/NPC coverage and adding explicit authority,
garrison conservation, thresholds, hysteresis and control-change assertions.

## 29. Persistence regression

**PASS** old v1/v2/v3 migrations, active operations and NPC participation/travel,
canonical bytes, StateHash round-trip, seven RNG futures, allocators, clock,
entity abstraction, promotion, malformed headers/versions/lengths/UTF-8/floats,
truncated data, references and collection budgets. Territorial malformed tests
cover unknown/duplicate IDs, zero/out-of-range presence, invalid latch/config,
legacy mirror divergence, bad consumption entries and derived-cache rebuild.

## 30. BaseWorldHash

Direct CLI, regression vectors and separate processes verify seed 12345:

* v1 2938f0ca42162bc9
* v2 5f3a446fe0d2f974
* v3 888c5dbd10c15886

V3 geometry remains 8000 x 8000 m, one water region, eight macro-zones, 210 road
nodes, 391 road edges, 18 districts, 8007 parcels/buildings, 41 territories, five
main factions and 22 major NPCs. GeneratorVersion remains 3.

## 31. Performance and save size

Standalone Debug samples, including process startup, generation and validation;
file I/O is included where stated. These are indicative tool-level measurements,
not isolated rule microbenchmarks:

| Invocation | ms |
|---|---:|
| 30-day CLI including save | 761.526 |
| 30-day independent process | 563.749 |
| 365-day CLI including save | 2033.393 |
| 335-day resumed CLI including load/save | 1748.255 |
| savecheck CLI | 490.900 |

Initial v4 save: 968833 bytes; memory encode/validation 108.927 ms; memory
load/validation/index rebuild 96.803 ms. Before/after hash 91d411fb99846223,
canonical bytes PASS. Day-30 save 1023945 bytes; year-end save 1039157 bytes.
Timings never enter authoritative state. Actual scripts/logs and save files are
under build/: phase11_measure.ps1, phase11-cli-verification.log,
phase11-metrics.json, phase11-sim-30.log, phase11-sim-30-second.log,
phase11-sim-365.log, phase11-resume.log, phase11-savecheck.log and
phase11-worldgen-v1.log through v3.log. The ordinary CLI has no timing in stdout.

## 32. Known limits

Single primary defender per aggregate battle; surviving foreign garrisons retain
membership but have no automatic retreat/occupation model. Entrenchment is a
persistent abstract foothold with causal changes, no generic decay. Zero-control
factions may defend meaningful local presence but cannot launch new attacks
without a currently controlled adjacent source; explicit base/recovery behavior
is deferred. No income/recruitment/collapse/diplomacy/market or physical gameplay
was added. Ledger grows alongside the existing operation history; future history
compaction is outside this phase. NPC influence is not added to territory, and
dealer role grants no market power. No special home-loss penalty beyond ordinary
operation morale. Dominance and eventual strategic inactivity are observable,
not masked by artificial balance caps. No unresolved errors, warnings or blockers.

## 33. Next task

**Phase 12 - District Market M1**, only after review: district demand/supply,
dealer capacity/market shares, bounded price and faction income. This session
stops after Phase 11 and does not implement Phase 12.
