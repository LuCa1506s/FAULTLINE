# Phase 13 verification

Date: 2026-10-17. Phase 13 only; Phase 14 not started. Parent canonical specs
were read and remain unchanged. No canonical conflict was found.

## Implementation and source mapping

### Independent-review presence-limit repair (2026-10-17)

Review found that monthly dealer influence could exceed a supported custom
`TerritoryRules::maximumPresence`, throwing after history mutation and preventing
saving. The increment now uses the smaller of the authoritative territory maximum
and the dealer-capacity target. Influence already above the dealer target remains
unchanged; increments remain at most ten and never exceed the territory maximum.
This is an invariant repair, with HistoryVersion 1, save v7 and generator v3
unchanged. No persistent schema, canonical rule or historical fixture changes.

Added a regression for maximumPresence 120 / homeEntrenchment 0 with all other
defaults and normal territory initialization (the reported valid configuration),
maximumPresence 125 to exercise a partial final increment, and default rules to
check preservation of stronger existing entrenchment. Each runs all 48 months,
checks bounded and preserved influence every month, reloads month 1/12/47 saves,
compares final hashes and complete save bytes with uninterrupted history, and
checks a further 30 days of ordinary runtime save/load continuation.

Before the production fix, the new regression failed at the first month with
`Invalid territorial presence`, maximumPresence 120, seed 12345, tick 1152000.
After the fix, the targeted regression passes 1 case / 7330 assertions. Direct
`build/faultline_history_tests.exe` passes 9 cases / 8991 assertions. Compilation
with the existing strict project warnings passes without warnings.

Additional commands actually run for this repair:

* `. ./tools/activate_toolchain.ps1; cmake --build build; build/faultline_history_tests.exe --test-case='history respects custom*'`
  (first reproduced failure; then passed after the fix).
* `build/faultline_history_tests.exe` (9/9 cases, 8991/8991 assertions PASS).
* `build/faultline_history.exe --seed 12345 --fixture-prologue --replay --save build/phase13-repair-full.flsave`
  (validation and replay PASS; StateHash remains `62a8488f568a6102`).
* `build/faultline_savecheck.exe --inspect build/phase13-repair-full.flsave`
  (save v7, generator v3, tick 56064000, validation PASS).
* `Get-FileHash build/phase13-repair-full.flsave` (SHA-256 remains
  `15272adffbbc6b3ff108583a7423106caf3e96e062f07d585d69d24bb09a32ce`).
* `git diff --check` (PASS).

The original implementation-session evidence follows below; repair gate results
are recorded separately so the two runs remain distinguishable.

WORLD_GENERATION sections 44..55, 74..76, 95..97 and 101; GDD sections 47..51;
FACTION_SIMULATION resource/presence/diplomacy/history rules; DRUG_MARKET sections
22..30; NPC aggregate conservation, persistent identity, relationships and soft
protection; roadmap section 17 guide this implementation. Entry requires explicit
FIRST DELIVERY then rival-discovery evidence. The headless tool uses an explicitly
requested synthetic prologue fixture, not a runtime mission or story graph.

HistoryVersion 1: 48 monthly decisions, exactly 1460 game days, daily reuse of
existing market economics, paid dealer investments transferring real reserves to
persistent NPCs, adjacent six-member settlements, bounded paid dealer influence,
causal political/personal relationships and compressed meaningful records. See
STRATEGIC_HISTORY.md for formulas, boundaries and limitations. Existing base
worlds/hashes remain immutable; the transition produces post-timeskip semantic
state. Save v7 / StateHash v9 persist every history field and existing economic
consequence. GameVersion remains 0.1.0 and GeneratorVersion remains 3.

## Commands actually run

* Read AGENTS.md, DEVELOPMENT_STATUS.md, automation/phases/phase-13.md, all six
  phase-named parent canonical documents, current save/version documentation,
  relevant source, migration fixtures and tests.
* Before editing/building the new writer:
  `build/faultline_sim.exe --seed 12345 --days 30 --save tests/fixtures/save_v6_phase12.flsave`.
  Real v6 fixture 1,025,498 bytes; hash/provenance in tests/fixtures/README.md.
* `. ./tools/activate_toolchain.ps1; cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++; cmake --build build`.
  Compilation PASS with project warning flags, no warnings.
* `. ./tools/activate_toolchain.ps1; ctest --test-dir build -L fast --output-on-failure`.
  All nine preexisting fast gates PASS in 30.41 s before the new suite was added.
* `build/faultline_history.exe --range 1 20 --fixture-prologue --replay`.
  PASS, all standard worlds reproducible/viable. Preliminary version before cost
  summaries / physical position synchronization; final gates below supersede it.
* `build/faultline_history_tests.exe`: 8 cases / 1661 assertions PASS before final
  position synchronization. The canonical build reruns the final suite.
* `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`.
  PASS, exit 0: all 19 CTest gates in 1268.59 s; log `build/phase13-build.log`,
  detailed stdout/counters in `build/Testing/Temporary/LastTest.log`.
* `build/faultline_history.exe --seed 12345 --fixture-prologue --replay --save build/phase13-full.flsave`.
* `build/faultline_history.exe --seed 12345 --fixture-prologue --months 12 --save build/phase13-year1.flsave`.
* `build/faultline_history.exe --load build/phase13-year1.flsave --months 48 --save build/phase13-resumed.flsave`.
* `Get-FileHash build/phase13-full.flsave,build/phase13-resumed.flsave`.
* `build/faultline_savecheck.exe --inspect build/phase13-full.flsave`.
* `build/faultline_savecheck.exe --seed 12345`.
* `build/faultline_sim.exe --load build/phase13-full.flsave --days 365 --save build/phase13-after-year.flsave`.
* `build/faultline_sim.exe --load build/phase13-full.flsave --days 30 --save build/phase13-after30.flsave`.
* `build/faultline_sim.exe --load build/phase13-after30.flsave --days 335 --save build/phase13-after30plus335.flsave`.
* `Get-FileHash build/phase13-after-year.flsave,build/phase13-after30plus335.flsave`.
* Direct `build/faultline_worldgen.exe --seed 12345 --version N --validate`, for N=1,2,3.
* `git diff --check`: PASS.

## Actual sample and persistence results

Final standard seed 12345 / origin 1: 48 months, tick 56064000, 49 persistent
semantic dealers, 372 compressed events, 1460 daily market updates. No physical
entities. Largest market share 2461 bps, largest territorial share 2926 bps.
Validation PASS; replay PASS. BaseWorldHash remains 888c5dbd10c15886.
Final StateHash 62a8488f568a6102. Year-one checkpoint: 12 months, tick 14016000,
45 dealers, 128 records, 365 updates, StateHash 7bf0a7e0619c77a4.

Uninterrupted and year-one-resumed final saves have identical SHA-256:
`15272adffbbc6b3ff108583a7423106caf3e96e062f07d585d69d24bb09a32ce`.
Savecheck inspection PASS. Initial-state savecheck v7 round-trip/bytes PASS:
970468 bytes, StateHash d20672b59b9a687d before and after; encode 109.137 ms,
load 98.194 ms (indicative local Debug measurements, not simulation state).

The eight new cases cover all five selected origin factions, all six deterministic
incident families, invalid/repeated/overflow entry with unchanged bytes, incomplete
history runtime rejection, exact monthly time and market-update counts, zero
runtime RNG consumption, unchanged base identity, real resource conservation,
paid settlements and dealer identities, meaningful relationship/tension changes,
checkpoints at discovery/month 1/year 1/month 47, ordinary runtime continuation,
unknown history versions, bad references/enums/costs/order/time, every truncated
history suffix and real v6 migration with unchanged old payloads and 335-day future
continuation. Existing v1..v5 migration and all historical worldgen gates remain.

Post-history ordinary runtime: a 365-day uninterrupted run and actual 30-day
save/load plus 335-day continuation match final StateHash 8272cdbbf5b8a60c and
all save bytes (SHA-256
`9e212e7e106fcd234a0a979b112eef4f6dbb8049650c6fda4d168390f38ea7b2`).
All actors/resources/references validate; 759 normal operations resolve with no
pending operation leak. As in earlier phases, ordinary runtime can snowball:
this sample ends with one faction controlling all 41 territories, while all five
retain market distribution and semantic leadership/members. This is a documented
later-runtime balancing limitation, not an altered history-output gate or a
claim of permanent equilibrium. Phase 13 guarantees viable generated entry;
it does not implement the later advanced faction/recruitment/balancing milestones.

Direct historical BaseWorldHash checks PASS: v1 2938f0ca42162bc9,
v2 5f3a446fe0d2f974, v3 888c5dbd10c15886.

## Required final gates

The canonical run has passed the final history unit suite (10.40 s), 1000
small-world four-year histories with independent replay (427.40 s), 250 standard
worlds with independent replay (310.17 s), and independent-process history equality
(2.91 s). All eight doctest invocations pass: 150 cases / 485658 assertions (including
8 new history cases / 1661 assertions). Both old five-year market gates pass
(113.53 s small/daily-validation suite, 59.18 s ten standard worlds). Territory,
foundation, persistence, faction, NPC, market and ordinary simulation process
checks pass. Runtime batches 250 x 30 days (132.43 s), 25 x 365 days (42.72 s),
and the standalone year (2.39 s) pass. No project warnings are present.
Final canonical build PASS, exit 0: 19/19 CTest gates in 1268.59 s.
Worldgen seeds 1..1000 PASS in 138.81 s; independent-process worldgen equality
PASS in 1.06 s. Fast-label total 41.94 s; persistence-label total 29.02 s.
All required gates completed. No failing test was skipped or weakened and no
canonical parent file or historical fixture was modified. Zero project warnings.
Stopped after Phase 13.

## Mass-history results

Every seed below runs the complete four-year transition twice from the same
initial snapshot and compares final StateHash. Full completed-output validation
checks references, resource/leadership viability, city-wide distribution and
unresolved tension; zero invalid or nonviable output worlds were accepted.

| Gate | Seeds | Time | Total dealers | Compressed records | Maximum market share | Maximum territory share | BatchHash |
|---|---|---|---|---|---|---|---|
| 512 m worlds | 1..1000 | 427.40 s | 24303 | 346141 | 34.70% | 50.00% | 79e799950a31c939 |
| Standard 8000 m worlds | 1..250 | 310.17 s | 10705 | 91568 | 32.57% | 42.85% | e1b1c2824b683a3a |

These are observed distributions, not hardcoded target shares. No retry, resource
injection, ownership overwrite or weakened assertion was needed to pass either
gate. All five selected origins and six discovery incident families are separately
covered in the unit suite; mass ranges use origin 1. Independent processes
reproduce complete seed-12345 stdout in 2.91 s.

## Scope and limitations

The history level models economic expansion, adjacent settlement, paid territorial
presence and political competition. It does not replay every runtime military
operation or kill major NPCs at random. Entry requires a prepared viable headless
prologue snapshot without operation/travel commitments. These are explicit API
preconditions, not fabricated reconstruction of a played mission. Runtime FIRST
DELIVERY is Phase 16; advanced alliance/war operations remain later phases. The
80% completed-output rejection criterion never modifies resources/control and is
not a permanent runtime balance restriction. Cross-platform floating-point bit
identity remains outside the existing supported determinism scope.
