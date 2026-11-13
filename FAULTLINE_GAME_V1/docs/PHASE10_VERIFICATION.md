# Phase 10 verification - NPC Semantic Simulation

Status: **completed and verified**. STOP before Phase 11 review.
Date: 2026-10-16. C++20 / GCC 16.2.0 / Ninja / Debug.
Canonical parent documents were read and were not modified.

## 1. Source inventory

Created:

* src/simulation/npc/npc_state.hpp
* src/simulation/npc/npc_system.hpp
* src/simulation/npc/npc_system.cpp
* src/persistence/schema_v3.hpp
* tests/npc_simulation_tests.cpp
* tests/fixtures/save_v2_phase9.flsave
* docs/NPC_SEMANTIC_M1.md
* docs/SAVE_FORMAT_V3.md
* docs/PHASE10_VERIFICATION.md

Modified:

* CMakeLists.txt
* src/world/state/actor_types.hpp
* src/world/state/world_state.cpp
* src/world/spatial/streaming.cpp
* src/simulation/runtime/game_state.hpp
* src/simulation/runtime/game.hpp
* src/simulation/runtime/game.cpp
* src/simulation/factions/faction_system.cpp (accounting comment only)
* src/persistence/save.hpp
* src/persistence/save.cpp
* tests/faction_simulation_tests.cpp
* tests/determinism/simulation_process.cmake
* tests/fixtures/README.md
* tools/simulator/main.cpp
* README.md
* DEVELOPMENT_STATUS.md
* docs/FILE_INDEX.md
* docs/DETERMINISM.md

All source, build artifacts and tools remain within the repository. Git already
contained the complete untracked Phase 0-9 tree; no commit was created and no
preexisting implementation was discarded. Persistence tests retain their original
fixtures, including position-only NPCs in foundation worlds without geography.

## 2. Versions

SaveFormatVersion **3**, independent from GeneratorVersion **3** and GameVersion
**0.1.0**. Runtime StateHash schema is `FAULTLINE.state.v5`. The three historical
BaseWorldHash algorithms and their field encodings are unchanged.

## 3. Backward save migration

**PASS** v1 -> current and v2 -> current. V1 covers a fixed independent wire
fixture and generated worlds for all three generator versions. V2 uses the real
Phase 9 day-30 save with 468 stored operations and eight pending operations.
Its eleven original section payloads remain byte-identical after migration.
Clock, resources, IDs/allocator high-water marks and all seven RNG states survive.
Migration adds deterministic safe activity/home/scheduling defaults without
consuming randomness; no journey or NPC operation assignment is invented.
Current v3 save/load/save bytes remain identical.

## 4. NpcState runtime additions

`NpcState.semantic` owns activity, home WorldLocation, optional travel,
nextUpdateTick and one OperationId. Existing identity, lifecycle, roles, traits,
capabilities, faction membership, current location/position, health and optional
EntityId link remain their sole authorities. Personal relationships stay in the
existing sparse directed relationship store. Generated baseline NPCs remain frozen.

## 5. Activity model

Idle, Working, Travelling, Meeting, Hiding, FactionOperation, Arrested, Recovering.
Alive actors can work/travel/participate; Injured/Arrested/Missing/Dead actors use
restricted states without ordinary travel or operation participation. Lifecycle
changes are explicit; this phase does not cause injury/arrest/death autonomously.
Leaders must remain alive; no implicit leadership-vacuum behavior is introduced.

## 6. Semantic scheduling

Default per-NPC interval: **800 ticks / 30 game minutes**, configurable. Ascending
NpcId, integer deadlines, single authoritative thread. An earliest-deadline cache
is derived and rebuilt on load. Arrival is its own scheduled boundary. Normal
fixed ticks and headless coarse advancement use the same faction -> NPC ->
structural-commit pipeline, skipping only ticks with no scheduled work.

## 7. Travel representation

Origin/destination WorldLocation plus departure/arrival ticks. Location remains
origin until arrival; completion updates domain location and normalized position.
Cancellation is explicit and does not teleport. Endpoints, lifecycle, geometry,
activity, times and optional body state are validated. There is no physical path.

## 8. Travel duration

Manhattan distance between semantic centers, multiplied by 1.25, at an estimated
120 meters/game minute; round upward to a whole tick, minimum 80 ticks. Buildings
use exact half-meter centers; other locations use territory/district centers.
The existing connected city supplies the aggregate context, not a road-by-road
trajectory. Travelling actors defer physical materialization in this milestone.

## 9. Role-aware decisions

Leaders coordinate at base. Members support operations according to courage,
loyaltyBias, ambition and leadership; home defense outranks other defense and
attack, with OperationId tie-breaking. Dealers work/hide according to confidence
and courage, and support only suitable defense. Other members may make bounded
NPC-RNG visits to leadership, then return home. No detailed citizen timetable.

## 10. Faction-operation integration

One semantic assignment per living member, preparation/active operation support,
arrival at target for suitable nonleaders, remote leadership at base, and same-tick
cleanup after resolution/cancellation. Finished support emits meaningful WorkedWith
relationship events. Named NPCs receive no aggregate random casualty roll.

## 11. Manpower accounting

Named NPCs remain outside aggregate reserve/garrison/operation-force pools and
count once in total membership. Assignment grants **no extra force or aggregate
commitment** and does not change the Phase 9 combat formula. Faction-member
promotion transfers exactly one uncommitted reserve person to named membership;
total manpower and existing commitments are conserved. Unit tests verify this.

## 12. Relationship system

RelationshipSystem is the sole runtime personal-relationship rule owner.
Helped, Threatened, WorkedWith and Abandoned update separate directed trust, fear,
hostility, personalLoyalty and debt dimensions, clamped to 0..10000. New edges
exist only for meaningful interaction; no full NPC matrix, invented history or
faction diplomacy mutation. Automatic operation support updates leadership ties.

## 13. Promotion

Noncopyable transient EphemeralActor token -> validate candidate transaction ->
allocate NpcId -> preserve identity/traits/health/location/stationary activity ->
initialize semantic scheduling -> attach existing body backlink if present ->
commit. Existing EntityId/components survive promotion without respawn. Invalid
sources, duplicate promotion, missing bodies or exhausted IDs leave state intact.

## 14. NpcId allocation

Existing persisted NpcStore allocator/high-water mark is reused. Runtime IDs come
after generated IDs with no collision/recycling. Save/load preserves the next
promotion ID. Fixtures verify two serial promotions, an existing-body promotion,
and conservative aggregate-reserve transfer. Normal generated runs have zero
organic promotions until physical/player interactions exist.

## 15. Seed 12345 / 30 days

**PASS**, tick 1152000, Validation PASS, StateHash **852beb6e925decfd**.
Full actual output:

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

NPC semantic updates: 31658

Travels started: 2349

Travels completed: 2349

Travels cancelled: 0

NPC operation participations: 1268

Relationship changes: 1237

Promotions: 0

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

StateHash: 852beb6e925decfd
```

## 16. Seed 12345 / 365 days

**PASS**, tick 14016000, Validation PASS, StateHash **4f31abf05bca9769**.
All 22 named NPCs remain alive (also confirmed by direct inspection of the actual
year-end save: 22 unique IDs, lifecycle Alive, abstract bodies, next NpcId 23).
No overdue/stuck travel or leaked commitment.
Full actual output:

```text
FAULTLINE HEADLESS SIM

Seed: 12345

Generator: 3

Simulated: 365 days

Tick: 14016000

Factions: 5

Faction #1 | Kuroda Syndicate

  Manpower: 60 | Reserve: 35 | Committed: 0 | Treasury: 26

  Cohesion: 10000 | Confidence: 10000 | Capacity: 0 | Active Operations: 0 | Operations created: 170

Faction #2 | Eastside Brotherhood

  Manpower: 37 | Reserve: 21 | Committed: 0 | Treasury: 727

  Cohesion: 6311 | Confidence: 3320 | Capacity: 1 | Active Operations: 0 | Operations created: 203

Faction #3 | Vitale Syndicate

  Manpower: 65 | Reserve: 45 | Committed: 0 | Treasury: 3

  Cohesion: 7870 | Confidence: 0 | Capacity: 0 | Active Operations: 0 | Operations created: 186

Faction #4 | Kuroda Consortium

  Manpower: 57 | Reserve: 13 | Committed: 0 | Treasury: 29

  Cohesion: 6751 | Confidence: 0 | Capacity: 0 | Active Operations: 0 | Operations created: 214

Faction #5 | Cumbre Cartel

  Manpower: 38 | Reserve: 16 | Committed: 0 | Treasury: 5

  Cohesion: 4590 | Confidence: 8520 | Capacity: 0 | Active Operations: 0 | Operations created: 170

NPC semantic updates: 385418

Travels started: 43518

Travels completed: 43518

Travels cancelled: 0

NPC operation participations: 2643

Relationship changes: 2343

Promotions: 0

Operations created: 943

Resolved (successful): 650

Failed: 293

Cancelled: 0

Active/preparing: 0

Attacks: 650

Defenses: 293

Manpower lost: 241

Treasury spent: 1011825

Peak committed manpower: 60

Factions unable to mobilize (resources): 4

Operations/faction/30-days: 15.50

Validation: PASS

StateHash: 4f31abf05bca9769
```

## 17. NPC statistics

| Statistic | 30 days | 365 days |
|---|---:|---:|
| Semantic updates | 31658 | 385418 |
| Travels started | 2349 | 43518 |
| Travels completed | 2349 | 43518 |
| Travels cancelled | 0 | 0 |
| Operation participations | 1268 | 2643 |
| Relationship changes | 1237 | 2343 |
| Organic promotions | 0 | 0 |

Promotion and interruption paths are exercised in deterministic fixtures. The
travel validator reconciles started = completed + cancelled + currently active.

## 18. Phase 9 strategic pacing

**468 operations / 30 days**, unchanged: 300 attacks, 168 defenses, 295 successful,
165 failed, eight still preparing/active. Year: 943 operations, none pending.
No operation rebalancing, recruitment, income, major NPC deaths or territory
ownership mutation was introduced. Thirty-day manpower loss 142; treasury spent
493200. Year loss 241; spent 1011825. Peak aggregate commitment 60 in both runs.

## 19. Save-mid-travel continuation

**PASS**: save halfway, load, compare state and canonical bytes, continue through
arrival and subsequent semantic updates; hashes/bytes match uninterrupted state.
Exact-deadline completion and explicit cancellation are separately covered.

## 20. Promotion save/load and operation persistence

**PASS** promoted identity/state and next NpcId survive save/load. Existing-body
linking preserves EntityId during promotion; save still uses the established
synchronized abstract snapshot policy. **PASS** save with participating named
actors and active faction operation, then compare resumed outcome/hash/bytes.
Actual day-30 CLI save plus 335 more days produces the exact same year-end save
bytes as an uninterrupted 365-day CLI run. Shared SHA-256:

`4087ac3aae92c6a40e671203276eb24ed508cb1724318d9a9354d78b566688d8`

## 21. Cross-process determinism

**PASS** final CTest separate-process simulation comparison, plus an additional
manual pair of 30-day CLI processes comparing full output byte for byte.
Both yield StateHash `852beb6e925decfd`. Year resumed/uninterrupted StateHash is
`4f31abf05bca9769`. Different-seed divergence, NPC RNG separation and fixed/coarse
pipeline equivalence are covered by automated tests.

## 22. Batch validation

| Batch | Result | Tool seconds | ms/seed |
|---|---|---:|---:|
| Seeds 1..250 x 30 days | PASS | 94.4534 | 377.814 |
| Seeds 1..25 x 365 days | PASS | 25.8855 | 1035.42 |
| Worldgen seeds 1..1000 | PASS | 138.961 | 138.961 |

All generated worlds terminate and validate. Aggregate simulation statistics:

| Statistic | 250 x 30 days | 25 x 365 days |
|---|---:|---:|
| Semantic updates | 7222341 | 8724462 |
| Travels started | 580030 | 909620 |
| Travels completed | 580022 | 909618 |
| Travels cancelled | 0 | 0 |
| Travels active at cutoff | 8 | 2 |
| NPC operation participations | 242600 | 49406 |
| Relationship changes | 238126 | 42428 |
| Promotions | 0 | 0 |
| Operations | 106232 | 21407 |
| Attacks / defenses | 73987 / 32245 | 17472 / 3935 |
| Successful / failed | 72879 / 31896 | 17472 / 3935 |
| Pending operations | 1457 | 0 |
| Manpower loss | 25999 | 3128 |
| Treasury spent | 113707425 | 23580675 |
| Peak commitment | 69 | 68 |
| Factions unable to mobilize from resources | 131 | 105 |

Active journeys at the cutoff have valid future deadlines, not overdue/zombie
travel. Operation counts/resource outcomes exactly retain Phase 9 batch behavior.

## 23. Build result

Canonical command actually executed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
```

**PASS** configuration, compilation/linking and tests; exit code 0. Latest sources
compiled with project warnings enabled and no unresolved project warnings/errors.
The final canonical invocation had no further source work to rebuild.

## 24. CTest result

**11/11 PASS**, **277.30 seconds**. Fast-labeled portion 16.97 s; persistence 7.21 s.
Coverage was retained. Labels include fast, determinism, stress, persistence,
simulation, factions, npc, relationships and worldgen/mass. Final log:
`build/phase10-build.log`, detailed outputs `build/Testing/Temporary/LastTest.log`.

## 25. Doctest cases/assertions

| Suite | Cases | Assertions | Result |
|---|---:|---:|---|
| Foundation | 49 | 210838 | PASS |
| Persistence | 15 | 3772 | PASS |
| Factions | 15 | 443 | PASS |
| NPC semantics | 20 | 511 | PASS |
| Total | 99 | 215564 | PASS |

## 26. Persistence regression

**PASS** canonical semantic round-trip, seven RNG futures, allocator continuation,
clock, normal tick continuation, malformed magic/version/length/reference/state,
every truncated prefix of a minimal save, nested truncation, finite floats,
UTF-8/collection budgets, file I/O cleanup, v1/v2 migrations, active operations,
mid-travel saves and promotion. No silent repair or process-memory serialization.

## 27. BaseWorldHash compatibility

Verified by direct CLI, regression vectors and two processes for each version:

| GeneratorVersion | Seed 12345 BaseWorldHash |
|---|---|
| 1 | 2938f0ca42162bc9 |
| 2 | 5f3a446fe0d2f974 |
| 3 | 888c5dbd10c15886 |

V3 worldgen output still has 8000 x 8000 m, one water region, eight macro-zones,
210 road nodes, 391 edges, 18 districts, 8007 parcels/buildings, 41 territories,
five factions and 22 major NPCs; Validation PASS. No generator version increment.

## 28. Performance and save size

Standalone Debug samples, including process startup/generation/validation and,
where stated, file save/load; not a microbenchmark of simulation rules alone:

| Invocation | ms |
|---|---:|
| 30-day CLI including save | 588.661 |
| 30-day independent process | 428.415 |
| 365-day CLI including save | 1087.051 |
| 335-day resumed CLI including load/save | 958.986 |
| savecheck CLI | 509.200 |

Memory savecheck, seed 12345 at tick zero: 967153 bytes; save 110.680 ms including
validation; load 97.390 ms including validation/index rebuild. StateHash before
and after is `9e4b2a1eb9f7ac91`; canonical bytes PASS. Day-30 save is 1013953 bytes;
year save is 1061453 bytes. Timing is tool-only and never authority/save content.
Commands/results are retained in `build/phase10_measure.ps1`,
`build/phase10-cli-verification.log`, `build/phase10-metrics.json`,
`build/phase10-sim-30.log`, `build/phase10-sim-365.log`, `build/phase10-resume.log`,
`build/phase10-savecheck.log` and `build/phase10-worldgen-v1.log` through `v3.log`.

## 29. Known limits

Semantic travel is an aggregate distance estimate, not detailed routing; physical
mid-travel materialization/interpolation is deferred. Named support does not alter
combat strength. Relationship memory decay, organic crowd generation, physical
AI, combat/arrest/injury causes, market, missions and territorial control remain
out of scope. Promotion uses a full candidate state copy for rare transactional
safety, not an optimized streaming path. Generic faction promotion currently
transfers uncommitted reserves, not a committed or garrison actor.
No unresolved project errors, warnings or current external execution blockers.
Earlier Windows Application Control denials were real and recorded; after the
user changed the execution environment, the complete current gate ran normally.
This agent did not change security policy or bypass blocked executables.

## 30. Next task

**Phase 11 / TerritorySystem**: presence, CONTROLLED/CONTESTED/UNCONTROLLED and
control reevaluation consuming operation consequences, only after review.
Phase 10 is complete; this session stops without starting Phase 11.
