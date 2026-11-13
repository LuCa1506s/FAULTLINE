# Phase 14 verification

Phase 14 complete; stop before Phase 15. Parent canonical specifications were read
only. No worldgen or Phase 13 history algorithm was modified. GeneratorVersion 3,
HistoryVersion 1 and GameVersion 0.1.0 remain unchanged. SaveFormatVersion is 8;
StateHash is FAULTLINE.state.v10; StoryGraphVersion is 1.

## Existing work reconciliation

At entry, git reported only untracked src/simulation/story/ (three files) and
tests/fixtures/save_v7_phase13.flsave. All were inspected and preserved. The partial
model/API, semantic node families, backwards prerequisites, live eligibility and
contact-rebinding approach were retained. Missing integration included the typed
ID, GameState field, build target source, validation, hashing, persistence, tool,
tests and documentation. Corrections include safe initialization, complete
dependency mismatch checking, important-dealer preference, actual successor
observation instead of a contradictory leadership vacuum, personal relationship
eligibility, current-controlled territory selection and compressed-history context.

The v7 fixture passed the original pre-edit Phase 13 savecheck: generator 3,
seed 12345, tick 38400, StateHash ab44633b2677f8ec. It is a real day-one v7 save,
not a completed-history save. Size 971960 bytes; its unchanged SHA256 is documented
in tests/fixtures/README.md. The exact original creation command is unavailable.

## Canonical build

Executed from repository root:

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
```

Output captured in build/phase14-canonical.log; complete test output preserved in
build/phase14-ctest.log. GCC 16.2.0 / Ninja / C++20 Debug. Exit 0; zero project
compiler warnings/errors. CTest 22/22 PASS, 622.60 s elapsed. Stopwatch including
configuration/build: 633.16 s. Four independent test processes ran concurrently;
authoritative simulation within each remains single-threaded. No test was removed
or weakened. A preliminary fast iteration also passed 13/13 in 59.73 s; the final
full run below includes all fast tests with the final source.

| CTest gate | Result | Seconds |
|---|---|---:|
| story_tests | PASS | 15.24 |
| story_standard_100 | PASS | 170.64 |
| story_process_determinism | PASS | 6.35 |
| history_tests | PASS | 23.89 |
| history_mass_1000 | PASS | 622.59 |
| history_standard_250 | PASS | 491.30 |
| history_process_determinism | PASS | 4.53 |
| persistence_tests | PASS | 3.93 |
| faction_tests | PASS | 5.43 |
| npc_tests | PASS | 4.52 |
| market_tests | PASS | 7.91 |
| market_multi_year | PASS | 174.62 |
| market_standard_multi_year | PASS | 92.50 |
| territory_tests | PASS | 8.86 |
| simulation_process_determinism | PASS | 2.28 |
| simulation_long_run | PASS | 3.99 |
| simulation_batch_250 | PASS | 218.39 |
| simulation_year_batch_25 | PASS | 71.28 |
| foundation_tests | PASS | 11.51 |
| worldgen_checkpoint | PASS | 0.47 |
| worldgen_mass_1000 | PASS | 227.90 |
| worldgen_process_determinism | PASS | 1.95 |

Doctest executions (the market fast/stress filters together retain the entire suite):

| Suite | Cases | Assertions |
|---|---:|---:|
| story | 12 | 6689 |
| history | 9 | 9015 |
| persistence | 15 | 4772 |
| factions | 15 | 403 |
| NPC | 20 | 511 |
| market fast | 15 | 1070 |
| market five-year | 1 | 262860 |
| territory | 27 | 3663 |
| foundation | 49 | 210838 |
| Total | 163 | 499821 |

## Story tests and mutation evidence

The new suite covers same-world generation/bytes/hash; all unrelated RNG,
allocators and world bytes unchanged; actual semantic/history references;
invalid source/prehistory and reinitialization rejection; success/failure branches;
duplicate semantic nodes, self/forward/cyclic/duplicate prerequisites, bad IDs,
enum/version/history context; important-dealer death substitution, then complete
dealer loss/invalidation; externally decided real leadership replacement; live
territory/political/treasury eligibility; personal RelationshipSystem events;
progressed graph save plus 30-day deterministic continuation; actual frozen v7
migration; completed-history v7 wire-prefix migration; malformed allocation marks,
story fields and an exhaustive story-section truncation sweep.

Death example: kill the bound important dealer through setNpcLifecycle, refresh
markets, repair. The graph binds an existing live registered dealer; original
death and NpcId allocator are unchanged. Kill all remaining same-faction dealers:
the contact becomes NONE and availability Invalidated. No free actor/resources,
resurrection, stale physical identity or dangling record is introduced.

World example: a hostile territorial possibility loses eligibility when the
actual TerritorySystem reevaluation removes its counterpart's control; changing
directed hostility also invalidates it. A separate personal relationship fixture
uses 60 actual Threatened events to reach hostility 6000 and make a suitably
ambitious non-leader betrayal possibility eligible. Story never edits those
foreign values itself. Organizational succession remains external; story merely
observes a real new leader after the original leader's death.

## Seed 12345 checkpoint and determinism

Executed:

```powershell
build/faultline_story.exe --seed 12345 --fixture-prologue --save build/phase14-seed12345.flsave
build/faultline_sim.exe --load build/phase14-seed12345.flsave --days 30 --save build/phase14-runtime30.flsave
```

First command uses explicit synthetic prologue evidence, followed by actual
48-month history; it does not implement FIRST DELIVERY. Exact story output:

```text
Seed: 12345
StoryGraphVersion: 1
Nodes: 70
Eligible: 5
StateHash: 7c4539b8c6002849
Worlds: 1
Nodes total/min/max: 70/70/70
Dependencies: 60
Eligible total: 5
No viable possibility: 0
Referenced major NPCs total: 13
Faction coverage total: 5
Death contact repairs: 1
Invalidated after mutations: 38
BatchHash: 83087b5df5e1e0a0
Validation: PASS
```

Saved size: 1006078 bytes. Two independently executed CTest story processes and
the above manual command produce identical saves, SHA256:
df1574a61f4005a182a7bf8dec87e7fbca4993a8076dcae9df65a0c947376f08.
The process gate compares complete stdout and saved bytes, not just counts.
The graph-present ordinary runtime continuation runs 30 days, 532 operations,
Validation PASS, final StateHash c28e8e7a87a36b78. Current-format roundtrips and
mid-story resolved-branch continuation compare both StateHash and complete bytes.

Real v1..v7 fixture migrations all PASS in the retained suites. For v7 the complete
fifteen-section payload prefix is byte-identical after migration; graph defaults
to uninitialized, with no manufactured prologue or story resolution. The separate
completed-history v7 prefix projection is explicitly synthetic; it is not passed
off as another historical fixture. Current story payload corruption rejects with
typed SaveError; no load-time silent repair is performed.

## Multi-seed statistics

CTest executed exactly:

```powershell
build/faultline_story.exe --range 1 100 --fixture-prologue
```

All 100 standard 8000 m worlds run full four-year history, generate twice and
compare canonical bytes, load/save/hash roundtrip and validate an important-dealer
death mutation. Zero invalid graphs and zero worlds lacking an eligible future
possibility. Totals: 7000 nodes (min/max 70/70), 6000 dependencies, 500 immediately
eligible opportunities, 1307 per-world unique major-NPC references summed,
500 faction coverage entries (all five per world), 133 changed contact bindings.
3836 Invalidated opportunities after mutation counts *all* world-ineligible or
incompatible possibilities, including ones already ineligible, not 3836 newly
destroyed opportunities. BatchHash bcd13563b647bd4c. Elapsed 170.64 s, average
1706.4 ms/world under concurrent full-suite load; includes full history, repeated
serialization/validation, replay projection and mutation, not just graph generation.

The standalone full history/story checkpoint took 3.732 s while other stress
processes were active. No isolated microbenchmark or claim of per-node timing is
made. Graph passes occur at coarse semantic changes, never every idle 20 Hz tick.
Fixed topology counts are intentional broad templates; contacts, regions,
history provenance, world predicates and later outcomes provide procedural variety.

## Preserved world identity and scope

Each command below was also executed directly, not inferred from stored fixtures:

```powershell
build/faultline_worldgen.exe --seed 12345 --version 1 --validate
build/faultline_worldgen.exe --seed 12345 --version 2 --validate
build/faultline_worldgen.exe --seed 12345 --version 3 --validate
```

All Validation PASS, hashes exactly 2938f0ca42162bc9 / 5f3a446fe0d2f974 /
888c5dbd10c15886. Full worldgen 1..1000, four-year 1000-small/250-standard histories
with replay, both five-year market suites, playerless 250 x 30 and 25 x 365 days,
NPC/faction/territory persistence, RNG/ID continuation and old malformed-save gates
remain PASS. Changed StateHash values reflect the explicit v10 runtime schema,
not changed generated identity or history algorithms.

Limits: explicit initialization after history, fixed coarse possibility families,
reversible live invalidation, caller repair hook for out-of-pipeline mutations,
existing-actor substitution without organizational succession AI. No MissionSystem,
objective execution, rewards, dialogue, runtime PRIMARY 001 or Phase 15 was started.
See STORY_GRAPH.md for actual algorithms/authority and SAVE_FORMAT_V8.md for wire
details; FILE_INDEX.md lists all changed/new files. Known blockers: none.
