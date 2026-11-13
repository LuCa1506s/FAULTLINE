# FAULTLINE CODEX HANDOFF

## Repository checkpoint

- Repository: `C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1`
- Branch: `master`
- Phase 20 starting HEAD: `a19439e Docs: Add Codex handoff`
- Latest committed canonical gameplay phase: `33f798a Phase 19: Combat`
- Phase 20 Witness + Police is completed/verified but intentionally uncommitted for review.
- Phase 21 Vehicles has not started.

Always inspect `git status --short`, `git diff --check`, `git diff --stat`, and
`git log -10 --oneline --decorate`. Preserve the uncommitted Phase 20 work; do not
reset, clean, stash, discard, or overwrite it.

## Canon and authority

Canonical parent specifications under `C:\Users\admin\Documents\FAULTLINE` are
read-only. `POLICE_WANTED_STATE_MACHINE_SPEC.md` is the primary Phase 20 authority.
Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, the requested phase file, and current
code. Canon/current code override this handoff.

Core invariants remain: C++20/CMake; 20 Hz/50 ms authoritative physical timestep;
single-threaded deterministic simulation; explicit named RNG streams; stable typed
IDs; `NpcId != EntityId`; streaming owns transient residency; semantic state stores
no raw pointers; ordered event buffers replace recursive/global event mutation;
renderer/input/Map2D never determine gameplay truth; seed + GeneratorVersion define
base-world identity.

Crime, witnesses/reports, police knowledge/Wanted, Character AI, Combat, streaming,
missions, StoryGraph and rendering retain separate authority. Police may update
last-known player information only from canonical reports or active-officer range +
static LoS. Police movement uses Character AI/shared movement. Police force produces
CombatIntent; only CombatSystem edits Health.

## Completed work

Phases 0-19 remain as documented. The committed `bbc4450` Map2D SVG viewer is an
audited non-authoritative tool.

Phase 20 adds:

- typed `CrimeEventId`/`CrimeReportId`, generic player/NPC/unknown actor attribution,
  combat-fact ingestion and replay-safe source identities;
- event-driven SpatialGrid witness discovery, stable NpcId ordering, cell-aware range,
  shared authoritative static LoS, delayed persistent reports, death interruption,
  abstraction continuation, multi-witness deduplication, and explicit direct police observation;
- explicit crime knowledge, suspect attribution, reported/last-known position,
  confidence/search state, district police pressure, and no omniscient hidden tracking;
- the eight states `CLEAR`, `SUSPICION`, `REPORTED`, `SEARCHING`, `LOCATED`,
  `PURSUIT`, `ESCAPED_PENDING`, `COOLDOWN`, with a guarded transition table and
  integer deadlines;
- deterministic semantic responders as normal persistent NPCs, abstract dispatch,
  streaming-owned bodies, Character AI search/pursuit, CombatSystem attacks, and
  arrest that advances the normal world simulation exactly 120 game days;
- headless `faultline_police`, independent-process save-byte comparison, real
  100-seed stress, focused persistence/FSM tests, and documentation.

The generated data model has no police-station semantic object. Arrest releases the
player at a deterministic district-boundary station-exterior proxy. This is the
smallest Phase 20-compatible limitation; no vehicle/cinematic system was invented.

## Versions and historical contracts

- GameVersion 0.1.0
- GeneratorVersion 3
- SaveFormatVersion 13
- StateHash `FAULTLINE.state.v15`
- PoliceStateVersion 1
- History, StoryGraph, MissionSimulation, PrologueState, PlayerState, CombatState: 1

Historical seed-12345 BaseWorldHashes must remain:

- v1 `2938f0ca42162bc9`
- v2 `5f3a446fe0d2f974`
- v3 `888c5dbd10c15886`

V13 is justified by pending reports, knowledge, Wanted deadlines/last-known position,
responders, pressure, and arrest state. V1-v12 migrate to neutral PoliceState without
gameplay RNG or invented facts. The authentic v12 fixture was frozen before writer
changes at `tests/fixtures/save_v12_phase19.flsave`: seed 12345, tick 15, StateHash
v14 `f83aa033ca06ffd4`, 972142 bytes, SHA-256
`681b6a97ec9d74bd183dbc8a5f1c5177069a48c534b8ece035e2c77e9e34cefc`.
Its exact provenance is in `tests/fixtures/README.md`.

## Tick order and determinism traps

Tick order is: restore residency; strategic systems/commits; player movement and
streaming; ordinary Character AI; existing report/FSM deadlines; police response
movement/perception; Combat; Combat-to-crime ingestion; witness perception; new
report/FSM processing; downstream death observation; interaction intent; clock
advance; arrest resolution. New response cannot recursively act in the same tick.

Do not full-scan semantic NPCs at 20 Hz, persist witness EntityId, read hidden player
Transform as police knowledge, use wall-clock timers, apply damage in AI, duplicate
responder bodies, consume EntityIds during lease reconstruction, exclude authoritative
future state from hashing, fabricate old fixtures, or regenerate BaseWorldHash.

## Verification at handoff

- Focused police: 19 cases / 1080 assertions PASS.
- Independent process determinism PASS; v13 save SHA-256
  `fa99aed265bbb2a71d0a2f5974fea2d9d83e1853c78bf2b0f2f6b59266eb25b1`.
- Fast regression: 26/26 PASS in 16.28 seconds, including Map2D.
- Final 100-seed stress: 708 crimes (608 witnessed/100 unwitnessed), 2324 reports,
  488 Wanted transitions (100 search/96 pursuit), 300 responses, 96 reacquisitions,
  zero duplicate facts,
  save mismatches, continuation mismatches, or StateHash mismatches.
- Canonical Release: 39/39 CTest PASS in 171.39 seconds; 326 doctest cases /
  1,143,209 assertions; no compiler warnings/errors.

## Files to review first

- `docs/WITNESS_POLICE.md`
- `docs/SAVE_FORMAT_V13.md`
- `docs/PHASE20_VERIFICATION.md`
- `src/simulation/police/police_state.hpp`
- `src/simulation/police/police_system.cpp`
- `src/simulation/runtime/game.cpp`
- `src/persistence/save.cpp`
- `tests/police_tests.cpp`
- `tools/police/main.cpp`

Run the canonical final gate with:

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

Stop after Phase 20 review. Do not implement vehicles, police cars, pursuits using
vehicles, helicopters, roadblocks, SWAT, advanced tactics/forensics/disguise/HUD, or
any other Phase 21/post-V1 scope.
