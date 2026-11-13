# Development status

Current phase: Phase 21 Vehicles implemented and focused-verified.
Stop before Phase 22.

Completed:

* Inspected empty working directory and read canonical design specifications.
* Initialized Git and CMake project with strict project warnings. No parent
  documents were modified.
* Implemented typed IDs, FNV-1a/seed derivation, xoshiro256** state API,
  integer SimulationClock and minimal diagnostics.
* Phase 2: authoritative GameState, Game/Simulation separation, abstract TickInput,
  stable command ordering, seven independent runtime RNG streams, domain event
  buffer, typed structural command buffer and application FixedStepRunner.
* Phase 3: non-recycling EntityRegistry, ComponentStore, ordered typed domain
  stores for NPCs, factions, territories, vehicles, missions and districts;
  reference validation and allocator-aware StateHash.
* Phase 4: configurable cell/local coordinates, normalization, spatial index,
  radius queries, ACTIVE/ABSTRACT NPC streaming with hysteresis and health/location
  synchronization. All four simulation level names exist; SIMULATED/DORMANT
  behavior is deliberately deferred. No renderer is needed.
* Phase 5: versioned generation context/configuration, buildable land, optional
  coast/water, macro-zones and adjacency, hierarchical connected road graph,
  districts, validation and canonical BaseWorldHash. New GameState initializes
  from the same generated M1 world used by the tool.
* Added vendored doctest 2.4.11 and a local-only toolchain bootstrap:
  w64devkit 2.10.1 / GCC 16.2.0, CMake 4.4.4, Ninja 1.13.2.
* Phase 6: typed ParcelId; shared integer rectangle polygons and static AABB index;
  parcels with exact road frontage, weighted basic building generation and
  road-aligned connected territories with baseline metadata. Independent urban
  stages return explicit results. Full geometric/reference validation and
  canonical hashing include all new fields/configuration. No buildings become
  ECS entities; territory runtime keys resolve their generated geometry by ID.
* Phase 6 introduced generator version 2, preserving version 1 with its original
  hash. Version 2 explicitly reuses v1 topology stream versions; urban randomness
  is isolated by stage/district/block/parcel. See docs/WORLDGEN_M2.md.
* Phase 7: exactly five main factions from the four canonical GDD macro-families,
  one seed-selected duplicate; distinct names, homes, personalities, treasury,
  cohesion and real manpower baselines. Connected territorial presence consumes
  reserve members. Major NPCs (3–5/faction) have typed persistent identities,
  compositional roles, authority, traits/capabilities and semantic locations.
  Initial directed political and sparse personal relationships have explicit
  baseline context without invented history. Actor data is imported into runtime
  stores without creating physical entities. Viability/reference/hash tests added.
* Generator version 3 is now the default; versions 1 and 2 remain supported with
  exact prior BaseWorldHashes. Phase 7 expanded Runtime StateHash to schema v2 for
  semantic fields. See docs/WORLDGEN_M3.md and docs/PHASE7_VERIFICATION.md.

* Phase 8: separate faultline_persistence target; SaveFormatVersion 1, GameVersion
  0.1.0, explicit little-endian binary header/sections and full semantic saves.
  Seven runtime RNG states, clock, allocator high-water marks, generated base,
  domain records, directed relationships and existing data-only components persist.
  Active NPCs synchronize into an abstract snapshot without modifying the running
  state. Spatial membership is rebuilt. Runtime StateHash schema v3 compares the
  same semantic snapshot. Missing references/malformed input fail with typed
  diagnostics; bounded lengths, UTF-8, floats, clock and allocators are validated.
  Added 15 persistence tests and the headless savecheck/inspection tool.

* Phase 9: typed OperationId and a single faction operation store; resource-backed
  ATTACK_TERRITORY / DEFEND_TERRITORY, preparing/active/terminal lifecycle,
  bounded aggregate outcomes and scoped events. Existing reserves/treasury/
  garrisons remain authoritative; manpower/commitments/capacity are derived.
  Confidence, strategic evaluation state and integer scheduling are persistent.
  Utility uses geometry, hostility, relative strength, cost and personality;
  choices are deterministic and RNG only affects bounded resolution uncertainty.
* Simulation integrates scheduled faction rules; safe headless advancement skips
  only idle fixed ticks and uses the same tick pipeline at every due boundary.
  The simulator supports seed/days/range, operation logs and save/load. Ownership,
  diplomacy and major NPC records are unchanged by aggregate simulation.
* Phase 9 introduced SaveFormatVersion 2, with real v1 migration and a frozen independent
  historical-wire fixture. GameVersion remains 0.1.0; GeneratorVersion remains 3.
  StateHash schema v4 includes strategy, scheduler, operations and allocator.
  See docs/FACTION_SIMULATION_M1.md, SAVE_FORMAT_V2.md and PHASE9_VERIFICATION.md.

* Phase 10: persistent NPC activity/home/travel/deadlines and semantic operation
  assignments; role/trait-aware headless decisions; sparse RelationshipSystem
  events; transactional ephemeral promotion preserving an existing body and
  runtime NpcId allocator. Generic faction promotion conserves aggregate manpower.
  Named participation grants no extra aggregate force or random named casualties.
* Phase 10 introduced SaveFormatVersion 3, with real v1/v2 migration, a fixed Phase 9 active-op
  save fixture, NPC travel/scheduler/assignment persistence and StateHash schema v5.
  GeneratorVersion remains 3; GameVersion remains 0.1.0.

* Phase 11: sparse integer territorial footholds, separate actual garrison
  membership, derived CONTROLLED/CONTESTED/UNCONTROLLED and gain/hold hysteresis;
  event-driven exactly-once attack/defense consequences and scoped control events.
  Faction strategy reads current control; stale operations cancel and release
  commitments. No faction-owned-territory list, soldier transfer on capture or
  forced single-success owner switch. Initial generated/saved maps are preserved.
* Phase 11 originally introduced save v4 / StateHash v6. Independent-review repair
  adds causal local resistance with attack-disruption and successful-defense relief.
  SaveFormatVersion is now 5, with real v1/v2/v3/v4 migration, fixed active-operation
  fixtures, territory/resistance/hysteresis/consumption persistence and StateHash schema v7.
  GeneratorVersion remains 3; GameVersion remains 0.1.0. No Phase 12 logic added.

In progress: none. Phase 21 implemented and focused-verified; the full canonical
Release gate remains to be rerun (a stale running `faultline_history` process held
that executable during the repair).

Next: Phase 22 only after Phase 21 review. Do not start automatically.

Known blockers: none. No unresolved project warnings/errors. Historical Windows
execution-policy startup blocks were resolved before the successful current gates;
this agent did not modify security settings or bypass blocked executables.

## Historical Phase 11 local-resistance repair

Independent review found the omitted roadmap/GDD local-resistance requirement.
Implemented bounded deterministic opposition from local battle casualties, relieved
by survivors of successful paid defenses. It contributes to control evaluation
without owning territory or creating faction members. Initial generated maps and
all historical BaseWorldHashes remain unchanged. Save v5 and StateHash v7 persist
and hash resistance/rules; real v1..v4 saves migrate without replaying history.
Five new territory cases cover causal control effects, boundaries, relief,
saturation, malformed state, hashing, migration and nonzero/custom-rule continuation.

Actual seed-12345 checks validate: day 30 StateHash 2e0033b8ca5379b9, day 365
fa5c761eb581b41d. Resumed 30+335-day and uninterrupted year-end saves are byte
identical (SHA-256 12615f4f1c5312b5417b970f1d287b89f21f4dbb19b9359897fc3633cc0a14e0).
Savecheck v5 canonical bytes PASS. V1/v2/v3 BaseWorldHashes retain all fixtures.
Canonical tools/build.ps1 PASS, exit 0: 12/12 CTest gates in 327.12 s, including
250 x 30-day, 25 x 365-day, 1000 worldgen seeds and both process determinism gates.
Doctest 126 cases / 219571 assertions PASS; territory 27 / 3663. No warnings or
weakened assertions. Final comment-only rebuild and fast gate: 8/8 PASS in
23.63 s. Full commands, results and limits are recorded in
docs/PHASE11_VERIFICATION.md.
Historical Phase 11 checkpoint stopped before Phase 12.

## Historical Phase 11 verification

Revalidation on 2026-10-17 preserved the Phase 11 implementation already present
at session start and added two integration cases for reordered/duplicate outcome
delivery and custom territory rules across contested save/load continuation.
Expanded territory suite: 22 cases / 3564 assertions PASS; combined doctest
coverage: 121 cases / 219440 assertions. Fresh day-30 and year runs reproduce
c5c174fb61dc193 and 48c3ef5c0b1eecea; day-30 + 335-day continuation matches the
uninterrupted year-end save SHA-256 exactly. Direct generator v1/v2/v3 hashes
and v4 savecheck remain unchanged. Details and current-session commands are in
docs/PHASE11_VERIFICATION.md. The following bullets retain the original
implementation-session measurements.

Fresh canonical build/CTest revalidation: exit 0, 12/12 PASS in 340.75 s,
including 250 x 30-day / 25 x 365-day simulation batches and 1000 worldgen seeds.
The expanded territory suite was rebuilt and passed separately after the full
run's original territory entry. No production changes or project warnings;
GeneratorVersion 3, SaveFormatVersion 4 and GameVersion 0.1.0 preserved.

* Final canonical tools/build.ps1 PASS, exit 0. C++20 / GCC 16.2.0 / Ninja / Debug;
  CTest 12/12 PASS in 319.46 s. Fast-label portion 22.05 s; persistence 11.86 s.
* Doctest 119 cases / 219398 assertions PASS: foundation 49/210838, persistence
  15/4124, factions 15/403, NPC 20/511, territory 20/3522. Territory label added;
  historical coverage retained/adapted for the new authority and control behavior.
* Seed 12345 / 30 days PASS: 508 operations (+40 from static-map baseline),
  49 controller changes, 15 contested transitions; final FactionIds 1..5:
  17/0/18/5/0 controlled, one contested, zero uncontrolled. StateHash c5c174fb61dc193.
* Seed 12345 / 365 days PASS: 648 operations, none pending/committed, 60 controller
  changes; 21/0/19/1/0 controlled, no contested/uncontrolled. StateHash
  48c3ef5c0b1eecea. All 22 major NPCs remain alive; no faction is collapsed/deleted.
* Seeds 1..250 x 30 days PASS: 124.988 s / 499.953 ms per seed; control changes
  mean 33.404, range 1..77; 147 factions reach zero control; max final share 84.61%.
  Seeds 1..25 x 365 days PASS: 32.912 s / 1316.48 ms per seed; mean 80.44 changes,
  range 37..133; 62 factions reach zero; max final share 100%. Zero static worlds
  in both batches. Dominance is reported, not suppressed by artificial caps.
* Mid-contest and post-capture round-trip/continuation PASS. V1/v2/v3 migrations
  preserve old section bytes and RNG without replaying past effects. Actual
  day-30 save + 335 days equals uninterrupted year-end hash and every save byte.
  Independent 30-day processes have identical complete stdout.
* Worldgen 1..1000 PASS: 137.624 s. Direct CLI/regression/process checks preserve
  seed 12345 v1 2938f0ca42162bc9, v2 5f3a446fe0d2f974, v3 888c5dbd10c15886.
* Standalone samples: 30 days without save 563.749 ms; with save 761.526 ms;
  365 days including save 2033.393 ms. Initial v4 save 968833 bytes, memory
  encode 108.927 ms / load 96.803 ms including validation/rebuild. Day-30
  1023945 bytes; year-end 1039157 bytes. Tool timings are not simulation state.
* Algorithms, limitations, exact commands, outputs and complete file inventory:
  docs/TERRITORY_M1.md, docs/SAVE_FORMAT_V4.md, docs/PHASE11_VERIFICATION.md.

## Historical Phase 10 verification

* Canonical tools/build.ps1 PASS, exit 0; C++20 / GCC 16.2.0 / Ninja / Debug;
  no unresolved project warnings/errors. Full CTest 11/11 PASS in 277.30 s.
* Doctest 99 cases / 215564 assertions PASS: foundation 49/210838, persistence
  15/3772, factions 15/443, NPC semantics 20/511. No earlier coverage removed.
  Fast-label portion 16.97 s; persistence-label portion 7.21 s; npc/relationships
  labels added alongside existing labels.
* Seed 12345 / 30 days PASS: 468 operations unchanged; 31658 NPC updates,
  2349/2349 travels started/completed, 1268 participations, 1237 relationship
  changes, no organic promotions. StateHash 852beb6e925decfd.
* Seed 12345 / 365 days PASS: 943 operations, none pending, all 22 named NPCs
  alive; 385418 updates, 43518 completed travels, 2643 participations, 2343
  relationship changes; StateHash 4f31abf05bca9769. No overdue/stuck travel.
* Separate 30-day processes produce identical full output. Mid-travel and
  promotion save/load continuation PASS; v1/v2 migration PASS including byte
  preservation of the real v2 fixture's eleven original section payloads.
  Day-30 actual save + 335 more days produces the exact same final save bytes
  and StateHash as uninterrupted 365 days.
* Simulation batch seeds 1..250 x 30 days PASS, 94.4534 s / 377.814 ms per seed;
  seeds 1..25 x 365 days PASS, 25.8855 s / 1035.42 ms per seed. Resources and
  operation counts match Phase 9; semantic NPCs add no free aggregate manpower.
* Worldgen seeds 1..1000 PASS, 138.961 s; CLI/regression/separate-process checks
  preserve seed 12345 v1 2938f0ca42162bc9, v2 5f3a446fe0d2f974 and
  v3 888c5dbd10c15886 exactly.
* Standalone CLI samples: 30 days without file save 428.415 ms; with save
  588.661 ms; 365 days with save 1087.051 ms. Initial v3 save 967153 bytes,
  encode 110.680 ms / load 97.390 ms including validation/rebuild. Day-30 save
  1013953 bytes; year save 1061453 bytes. Timings are indicative, not state.
* Full file inventory, algorithms, commands, counters and limitations are in
  docs/PHASE10_VERIFICATION.md, docs/NPC_SEMANTIC_M1.md and SAVE_FORMAT_V3.md.

## Historical Phase 9 verification

* Final canonical `powershell -NoProfile -ExecutionPolicy Bypass -File
  tools/build.ps1`: configure/build PASS, no unresolved project warnings/errors;
  full CTest 10/10 PASS in 249.83 s. GCC 16.2.0 / Ninja / C++20 / Debug.
* Doctest totals: 79 cases / 214,590 assertions PASS. Foundation 49 / 210,838;
  persistence 15 / 3,419; faction simulation 15 / 333. No prior tests removed.
  Fast-label total 16.13 s; persistence-label total 3.94 s. Labels include
  fast/determinism/stress/persistence/simulation/factions.
* Simulation seed 12345 / 30 days: 468 operations (300 attacks, 168 defenses),
  295 successes, 165 failures, 8 live; manpower loss 142, treasury spent 493,200,
  peak commitment 60; validation PASS; StateHash 1129a254f905b856.
* Seed 12345 / 365 days: 943 operations, no live operations or commitments,
  validation PASS; StateHash 1a794fe51aff2621. All named NPCs remain alive and all
  territory owners/relationships remain unchanged. Four factions exhaust minimum
  operation funding; no recruitment/revenue/collapse system is introduced.
* Separate 30-day processes produce identical full output. Active linked
  attack/defense save/load continuation matches hashes/bytes/future OperationIds.
  Actual CLI file save on day 30 (8 live operations), load and 335 more days
  produces the same year-end hash. Day-30 v2 save is 1,012,765 bytes.
* Final simulation batches: seeds 1..250 x 30 days PASS, 83.7945 s / 335.178 ms
  per seed; seeds 1..25 x 365 days PASS, 9.34377 s / 373.751 ms per seed.
  Complete operation/resource statistics are in docs/PHASE9_VERIFICATION.md.
* Worldgen 1..1000 PASS, 140.086 s. Seed 12345 hashes remain exactly
  v1 2938f0ca42162bc9; v2 5f3a446fe0d2f974; v3 888c5dbd10c15886, verified by
  direct CLI, regression vectors, migration and separate-process comparisons.
* Standalone Debug CLI samples, including startup/generation/validation:
  30 days 447.730 ms; 365 days 435.054 ms. These are indicative samples; startup
  and measurement noise dominate the small difference. Timings are not state.
* Runtime validator covers legal operation/status/target/reference/time state,
  bounded outcomes, exact derived commitments/capacity, scheduler/config and
  cache consistency. Malformed save topology is bounded before adjacency rebuild.
  Initial generated garrisons stay strict; runtime exhausted garrisons retain
  their claim until Phase 11, with no ownership mutation in Phase 9.

## Historical Phase 8 verification

* Canonical command `powershell -NoProfile -ExecutionPolicy Bypass -File
  tools/build.ps1` succeeded: Debug GCC 16.2.0 / Ninja / C++20. Changed sources
  compile with project warnings enabled and no unresolved warnings/errors.
* Final full CTest 5/5 PASS in 152.52 s: persistence_tests, foundation_tests,
  worldgen_checkpoint, worldgen_mass_1000, worldgen_process_determinism.
* Persistence doctest: 15 cases / 2,955 assertions PASS. Existing foundation:
  49 cases / 210,838 assertions PASS. Total: 64 cases / 213,793 assertions.
* Full worldgen seeds 1..1000 PASS: final batch 138.418 s / 138.418 ms per seed.
  Counts/distributions match the prior M3 checkpoint. Two processes per supported
  generator version produce identical output. Seed 12345 hashes remain exactly
  v1 2938f0ca42162bc9; v2 5f3a446fe0d2f974; v3 888c5dbd10c15886.
* Seed 12345 / v3 save: 965,599 bytes; StateHash before/after load
  1c38e03df408cb09; reserialized bytes identical. Continuation tests cover seven
  RNG streams, all allocator domains, clock and 1000+1000 headless ticks with
  test-driven RNG/object allocation. Actual encode/load measurements and full
  file inventory are recorded in docs/PHASE8_VERIFICATION.md.
* Malformed saves reject explicitly: invalid magic/versions/section framing,
  every truncated prefix of a minimal save, nested-world truncations, invalid
  lengths/budgets/UTF-8/floats/RNG/time, duplicate/null IDs, allocator collision,
  orphan components and missing semantic references. File I/O and cleanup pass.
* Labels retain fast/determinism/stress coverage and add persistence; fast CTest
  portion of the final full run totals 14.09 s, persistence 5.12 s. No stress tests
  were removed. See docs/SAVE_FORMAT_V1.md for actual encoding and limitations.

## Historical Phase 7 verification

* Out-of-source Debug configure/build succeeded with the project's warning flags;
  the last compilation has no project warnings.
* Final `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`
  configured/built the M3 Debug targets in strict C++20 mode and passed CTest 4/4:
  `foundation_tests`, `worldgen_checkpoint`, `worldgen_mass_1000`,
  `worldgen_process_determinism`. Total final CTest time: 150.32 seconds;
  mass-seed test: 138.65 seconds.
* Final doctest: 49 cases and 210,838 assertions passed inside CTest.
  Retains core/runtime/streaming/M1 coverage and adds M2 stage reproducibility,
  general object geometry/reference properties, corrupt records, AABB oracle,
  configurable building weights, RNG isolation, micro/large maps, extreme
  origins, finite resource limits and semantic GameState integration.
  M3 coverage adds family variation, all actor stages, per-faction viability,
  disconnected/overwhelming control, collision fallback, sparse relationships,
  actor corruption, runtime import and identity/resource fingerprint checks.
* Full M3 batch: seeds 1..1000 all terminated and validated, with zero failed seeds.
  Tool time 138.641 seconds / 138.641 ms per seed. Duplication counts in canonical
  family order: 246 / 251 / 262 / 241. NPC count range 15..25; individual faction
  area share range 5.35%..23.50%. Detailed histograms are in PHASE7_VERIFICATION.
* Two independent seed 12345 / version 3 processes produced identical full output:
  8000 x 8000 m; 1 water region; 8 macro-zones; 210 road nodes; 391 road edges;
  18 districts; 8007 parcels; 8007 buildings; 41 territories; 5 factions;
  22 major NPCs; `Validation: PASS`; `BaseWorldHash: 888c5dbd10c15886`.
  Repeated after the complete build/CTest gate with the same result.
* `--version 1 --seed 12345 --validate` still produces `2938f0ca42162bc9`.
  `--version 2` still produces `5f3a446fe0d2f974`. Tests compare complete prior
  canonical topology/urban data with v3 stripped of actors. Two independent
  process outputs match for each supported version. Unknown version 4 is rejected.

Phase 10 verifies semantic NPC activity/travel, relationships and promotion.
Before Phase 12, no market revenue/recruitment, police,
mission simulation, combat, input devices or renderer are implemented. Gate C is
verified for streaming and Phase 8 semantic save/load. M1 validation
does not claim validation of the complete future sandbox. Geometric limits:
orthogonal rectangular polygons, flat geography, one building per parcel, no
interiors; large block interiors can remain vacant. Initial control is connected,
resource-backed and may leave neutral territories. At the Phase 11 checkpoint, relationships encoded baseline
context and no four-year history existed. Phase 12 added runtime markets;
Phase 13 adds compressed strategic history and causal political evolution.
Phase 9 verifies playerless aggregate operations/resources for Gate D; broader
territory/NPC consequences are now verified; market is implemented in Phase 12 below.

## Phase 12 / Market M1

Implemented deterministic hourly district markets with differentiated abstract
baseline demand, bounded supply and damped prices, actual semantic Dealer
capacity, exact faction shares/sales and authoritative treasury credits. Territory
control affects access without transferring market ownership. Lifecycle/body-health
loss, disablement and decaying district disruption reduce actual supply and income.
No automatic dealer replacement, resource duplication, worldgen changes or future
Phase 13 history is introduced. Rules, fractional credits, accounting counters,
assignments and market memory persist in save v6 and StateHash v8. V1..v5 migration
adds market runtime without retroactive revenue or RNG consumption. GeneratorVersion
3 and GameVersion 0.1.0 are unchanged.

Canonical tools/build.ps1 PASS, exit 0: all 15 CTest gates in 518.99 seconds,
C++20 Debug / GCC 16.2.0, no unresolved project warnings. Doctest: 142 cases /
483587 assertions PASS, including 16 market cases. Five small worlds x five years
passed daily validation (262860 assertions); ten standard worlds x five years
passed 437990 market updates and 62648762 total income. Prior 250 x 30-day,
25 x 365-day, 1000-seed worldgen and both process-determinism gates passed.
Historical v1/v2/v3 BaseWorldHash fixtures remain 2938f0ca42162bc9,
5f3a446fe0d2f974 and 888c5dbd10c15886. All v1..v5 save migrations pass.

Actual CLI seed-12345 day-30 plus 335-day continuation equals uninterrupted
year-end save bytes (SHA-256
487c8ef87e8e7d49cc6e1ac995ba8e5b737a4f0825c568fb80cf5ad32d267094).
Day-30 StateHash dad9074a462b8212; year-end 86fa867542f4778f. Savecheck v6
canonical bytes and semantic fingerprint PASS. No canonical file was changed.
Full commands, evidence, formulas and limits: docs/PHASE12_VERIFICATION.md,
docs/MARKET_M1.md and docs/SAVE_FORMAT_V6.md.
STOP after Phase 12. No Phase 13 history or dealer-spread implementation.


## Phase 13 / Four-Year Strategic History

Implemented HistoryVersion 1 structural transition over the immutable generated
prologue base: 48 monthly decisions, exactly 1460 daily coarse market updates,
paid dealer spread and reserve-to-persistent-NPC conversion, adjacent settlements
with six real garrison members, bounded paid influence and causal political/
personal relationships. Records compress investments, entry/discovery, annual
cooperation/disputes and monthly faction support spending. No 20 Hz four-year
loop, free resource recovery, random major NPC death, story graph or Phase 14
implementation. Explicit FIRST DELIVERY and rival-discovery evidence gates entry;
headless tools require an explicit synthetic prologue fixture. Runtime mission
integration remains Phase 16. Normal simulation resumes after completion and
never enforces permanent equilibrium caps.

SaveFormatVersion 7 / StateHash v9 preserve monthly checkpoints, evidence,
history and resulting world consequences. V1..v6 migrate without invented or
replayed history. The real v6 fixture retains all fourteen old section payloads
byte for byte. GeneratorVersion 3 and GameVersion 0.1.0 remain unchanged. Direct
v1/v2/v3 seed-12345 BaseWorldHashes retain 2938f0ca42162bc9,
5f3a446fe0d2f974 and 888c5dbd10c15886. Parent canonical specs are unchanged.

Canonical tools/build.ps1 PASS, exit 0: 19/19 CTest gates in 1268.59 s, C++20
Debug / GCC 16.2.0, zero project warnings. All 150 doctest cases / 485658 assertions
PASS, including eight new history cases / 1661 assertions. Full history replay
and viability/reference checks PASS for 1000 small worlds (427.40 s) and 250
standard worlds (310.17 s). Maximum observed market shares: 34.70% / 32.57%;
territorial shares: 50.00% / 42.85%. All five factions remain viable at generated
entry. No retry, subsidy or ownership repair was needed. All prior unit,
persistence, process determinism, 250 x 30-day / 25 x year runtime batches,
five-year markets and 1000-seed worldgen gates PASS.

Actual standard seed 12345 produces 49 dealers, 372 compressed records, tick
56064000 and StateHash 62a8488f568a6102. A saved year-one checkpoint resumes to
identical final bytes (SHA-256
15272adffbbc6b3ff108583a7423106caf3e96e062f07d585d69d24bb09a32ce).
Post-history 30+335-day save/load and uninterrupted year also match every byte
(SHA-256 9e212e7e106fcd234a0a979b112eef4f6dbb8049650c6fda4d168390f38ea7b2).
Savecheck v7 canonical bytes and semantic fingerprint PASS. Later runtime warfare
can still snowball, as documented in earlier phases and the verification report;
permanent balancing/recruitment is not pulled into this phase.

Full commands, measured outputs, algorithm and API limitations are recorded in
docs/PHASE13_VERIFICATION.md, docs/STRATEGIC_HISTORY.md and docs/SAVE_FORMAT_V7.md.
STOP after Phase 13. Phase 14 has not begun. Known blockers: none.

## Phase 14 / Story Graph Foundation

Completed and verified after inspecting/preserving all three pre-existing partial
story source files and the untracked frozen v7 fixture. Deterministic post-history
high-level possibilities use typed StoryNodeId, actual semantic world references,
outcome prerequisites, success/failure branches, live world eligibility and
existing-actor contact repair. Important NPC death never creates/resurrects an NPC
or appoints a leader. Faction/territory/market/relationship authority remains with
its existing systems. Runtime observation occurs only at coarse semantic changes.

SaveFormatVersion 8 / StateHash v10; StoryGraphVersion 1. Real v1..v7 migrations,
canonical save bytes, progressed graph continuation and malformed payload rejection
pass. GeneratorVersion 3, HistoryVersion 1 and GameVersion 0.1.0 are unchanged.
All three seed-12345 BaseWorldHash fixtures are independently rerun and unchanged.

Canonical tools/build.ps1: exit 0, warning-clean, 22/22 CTest PASS in 622.60 s
(633.16 s including build), CTEST_PARALLEL_LEVEL=4 for independent test processes.
Each authoritative simulation remains single-threaded. Doctest totals: 163 cases,
499821 assertions; new story suite 12 cases / 6689 assertions. All prior 19 gates
remain, including 1000 small / 250 standard four-year histories with replay,
worldgen 1..1000, both five-year market suites, runtime 250 x 30 / 25 x 365 days,
all old migration, RNG, allocator, NPC and faction tests.

New story standard-world batch 1..100: 7000 nodes, 6000 dependencies, 500 eligible
roots, no invalid graphs or worlds without an eligible possibility, 1307 major-NPC
references (per-world unique counts summed), all five factions covered per world;
133 contact repairs after deterministic major-dealer death fixtures. 170.64 s
under concurrent stress. Independent processes reproduce stdout and save SHA256.
Seed 12345: 70 nodes / 60 edges / 5 eligible, StateHash 7c4539b8c6002849.

Details: docs/STORY_GRAPH.md, docs/SAVE_FORMAT_V8.md and
docs/PHASE14_VERIFICATION.md. The graph must be explicitly initialized after
completed history; old saves remain uninitialized. Fixed opportunity templates,
live derived invalidation and existing-actor substitution are this milestone's
limits; no mission execution, dialogue, rewards, FIRST DELIVERY or organizational
succession AI. Known blockers: none. STOP before Phase 15.

## Phase 15 / Mission Core

Completed and verified. Implemented generic mission system with MissionState lifecycle (AVAILABLE, ACCEPTED, ACTIVE, COMPLETED, FAILED, EXPIRED, INVALIDATED, RESOLVED_BY_NPC), objective graph (GO_TO, INTERACT, DELIVER, PROTECT, ATTACK), typed IDs (MissionId, ObjectiveId), MissionDefinition/MissionInstance separation, event-driven architecture via semantic events (LocationReached, InteractionOccurred, DeliveryOccurred, ProtectedTargetResolved, AttackTargetResolved), StoryGraph bridge, persistence (SaveFormatVersion 9, StateHash v11), validation, and deterministic save/load. All tests pass, including persistence, story, core, faction, NPC, territory, and market tests. Historical BaseWorldHash fixtures remain unchanged: v1 2938f0ca42162bc9, v2 5f3a446fe0d2f974, v3 888c5dbd10c15886.

Final verification:
- CTest: 23/23 tests PASS (100%).
- Doctest totals: 170 cases / 500,563 assertions ALL PASS.
- v8 fixture: Authentic Phase 14 binary, SHA-256 df1574a61f4005a182a7bf8dec87e7fbca4993a8076dcae9df65a0c947376f08 (1,006,078 bytes); loads and migrates to v9 with MissionSimulation initialized from defaults. Phase 14 StateHash (FAULTLINE.state.v10): 7c4539b8c6002849.
- Save-mid-mission: Round-trip test verifies save/load continuation produces byte-identical output to uninterrupted execution.
- Cross-process determinism: simulation_process_determinism PASS (seeds 1-5, 30 days each).
- Multi-world batch: simulation_batch_250 (250 seeds x 30 days) and simulation_year_batch_25 (25 seeds x 365 days) PASS.
- Fix: Added MissionSimulation persistence (Section::MissionSimulation, v9 format gate), fixed const-correctness in schema_v9.hpp fields functions for MissionInstance/ObjectiveCondition/ObjectiveRuntimeState.
- Fix: Regenerated v8 fixture from authentic Phase 14 StoryGraph writer in a temporary git worktree; replaced corrupted/reconstructed fixture with proper v8-format binary. Added .gitattributes with *.flsave -text binary.

## Phase 16 / Prologue (PRIMARY 001)

Implemented canonical prologue mission system: FIRST DELIVERY using the generic MissionSystem
(GO_TO→INTERACT→DELIVER objective graph), deterministic rival discovery via `historyPrologueCandidate`,
abstract fictional product token, 48-month four-year transition via strategic history, and story graph
initialization. `PrologueState` struct (version=1, phase, firstDelivery MissionId, origin/rival FactionId,
giver/recipient NpcId, evidence) added to GameState. SaveFormatVersion 9→10, StateHash v11→v12.

Key deliverables:
- `src/simulation/prologue/prologue_state.hpp` — ProloguePhase enum, PrologueState struct
- `src/simulation/prologue/prologue_system.hpp/.cpp` — runPrologue flow, createFirstDeliveryMission,
  generatePrologueEvidence, commitPrologueEvidence, validatePrologueState
- `src/simulation/runtime/game.cpp` — initializePrologueState in constructors, validatePrologueState
  in validateState, appendPrologueStateHash in stateHash (FAULTLINE.state.v12)
- `src/persistence/schema_v10.hpp` — PrologueState serialization
- `src/persistence/save.hpp/.cpp` — Section::Prologue, v9→v10 migration
- `tools/prologue/main.cpp` — headless prologue tool
- `tests/prologue_tests.cpp` — 43 test cases / 270 assertions

Verification:
- CTest: 15/15 tests PASS (100%), incl. prologue_tests 43/43 cases / 270 assertions.
- All prior Phase 15 gates remain green; Historical BaseWorldHash fixtures unchanged.
- `tools/prologue` runs headless prologue on multiple seeds with save/load/resume.
- Stop before Phase 17.



## Phase 17 / Player Physical Controller ? verified recovery

Recovered dirty Phase 16 + partial Phase 17 work; HEAD remains Phase 15 e60833b.
No reset, clean, commit or external worktree mutation. Correct movement/body/save
foundation retained. Found first continuation divergence at tick 11: original
EntityRegistry.nextId=22, restored=35, caused by dropping NPC residency/handle
reservations while retaining the allocator. Restoring logical leases BEFORE NPC
rules fixes allocation, hysteresis and semantic scheduling without persisting
transient bodies or excluding allocator state from StateHash. Also corrected
2250 ms accelerated game-time movement to the actual 50 ms physical timestep.

Player/body identity, bounded diagonal/analog movement, aim-facing, signed cell
crossings, kinematic static collision/sliding and one-tick interaction intent are
implemented headlessly. FIRST DELIVERY remains headless; physical body creation
is explicit session setup. SaveFormatVersion 11, StateHash v13, PlayerStateVersion
1. GeneratorVersion 3, HistoryVersion 1 and GameVersion 0.1.0 unchanged.
Authentic v10 source writer recovered and independently reproduced, with the
recorded Phase 16 v12 hash 2517a20c0ec2a76e. Historical tests retain byte-exact
payload guarantees with only the explicit new empty CharacterBody field projected
out. A genuine completed-history v7 writer fixture replaces synthetic wire truncation.

Fresh canonical Release build: exit 0, **28/28 CTest PASS in 97.21 s** (111.268 s
including configure/build), CTEST_PARALLEL_LEVEL=4. No current project compiler
warnings/errors. Doctest **248 cases / 551422 assertions**, including five-year
market stress; fast suites **247 / 288562**; player **32 / 49531**.

Player seed 12345: 12000 commands, save at 6000 + continuation 6000, 24 cell
crossings, StateHash 4c0523b02894026c, byte-identical process saves SHA-256
97f570983eeb07d19dbc0325838190e056a580160818700e7c90cc7aa036cad1.
Focused active-NPC checkpoint: 10 + 1700 ticks, per-tick hash/allocator/live IDs/RNG
match. Hysteresis-ring restoration and 16000-command signed movement stress pass.
Seeds 1..25 x 12000 commands: 593 cell crossings, zero invalid positions, duplicate
players, lost components or save/continuation/hash mismatches; BatchHash
2e768b07d7cd5861, 20.05 s. All existing 1000 worldgen/history, 250 standard history,
100 story, 250 x 30-day and 25 x 365-day gates pass. BaseWorldHash v1/v2/v3 unchanged.
See docs/PHASE17_VERIFICATION.md, PLAYER_CONTROLLER.md and SAVE_FORMAT_V11.md.

Limits: ground-plane static collision only; no interiors/doors/vertical physics,
NPC physical navigation, Character AI, combat, renderer or target interaction
execution. Linear static-footprint scan is adequate for this milestone.
No Phase 18 implementation; canonical parent documents unchanged.

## Phase 18 / Basic Character AI

Implemented active-only deterministic physical AI for persistent NPC bodies.
Transient intents provide idle, move-to, locally perceived follow, flee-to-home,
attack approach and protect-location execution. Attack intent performs no combat.
NpcId remains the semantic identity; streaming owns residency and EntityId bodies.
Materialization/restoration now attach CharacterBody, abstraction synchronizes
position/health, and ordered residency bounds all 20 Hz AI work.

NPC speed is 3.5 m/s using the fixed 50 ms physical tick (0.175 m/tick). Facing is
owned by CharacterBody and reconstructed deterministically. NPC and player use the
same swept static collision/sliding and WorldPosition normalization. No persistent
AI state was required: SaveFormatVersion remains 11 and StateHash remains v13.

Focused verification: 11 doctest cases / 561769 assertions pass. Two independent
processes produce identical output/save bytes. Seed 12345 x 1200 ticks and seeds
1..10 x 1000 ticks pass with real movement, abstraction/rematerialization and zero
link/body/save/continuation/hash mismatches. Full canonical Release results are in
docs/PHASE18_VERIFICATION.md and the session report. No known blocker. STOP before
Phase 19; no combat, police or vehicle AI is implemented.

Fresh canonical Release gate: exit 0, 31/31 CTest PASS in 114.08 s (155.634 s
including configure/build), 259 doctest cases / 1,113,191 assertions PASS. Fast
regression: 20/20 in 8.05 s. Historical BaseWorldHash v1/v2/v3 unchanged.

## Phase 19 / Combat — verified

Authoritative attack intents, pistol/shotgun/SMG definitions, persistent ammo,
tick cooldown/reload, global range, simulation static LoS, deterministic damage,
player terminal Health and persistent NPC death are implemented. SaveFormatVersion
is 12, CombatStateVersion 1 and StateHash is v14. An authentic v11 fixture is
frozen and migration-tested. No witness, police, wanted or vehicle combat exists.
Fresh final Release: 34/34 CTest PASS in 153.80 s. Combat stress: 100 worlds,
2700 attacks, 300 deaths and zero save/continuation/hash mismatches. Historical
BaseWorldHash v1/v2/v3 remains unchanged. See `docs/PHASE19_VERIFICATION.md`.

## Non-canonical 2D debug viewer audit

The read-only SVG tool has explicit raw generated-world, runtime-initialized and
loaded-save modes. Runtime mode uses canonical player creation and streaming; save
mode reconstructs transient residency leases without advancing a tick or changing
StateHash. Runtime territory control, current semantic NPC state and CharacterBody
facing are rendered. The model has faction home territories but no HQ point, so the
viewer fabricates no HQ marker. Actual emitted SVG counts and byte determinism are
covered by the map2d process gate. Persistence, StateHash, GeneratorVersion and
worldgen remain unchanged.

## Phase 20 / Witness + Police

Implemented typed replay-safe CrimeEvents, event-driven local witness perception,
persistent NpcId observations/reports, explicit police knowledge and last-known
information, all eight canonical high-level Wanted states, deterministic abstract/
physical response, Character AI movement, CombatSystem force, district pressure,
and 120-day arrest advancement. No hidden player Transform is consulted without
valid active-officer LoS. SaveFormatVersion 13, StateHash v15 and PoliceStateVersion
1 persist all future-affecting state; v1-v12 migrate neutrally without Police RNG.

Focused verification is 19 cases / 1080 assertions; fast regression 26/26 PASS.
Independent process/save bytes match and 100-seed real-system stress passes with no
save, continuation, or hash mismatch. Final canonical Release is 39/39 CTest PASS
in 171.39 seconds; aggregate doctest coverage is 326 cases / 1,143,209 assertions.
See `docs/WITNESS_POLICE.md`,
`docs/SAVE_FORMAT_V13.md`, and `docs/PHASE20_VERIFICATION.md`. GeneratorVersion 3
and all historical BaseWorldHash fixtures remain unchanged. Phase 21 and vehicles
were not started.

## Phase 21 / Vehicles

Implements simple V1 cars with persistent `VehiclePersistentId` identity distinct
from transient `EntityId` bodies. Fresh games seed a deterministic road-derived
civilian and police fleet; streaming materializes/abstracts vehicles around the
player and pins the driven vehicle; enter/exit and driving run on the fixed
20 Hz / 50 ms tick through `PlayerInputFrame` only. Vehicle bodies are rebuilt from
semantic state, and persisted residency leases are reused on restore so allocator
identity never drifts. SaveFormatVersion 14, StateHash v16 and
VehicleSimulationVersion 1 persist fleet rules, driven identity, leases and
semantic vehicle records; v1-v13 migrate neutrally without seeding a fleet or
consuming gameplay RNG. GeneratorVersion remains 3 and historical BaseWorldHash
fixtures are unchanged.

Code verification (HIGH items):
- **A. Player arrest while driving:** `resolvePlayerArrest` now clears
  `state.vehicleSimulation.drivenVehicle = {}` before player destruction, verified
  in `src/simulation/police/police_system.cpp:435`.
- **B. External fatal vehicle damage:** `updateVehicleControl` now clears
  `drivenVehicle` when vehicle health <= 0, verified in
  `src/simulation/vehicles/vehicle_system.cpp:226-232`.
- **C. Player death while driving:** Already handled in
  `updatePlayerMovement` — when player health <= 0, `drivenVehicle` is cleared and
  residency committed before returning early, verified in
  `src/simulation/player/player_system.cpp:77-83`.
- **D. Legacy wrecked vehicle migration:** `migrateLegacyVehicles` now sets
  `health.current = health.maximum` when legacy vehicle had health <= 0, verified
  in `src/persistence/save.cpp:76-87`.
- **E. Vehicle exit collision:** `exitDrivenVehicle` now computes both right-side
  and left-side exit positions using `moveWithRadius` swept collision checks with
  a left-side fallback, verified in `src/simulation/vehicles/vehicle_system.cpp`.

Review repair (MUST FIX items):
1. Entity allocation: `materializeVehicle` is lease-aware and streaming/restore
   reuse the persisted allocation; an incapacitated player releases the driven
   vehicle.
2. Incapacitated player: `updatePlayerMovement` clears `drivenVehicle` and still
   runs streaming residency collection when player health is zero.
3. `VehiclePhysics.handling` is now the steering source at runtime.
4. `tools/vehicles` skips seeds with no fleet/clear segment and reports
   `Skipped seeds`, instead of aborting the mass-seed gate.
5. Speed-adaptive prefetch is explicitly deferred (below).

Measured focused gate after all repairs: 5/5 CTest PASS in 69.38 s
(vehicle, vehicle process determinism, persistence, police, police process
determinism). `faultline_vehicles --range 1 100 --ticks 240` -> Validation PASS,
100/100 worlds, 0 skipped. 5 deterministic collision tests pass: building/static,
pre-save/load, post-continuation, cell-boundary, meaningful footprint. See
`docs/SAVE_FORMAT_V14.md` and `docs/PHASE21_VERIFICATION.md`.

Collision stress test: 5 deterministic focused tests exercise building/static
collision, collision immediately before save/load, collision after continuation,
collision near a cell boundary, and meaningful vehicle footprint — all passing
(49/49 total tests, up from the original 100-seed run with 0 collisions that the
user explicitly said should not be misrepresented as collision stress coverage).

Environment note: the full-tree build could not link `faultline_history.exe`
because an unrelated running process (PID 3432) held that file open; every changed
source compiled and the focused targets linked and passed. The canonical full
Release gate must be rerun after that process exits.

git diff --check: passed (no whitespace errors).

git status --short: 7 files modified within Phase 21 scope (5 code fixes +
PHASE21_VERIFICATION.md update + FILE_INDEX.md update).
