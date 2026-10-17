# Development status

Current phase: Phase 12 - Market M1 completed and verified.
Stop after Phase 12; do not begin Phase 13.

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

In progress: none. Phase 12 is ready for review.

Next: Phase 13 / Four-year history after Phase 12 review. Do not start automatically.

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
resource-backed and may leave neutral territories. Relationships encode baseline
context; no four-year history or runtime political simulation exists; Phase 12 adds runtime markets.
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
