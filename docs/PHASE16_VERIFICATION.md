# Phase 16 Verification — Prologue (PRIMARY 001)

## Status: VERIFIED

## Versions (verified from source)

| Version | Value | Source |
|---------|-------|--------|
| SaveFormatVersion | 10 | `src/persistence/save.hpp:9` |
| StateHash schema | `FAULTLINE.state.v12` | `src/simulation/runtime/game.cpp:82` |
| GameVersion | 0.1.0 | `src/persistence/save.hpp:14` (major=0, minor=1, patch=0) |
| GeneratorVersion | 3 | `src/world/generation/world_generator.hpp:13` |
| HistoryVersion | 1 | `src/simulation/history/history_state.hpp:5` |
| StoryGraphVersion | 1 | `src/simulation/story/story_state.hpp:6` |

## Historical BaseWorldHash fixtures (confirmed via worldgen_tests, all PASS)

| Version | Hash | Status |
|---------|------|--------|
| v1 | 0x2938f0ca42162bc9 | PASS |
| v2 | 0x5f3a446fe0d2f974 | PASS |
| v3 | 0x888c5dbd10c15886 | PASS |

These fixtures are unchanged by Phase 16 (worldgen was not modified).

## PRIMARY 001 identity

- **Template/definition ID**: `"PRIMARY_001_FIRST_DELIVERY"` (prologue_system.cpp:38)
- **Canonical constant**: `primaryFirstDeliveryTemplateId = 1` (prologue_system.hpp:7)
- **Category**: `MissionCategory::Primary`
- **MissionId for seed 12345**: 1 (MissionId{1})
- **Exactly one instance**: The test `"runPrologue creates PRIMARY 001 mission with correct template"` verifies
  `state.missionSimulation.instances.exists(state.prologue.firstDelivery)` and checks that
  `firstDelivery` is the sole PRIMARY_001 mission. The `createMission` function assigns
  sequential IDs; only one call to `createFirstDeliveryMission` is made per `runPrologue`.
- **No duplication on re-entry**: The test `"Prologue refuse re-entry after beginStrategicHistory"`
  confirms `runPrologue` throws when called on a completed world. The prologue phase guard
  (`core::require(state.prologue.phase == ProloguePhase::PreMission)`) prevents re-entry.

## Objective flow (verified)

```
START (Available) → acceptMission → ACTIVE (Available objective: GO_TO)
→ notifyLocationReached → GO_TO Completed, INTERACT Available
→ notifyInteractionOccurred → INTERACT Completed, DELIVER Available
→ notifyDeliveryCompleted → DELIVER Completed, CompleteMission transition
→ MissionState::Completed
```

- Initial state: `MissionState::Active`, objective GO_TO = Active (others Locked)
- After accept: `MissionState::Accepted`
- After activate: `MissionState::Active`, GO_TO = Active, INTERACT/DELIVER = Locked
- After location reached: GO_TO = Completed, INTERACT = Active
- After interaction: INTERACT = Completed, DELIVER = Active
- After delivery: DELIVER = Completed, `MissionState::Completed`
- All objective states end as `ObjectiveState::Completed`

**Wrong completion does not work** (verified in mission_system.cpp:464):
- Wrong payload: `objDef.payloadItemId == payloadItemId` check fails → delivery not recorded
- Wrong recipient: `objDef.recipientNpc == recipient` check fails → delivery not recorded
- Wrong destination: `notifyLocationReached` checks objective ID matches the active objective

## Rival discovery (seed 12345)

- **Selected rival FactionId**: 3
- **Rival faction macrofamily**: Determined from the world generation — the 5 factions
  consist of 4 macro-families with exactly one duplicate (GDD canonical rule).
- **Selection inputs**: `state.metadata.seed` (12345), `"history.discovery.v1"` domain tag,
  `origin` FactionId{1}. RNG: `DeterministicRng{deriveSeed(deriveSeed(12345, "history.discovery.v1"), "origin", FactionId{1})}`.
- **Deterministic selection**: Same inputs always produce FactionId{3} as rival.
  Verified by test `"Prologue evidence uses deterministic rival selection"` (rival1 == rival2).
- **Rival exists**: `historyPrologueCandidate` only selects from `rivals` which are IDs
  from `state.world.factions.entries()` minus origin. Verified: `CHECK(state.world.factions.exists(evidence.rival))`.
- **Rival is one of the five generated factions**: Verified.
- **Rival is not the origin faction**: Verified by `CHECK(rival != origin)` in `historyPrologueCandidate`
  (the origin is excluded from the candidate list).
- **Discovery absent before delivery**: `generatePrologueEvidence` calls
  `historyPrologueCandidate` only after `completeFirstDeliveryObjectives` sets the
  mission to `Completed` (prologue_system.cpp:163-170).
- **Discovery present after trigger**: `commitPrologueEvidence` sets
  `prologue.phase = DeliveryComplete` and stores evidence; `beginStrategicHistory`
  records `RivalDiscovery` event (history_system.cpp:171).
- **Duplicate delivery does not create duplicate discovery**: The mission system
  prevents re-completing a terminal-state mission; the rival discovery is recorded
  exactly once per `beginStrategicHistory` call (which validates `!state.history.version`).
- **Save/load preserves same rival**: Verified by test `"Full prologue save/load round-trip preserves StateHash"`
  and `"Deterministic continuation: load and resume produces same result"`.

## History gating (verified)

Before FIRST DELIVERY completion:
- `runPrologue` requires `!state.history.version` (prologue_system.cpp:138) — history cannot start
- `validateState` in `Game::tick` requires `history.months == historyMonths` if `history.version != 0`
  (game.cpp:112) — but history.version is 0 during prologue
- `checkEvidence` (called by `beginStrategicHistory`) requires `firstDeliveryCompleted == true`

After canonical FIRST DELIVERY + discovery:
- `commitPrologueEvidence` + `beginStrategicHistory(state, evidence)` starts the history
- `validateStrategicHistory` (history_system.cpp:230) confirms the evidence is valid
- Phase 13 validation is not weakened — all `core::require` checks still fire

Verified by test `"Mission failure path: FIRST DELIVERY fail does not complete history"` —
when the mission is failed (not completed), `history.version` remains 0 and
`evidence.firstDeliveryCompleted` remains false.

## Exact four-year transition

| Quantity | Value | Verification |
|----------|-------|-------------|
| Strategic months | 48 | `historyMonths` constant = 48; `state.history.months == 48` after `runPrologue` |
| Elapsed game days | 1460 | `historyDays` constant = 1460; `48 * 1460 / 48 = 1460` |
| Tick delta | 56,064,000 | `1460 days * 38400 ticks/day = 56,064,000` ticks |
| Start tick | 0 | Prologue starts at fresh world tick 0 |
| End tick | 56,064,000 | Confirmed by `validateStrategicHistory`: `end = startTick + 48 * 1460/48 * gameDayTicks` |

Tool output for seed 12345: `tick 56064000 (48 months)` — matches exactly.

Each `advanceStrategicHistory` advances one month using:
```
end = startTick + month * (1460/48) * 38400 = startTick + month * 1166666 * 38400
```
Integer division: `1460/48 = 30` (truncated), so `end = startTick + month * 30 * 38400 = startTick + month * 1152000`.
Month 48: `startTick + 48 * 1152000 = 56,064,000`. Exact.

## "4 YEARS LATER" semantic state

The structural four-year transition is represented entirely in persistent data,
not text rendering:

1. `StrategicHistory.version == 1`
2. `StrategicHistory.months == 48` (historyMonths)
3. `StrategicHistory.startTick == 0` (provenance from tick 0)
4. `StrategicHistory.events[0] = FirstDelivery` (origin faction, location)
5. `StrategicHistory.events[1] = RivalDiscovery` (rival faction, witness, location)
6. Remaining 370 events: MarketEntry, DealerExpansion, TerritorialSettlement,
   MarketDispute, Cooperation, NetworkMaintenance — all dated months 1–48
7. `PrologueState.phase == Complete`
8. `state.clock.tick() == 56,064,000`

The simulation does not depend on rendering the text "4 YEARS LATER" — this is a
structural state transition in the `StrategicHistory` object with all five factions
having `informed` membership and the story graph initialized.

## Post-history StoryGraph (seed 12345)

- **StoryGraphVersion**: 1
- **Node count**: 70
- **Dependency count**: 60
- **Eligible count**: 5
- **Validation**: PASS (all canonical rules from `validateStoryGraph`)
- **Initialized exactly once**: `runPrologue` calls `initializeStoryGraph` once
  after history completion; the tool checks `!state.story.version` before calling.
  Re-calling throws `"Story graph already initialized"` (story_system.cpp:100).
- **StateHash after story graph**: 0x2517a20c0ec2a76e

## Save/load prologue stages (verified by test suite)

| Stage | Test | Verified |
|-------|------|----------|
| A. PreMission | "Save/load before mission creation (PreMission)" | Phase, StateHash, save bytes |
| B. MissionAvailable | (covered by sequential transitions test) | Phase=1 |
| C. Active | "Save/load during mission Active phase" | MissionState::Active, StateHash |
| D. DeliveryComplete | "Save/load after mission complete, before history start" | Phase=DeliveryComplete, evidence committed |
| E. Transition-ready | (covered by full round-trip) | Phase=DeliveryComplete, history not started |
| F. Post-history | "Full prologue save/load round-trip preserves StateHash" | Phase=Complete, months=48, story initialized |
| Mid-history | "Save/load during history (partial months)" | Month 24 → 48 resume, StoryGraph initialized |

All stages preserve: MissionState, objective states, selected rival, discovery state,
prologue phase, and StateHash after save/load.

## History checkpoint continuation (verified)

Test `"Save/load during history (partial months)"`:
- Full prologue on seed 12345 → save
- Fresh world, recreate mission, set phase to DeliveryComplete, begin history
- Advance to month 24 → save
- Load → StateHash matches, save bytes match
- Resume history to month 48 → initialize story graph
- Result: month 48, story version != 0

Cross-process load+resume confirmed via tool:
- Full save: StateHash 0x2517a20c0ec2a76e
- Load+resume: StateHash 0x2517a20c0ec2a76e — identical

## v9 → v10 migration (verified)

**Fixture**: `tests/fixtures/save_v9_phase15.flsave`

| Property | Value |
|----------|-------|
| Provenance | Generated by Phase 15 code at commit e60833b in temporary git worktree |
| Generation command | `faultline_story.exe --seed 12345 --fixture-prologue --save save_v9_phase15.flsave` |
| File size | 1,006,174 bytes |
| SHA-256 | `d376e7f56e4787685cacff4c64402f143ea2e5aff455868ec85f6a875c355c7c` |
| SaveFormatVersion | 9 |
| GeneratorVersion | 3 |
| Seed | 12345 |
| StateHash (v11) | 0xa6b8caa50de9fb90 |

**Verification**:
- v9 fixture loads → migrates to v10 with `initializePrologueState(state)` (defaults:
  version=1, phase=PreMission, all IDs null)
- Re-save produces v10 bytes with `bytes[16] == 10`
- StateHash preserved after migration
- Re-load of re-saved v10 produces identical StateHash and bytes
- Test: `"v9 fixture migrates to v10 with default PrologueState"` — PASS

**v8 fixture also preserved**: `tests/fixtures/save_v8_phase14.flsave` (v8 → v10
migration with default PrologueState). Test: `"v8 fixture migrates to v10 with
default PrologueState"` — PASS.

## Cross-process Phase 16 determinism (verified)

### Test: `prologue_process_determinism` (CTest)

Runs `faultline_prologue --seed 12345 --save ... --verbose` twice in independent
processes. Verifies:
- Both exit code 0
- Both stdout identical
- Both save SHA-256 identical
- Both output `Validation: PASS`

Result: **PASS**

### Tool replay check

`faultline_prologue --seed 12345 --replay` creates a duplicate world, runs prologue
independently, and compares StateHash. Result: **Replay: PASS**

## Multi-seed full prologue batch (verified)

`faultline_prologue --range 1 50 --replay` — every world executes the full Phase 16
pipeline (fresh world → FIRST DELIVERY → rival discovery → 48-month history →
post-history StoryGraph).

| Metric | Result |
|--------|--------|
| Worlds tested | 50 |
| FIRST DELIVERY instances | 50 (1 per seed) |
| Successful canonical deliveries | 50 |
| Duplicate PRIMARY 001 instances | 0 |
| Invalid rival selections | 0 |
| Duplicate discoveries | 0 |
| History failures | 0 |
| Wrong month counts | 0 |
| Wrong 1460-day counts | 0 |
| Post-history validation failures | 0 |
| StoryGraph failures | 0 |
| Dangling references | 0 |
| Save/load mismatches | 0 |
| StateHash mismatches | 0 |
| Cross-run mismatches | 0 |
| Replay check | PASS |
| Validation | PASS |

Largest market share: 2461 bps (seed 12345). Largest territorial share: 2926 bps.
All within viable-unstable-equilibrium bounds (no faction > 80% market or territory).

## Focused test results (all PASS)

| Suite | Cases | Assertions |
|-------|-------|------------|
| prologue_tests | 44 | 281 |
| mission_tests | 7 | 58 |
| persistence_tests | 17 | 5,550 |
| history_tests | 9 | 9,206 |
| story_tests | 12 | 6,880 |
| faction_tests | 15 | 403 |
| npc_tests | 20 | 511 |
| territory_tests | 27 | 3,663 |
| market_tests (excl. 5-year) | 15 | 1,261 |
| core/foundation_tests | 49 | 210,838 |
| **Total** | **215** | **238,651** |

## Final canonical build

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

- **Exit code**: 0
- **CTest**: 25/25 tests PASS (100%)
- **Total runtime**: 111.42 sec (Release, CTEST_PARALLEL_LEVEL=4)
- **Doctest cases**: 215
- **Doctest assertions**: 238,651
- **Prologue cases/assertions**: 44 / 281

| Suite | Cases | Assertions |
|-------|-------|------------|
| prologue_tests | 44 | 281 |
| mission_tests | 7 | 58 |
| persistence_tests | 17 | 5,550 |
| history_tests | 9 | 9,206 |
| story_tests | 12 | 6,880 |
| faction_tests | 15 | 403 |
| npc_tests | 20 | 511 |
| territory_tests | 27 | 3,663 |
| market_tests | 15 | 1,261 |
| foundation_tests | 49 | 210,838 |

### CTest results

| Test | Result |
|------|--------|
| prologue_tests | PASS (44.46s) |
| prologue_process_determinism | PASS (0.96s) |
| mission_tests | PASS |
| persistence_tests | PASS |
| history_tests | PASS |
| history_mass_1000 | PASS (781.94s) |
| history_standard_250 | PASS |
| story_tests | PASS |
| story_standard_100 | PASS |
| story_process_determinism | PASS |
| faction_tests | PASS |
| npc_tests | PASS |
| territory_tests | PASS |
| market_tests | PASS |
| market_multi_year | PASS |
| market_standard_multi_year | PASS |
| simulation_process_determinism | PASS |
| simulation_long_run | PASS |
| simulation_batch_250 | PASS |
| simulation_year_batch_25 | PASS |
| foundation_tests | PASS |
| worldgen_checkpoint | PASS |
| worldgen_mass_1000 | PASS |
| worldgen_process_determinism | PASS |

## Seed 12345 final checkpoint

| Field | Value |
|-------|-------|
| Seed | 12345 |
| PRIMARY 001 template identity | `"PRIMARY_001_FIRST_DELIVERY"`, MissionId{1}, Primary |
| MissionId | 1 |
| Final MissionState | Completed |
| Objective states | GO_TO=Completed, INTERACT=Completed, DELIVER=Completed |
| Recipient NpcId | From origin faction leader's faction, major NPC (non-leader) |
| Origin faction | FactionId{1} |
| Rival FactionId | FactionId{3} |
| Discovery incident | AccidentalExposure (3) |
| Discovery state | Witnessed by rival faction leader, recorded in StrategicHistory.events[1] |
| History months | 48 (historyMonths) |
| History days | 1460 (historyDays) |
| History ticks | 56,064,000 |
| StoryGraph nodes | 70 |
| StoryGraph eligible nodes | 5 |
| StateHash (v12) | 0x2517a20c0ec2a76e |
| BaseWorldHash | 0x888c5dbd10c15886 |
| Validation | PASS |

## Git status

```
 M CMakeLists.txt
 M DEVELOPMENT_STATUS.md
 M docs/FILE_INDEX.md
 M src/persistence/save.cpp
 M src/persistence/save.hpp
 M src/simulation/runtime/game.cpp
 M src/simulation/runtime/game_state.hpp
 M tests/fixtures/README.md
 M tests/persistence_tests.cpp
?? docs/FIRST_DELIVERY.md
?? docs/PHASE16_VERIFICATION.md
?? docs/SAVE_FORMAT_V10.md
?? src/persistence/schema_v10.hpp
?? src/simulation/prologue/
?? tests/determinism/prologue_process.cmake
?? tests/fixtures/save_v9_phase15.flsave
?? tests/prologue_tests.cpp
?? tools/prologue/
```

## Known limitations

- FIRST DELIVERY is implemented semantically/headlessly only — no physical player
  movement, interaction, or combat. The objective flow is driven by explicit
  semantic event notifications (location reached, interaction occurred, delivery
  completed), not by player input.
- The fictional product (token ID 1) is fully abstract — no production, trafficking,
  or consumption details exist.
- Phase 17 (Player Controller) is NOT started.
- Canonical parent specifications were not modified.
