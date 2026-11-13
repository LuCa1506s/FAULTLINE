# Phase 12 verification / Market M1

Date: 2026-10-17. Repository: FAULTLINE_GAME_V1. Phase scope: abstract district
markets and the first faction revenue loop. COMPLETED: all Definition-of-Done
items verified. Canonical tools/build.ps1 exited 0; all 15 CTest gates passed
in 518.99 seconds. Stop after Phase 12; no Phase 13 work was implemented.

## Sources and implementation

Read AGENTS.md, DEVELOPMENT_STATUS.md, automation/phases/phase-12.md, the canonical
DRUG_MARKET_SIMULATION_SPEC, FACTION_SIMULATION_SPEC,
WORLD_SPATIAL_STREAMING_SPEC, GAME_STATE_SAVE_FORMAT_SPEC and V1_IMPLEMENTATION_ROADMAP,
current save/version documentation and relevant runtime, semantic actors, faction,
territory, persistence and regression sources. No canonical conflict found. No
canonical parent file changed. Market balance formulas are implementation choices
explicitly permitted by the market spec. Initial city-wide spread/history stays
in Phase 13. No subagents were used.

Implementation algorithms, authority, income formula and limits: MARKET_M1.md.
Wire schema, validation and migration: SAVE_FORMAT_V6.md. Complete changed-file
inventory: FILE_INDEX.md, Phase 12 section. Versions: GeneratorVersion 3,
GameVersion 0.1.0 unchanged; SaveFormatVersion 6; StateHash schema v8.

## Commands executed

From the repository root using the local toolchain:

```powershell
. ./tools/activate_toolchain.ps1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build -L fast --output-on-failure
ctest --test-dir build -R '^market_tests$' --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
build/faultline_sim.exe --seed 12345 --days 30 --save build/phase12-day30.flsave
build/faultline_sim.exe --load build/phase12-day30.flsave --days 335 --save build/phase12-resumed.flsave
build/faultline_sim.exe --seed 12345 --days 365 --save build/phase12-year.flsave
Get-FileHash build/phase12-resumed.flsave,build/phase12-year.flsave -Algorithm SHA256
build/faultline_savecheck.exe --seed 12345
git diff --check
```

The canonical command output is retained in build/phase12-build.log; CLI outputs
are build/phase12-day30.log, phase12-resumed.log, phase12-year.log and
phase12-savecheck.log. Earlier iteration exposed synthetic fixture initialization,
expected current save version, a CTest filter quoting error and two new fixture
expectations (garrison presence and real named membership); all were corrected.
Appending section 14 also required historical resistance corruption tests to locate
section 13's end explicitly, retaining the actual resistance assertions. The old
income-free treasury inequality is replaced by exact money conservation including
market income and real paid costs. No behavior assertion or required gate was
removed. An obsolete accidentally unfiltered test process was stopped; attempts
to relink its in-use Windows executables failed until the old run exited. These
were local iteration errors, not final build results or external blockers.

## Focused coverage

Market tests exercise hourly deadlines; fixed vs coarse byte/hash equivalence;
RNG isolation; repeat-call no-op; rival share under foreign physical control;
all restrictive dealer lifecycle states; body health vs abstract-save behavior;
enabled/role/faction loss; contested and changed territory access; bounded decaying
disruption; zero demand/network destruction; custom scheduler/price/recovery rules;
fractional income continuation; duplicate/invalid registration; price damping;
treasury saturation; malformed numeric/reference/accounting values; truncated
section-14 prefixes; real frozen v5 migration and every old section byte;
all-five-faction income and exact spending accounting; maximum district supply
and small-demand apportionment; clock exhaustion and late-deadline rejection.

Five-year daily-validation tests run five small worlds with disruption and a
mid-run reload. A separate CLI gate runs ten standard worlds for five years.
All prior fast, deterministic, persistence, 250-seed month, 25-seed year,
1000-seed worldgen and independent process gates remain in the canonical build.

## Final canonical build

Command: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`.
Exit 0. All 15 CTest gates PASS, no failed tests, 518.99 seconds. Fast-label
portion 28.72 seconds; persistence 18.67 seconds. Debug GCC 16.2.0 / C++20,
CMake 4.4.4 / Ninja 1.13.2. Changed sources compiled without project warnings.
The only changes after this successful run are final documentation results.

| Gate | Seconds | Result |
| --- | ---: | --- |
| persistence_tests | 2.57 | PASS |
| faction_tests | 3.49 | PASS |
| npc_tests | 2.76 | PASS |
| market_tests | 4.94 | PASS |
| market_multi_year | 114.81 | PASS |
| market_standard_multi_year | 59.52 | PASS |
| territory_tests | 4.91 | PASS |
| simulation_process_determinism | 1.20 | PASS |
| simulation_long_run | 2.41 | PASS |
| simulation_batch_250 | 132.19 | PASS |
| simulation_year_batch_25 | 42.68 | PASS |
| foundation_tests | 7.55 | PASS |
| worldgen_checkpoint | 0.25 | PASS |
| worldgen_mass_1000 | 138.64 | PASS |
| worldgen_process_determinism | 1.04 | PASS |

Worldgen seeds 1..1000 all validate. Historical seed-12345 vectors remain exactly
v1 2938f0ca42162bc9, v2 5f3a446fe0d2f974, v3 888c5dbd10c15886; foundation
and independent processes verify all supported versions. No worldgen source,
GeneratorVersion or old fixture changed. Historical save v1..v5 migration gates
pass, including preservation of all old payload bytes and future continuations.
No unresolved canonical conflict or external/environment blocker remains.

## Verified multi-year gates

The corrected, actually selected small-world gate passed in 114.81 seconds:
one stress case, 262860 assertions, five seeds x 1825 days, daily validation,
causal disruption and mid-run reload. The other fifteen market cases passed
964 assertions in 4.94 seconds. These are disjoint CTest filters, so every one
of the sixteen market cases was executed.

The standard-world gate passed in 59.52 seconds: seeds 1..10 x 1825 days,
437990 scheduled market updates, 62648762 credited income, final total abstract
demand 137009 and supply 29393; control changes per seed 59..137, zero static
worlds, Validation PASS. These test bounds/stability rather than imposing an
artificial equilibrium or guaranteeing manpower recovery.

## Actual CLI continuation

All following commands exited 0. Seed 12345 / 30 days: tick 1152000,
719 market updates, 97094 market income, abstract demand 14341 / supply 3284;
508 operations, 49 controller changes, one currently contested territory.
StateHash dad9074a462b8212. Seed 12345 / 365 days: tick 14016000,
8759 market updates, 1160741 market income, demand 14356 / supply 2889;
883 operations, 141 controller changes, no pending operations, one faction
physically controlling all 41 territories. All five retain real dealers and income;
territory dominance is measured, not countered with fake market ownership.
StateHash 86fa867542f4778f. Actual operation spending is 521775 at day 30 and
918525 at year end. Named NPCs retain their identities/lifecycle.

The actual resumed and uninterrupted year-end saves have identical SHA-256:
`487c8ef87e8e7d49cc6e1ac995ba8e5b737a4f0825c568fb80cf5ad32d267094`.
This supplements tests for non-default market rules, fractional remainder,
active body health, disruption and historical pending-operation migration.

Savecheck v6: 970386 bytes; before/after StateHash e301f3cc87637e0;
canonical bytes PASS; validation PASS; encode 107.154 ms, load 100.612 ms
including validation/cache rebuild. Timing is diagnostic, not simulation state.

## Retained simulation/regression results

The 250-seed month gate passed in 132.19 seconds: 179750 market updates,
28817857 credited income, zero invalid or static worlds, controller-change range
1..77. The 25-seed year gate passed in 42.68 seconds: 218975 market updates,
30972858 credited income, zero invalid or static worlds, controller-change range
57..240. The single standard year, NPC/faction/territory/persistence suites and
independent simulation-process comparison passed in the canonical run.

Doctest totals across disjoint gates: 142 cases / 483587 assertions PASS.
Foundation 49/210838; persistence 15/4348; factions 15/403; NPC 20/511;
territory 27/3663; market fast 15/964; market multi-year 1/262860.
No skipped market case is omitted overall: its complementary CTest filter executes it.

Supported environment verified locally: C++20 Debug, GCC 16.2.0, CMake 4.4.4,
Ninja 1.13.2, Windows. No unresolved project warning or compiler error.

## Definition-of-Done evidence

| Requirement | Evidence |
| --- | --- |
| District demand/supply, capacity, shares, bounded damped price | Sixteen market cases, exact allocation and limit tests |
| Territory and market authority distinct | Rival/contested access cases; no control writes in MarketSystem |
| Existing Dealer roles | Real semantic IDs, lifecycle/role/body-health loss and registration cases |
| Sustainable authoritative income | All-five-faction income; exact initial + credits - paid costs conservation |
| Coarse deterministic scheduling | Fixed/coarse byte/hash equality, RNG isolation and process gate |
| Persist/migrate market state and StateHash | v6 canonical bytes, v1..v5 fixtures, custom rules/remainders/disruption continuation |
| Dealer loss/disruption and territory modifiers | Restrictive lifecycle, disabled/removed/reassigned role and causal access cases |
| Multi-year headless stability | Five daily-validated small worlds and ten standard worlds x five years |
| Prior base and save compatibility | Unmodified worldgen fixtures and historical payload comparisons |
| Build, regression/stress gates and documents | Canonical build log and final results below; FILE_INDEX and DEVELOPMENT_STATUS |

## Limits

No Phase 13 history, city-wide dealer spread, real-world drug mechanics, new
faction operation types, dealer replacement, recruitment, police AI, mission/story
logic or physical dealer AI. Multi-year gates validate numerical/structural
stability and deterministic continuation, not final balance or a complete V1.
Market income can fund continued operations but never regenerates lost manpower.
