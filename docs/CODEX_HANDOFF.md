# FAULTLINE CODEX HANDOFF

## 1. Repository

- Repository: `C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1`
- Branch: `master`
- HEAD when this handoff was written: `bbc4450 Tools: Add and audit deterministic 2D debug map viewer`
- Previous canonical gameplay commit: `33f798a Phase 19: Combat`
- The working tree was clean before this file was created. After creation,
  `docs/CODEX_HANDOFF.md` is the only expected untracked file.
- The Map2D viewer is committed at `bbc4450`; it is no longer inherited dirty work.

Always inspect `git status --short`, `git diff --check`, `git diff --stat`, and
`git log -10 --oneline --decorate` before editing. If future work is dirty, do not
reset, clean, stash, checkout over, or discard it. Determine its provenance first.

## 2. Canonical source documents

Canonical design lives in `C:\Users\admin\Documents\FAULTLINE`, one directory
above the repository. These files are read-only unless the user explicitly decides
to change canon:

- `GDD_FAULTLINE_v0.1.md`
- `ENGINE_ARCHITECTURE_SPEC.md`
- `ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `entity_domain_data_model.md`
- `WORLD_SPATIAL_STREAMING_SPEC.md`
- `WORLD_GENERATION_SPEC.md`
- `FACTION_SIMULATION_SPEC.md`
- `NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `MISSION_GRAPH_SPEC.md`
- `DRUG_MARKET_SIMULATION_SPEC.md`
- `RESPECT_PROGRESSION_SPEC.md`
- `POLICE_WANTED_STATE_MACHINE_SPEC.md`
- `GAME_STATE_SAVE_FORMAT_SPEC.md`
- `RENDERER_INTERFACE_SPEC.md`
- `V1_IMPLEMENTATION_ROADMAP.md`

Also read the repository’s `AGENTS.md` and the requested phase contract under
`automation/phases/`. If canonical documents materially conflict, do not invent a
rule or edit canon; report the conflict as a blocker.

## 3. Core architecture invariants

- C++20 and CMake; canonical build entry point is `tools/build.ps1`.
- Authoritative simulation runs at fixed 20 Hz / 50 ms. Wall clock, render frame
  time, and accelerated game time never control physical movement.
- Authority flows as `INPUT -> COMMANDS -> SIMULATION -> WORLD STATE -> SNAPSHOT -> RENDERER`.
  Renderer/platform code never decides hits, movement, police knowledge, territory
  control, mission outcomes, or other gameplay truth.
- Authoritative V1 simulation is single-threaded. Processing order and tie-breaking
  use stable IDs, never pointer addresses or unordered-container iteration.
- Gameplay randomness uses explicit deterministic domain streams/sub-seeds. Never use
  `rand()`, `random_device`, a global gameplay RNG, wall clock, or `std::hash` as a
  persistent generation contract.
- IDs are strong typed IDs. `NpcId != EntityId`: `NpcId` is persistent semantic
  identity; `EntityId` is a transient physical body identity.
- Streaming owns residency, materialization, and abstraction. Semantic NPC simulation
  continues when no physical body exists. Physical systems may act only on legitimately
  active bodies and must not materialize arbitrary targets.
- `GameState` contains persistent authoritative reality. `Simulation` transforms it.
  `Game` is the session/orchestration facade. Application/platform loops remain outside
  authoritative rules.
- Structural mutations are deferred to explicit commit boundaries. Cross-domain
  consequences use explicit ordered domain-event buffers, not a magical global bus.
- Factions, territories, districts, markets, missions, and StoryGraph are semantic
  domain objects, not forced into the EntityRegistry.
- Persistence saves semantic authority and required allocator/RNG state, not raw
  pointers, renderer state, OS handles, spatial caches, path-search scratch, or
  transient process layout. Derived indexes are rebuilt deterministically.
- Historical fixtures must come from authentic historical writers. Never fabricate an
  older save by patching a newer header, truncating sections, or renaming newer bytes.
- Generated base-world identity derives deterministically from seed + GeneratorVersion.
  Runtime systems do not alter `BaseWorldHash`.

## 4. Canonical game-design invariants

- A standard world has exactly five main factions generated from four canonical
  macrofamilies; exactly one family repeats.
- The central drug/product is completely fictional and abstract. Do not add real-world
  production, dosing, trafficking, consumption, or instructional detail.
- Canonical prologue: `FIRST DELIVERY -> procedural rival discovery -> exactly four
  years / 1460 days of structural history -> unstable five-faction sandbox`.
- The world continues when the player does nothing. Mission failure changes world state;
  it is not automatic game-over.
- Territory control reflects actual systemic presence. Missions never overwrite
  TerritorySystem truth, and faction operations consume real strategic resources.
- Important NPCs have soft protection from low-level abstract random death, not physical
  invulnerability. Their death must permit deterministic succession, StoryGraph repair,
  and mission invalidation/repair without resurrection or free replacement actors.
- Death/arrest advances world time rather than resetting the world. Arrest implementation
  belongs to Phase 20.
- Mission Core has exactly eight states: `AVAILABLE`, `ACCEPTED`, `ACTIVE`, `COMPLETED`,
  `FAILED`, `EXPIRED`, `INVALIDATED`, `RESOLVED_BY_NPC`.
- V1 is single-player. Multiplayer, advanced squad tactics, advanced weapon customization,
  a general-purpose engine, and post-V1 scope must not be pulled into current phases.

## 5. Completed phases

- **Phases 0-4:** repository/core/runtime/entity/domain/spatial foundations: typed IDs,
  deterministic hashing/RNG, SimulationClock, GameState/Simulation, ordered events and
  commands, domain stores, WorldPosition, SpatialGrid, and streaming interfaces.
- **Phases 5-7:** Worldgen M1-M3: geography/roads/districts, parcels/buildings/territories,
  five factions, major NPCs, initial relationships and territory assignments. Current
  GeneratorVersion is 3.
- **Phase 8:** semantic save foundation, explicit little-endian versioned persistence,
  StateHash, RNG/allocator continuation, malformed-input rejection.
- **Phases 9-12:** headless faction operations, semantic NPC simulation and promotion,
  authoritative territory presence/control, and Market M1.
- **Phase 13** (`1d5c2e7`): deterministic 48-month/exactly-1460-day strategic history,
  HistoryVersion 1, compressed meaningful records, resumable byte-identical continuation.
- **Phase 14:** persistent high-level StoryGraph possibilities derived from post-history
  world truth, with dependency validation and repair/invalidation. It remains distinct
  from Mission Core.
- **Phase 15** (`e60833b`, after earlier Phase-15 work at `0aab7c4`): generic Mission Core,
  objective graph, event-driven progression, eight canonical mission states,
  StoryGraph bridge, persistence and deterministic continuation.
- **Phase 16** (`ef85393`, combined Phase 16-17 commit): PRIMARY 001 FIRST DELIVERY,
  procedural rival discovery and the structural four-year transition. It uses Mission
  Core and does not fabricate physical actors during the headless prologue.
- **Phase 17** (`ef85393`): authoritative physical player Entity with Transform, Health,
  CharacterBody, semantic input, 5 m/s fixed-tick movement, WorldPosition normalization,
  swept static collision/sliding, interaction intent, and deterministic streaming leases.
- **Phase 18** (`76d0b26`): active-only Basic Character AI. Physical NPC intent reuses
  `world::moveCharacter`; NPC speed is 3.5 m/s; CharacterBody owns facing; semantic NPC
  simulation remains authoritative when abstract. No combat was implemented here.
- **Phase 19** (`33f798a`): authoritative CombatSystem, player/NPC attack intents, pistol/
  shotgun/SMG definitions, global range, static line of sight, deterministic damage,
  persistent ammo/cooldown/reload, player terminal Health and semantic NPC death/
  succession. No Witness, Police, Wanted, or vehicle combat.
- **Map2D tool** (`bbc4450`): non-canonical, read-only deterministic SVG debug viewer,
  independently audited after Phase 19. It changes no gameplay or persistence schema.

## 6. Current versions

Verified from current source:

- `GameVersion = 0.1.0` (`currentGameVersion`)
- `GeneratorVersion = 3`
- `SaveFormatVersion = 12`
- StateHash schema string: `FAULTLINE.state.v14`
- `HistoryVersion = 1`
- `StoryGraphVersion = 1`
- `MissionSimulationVersion = 1`
- `PrologueStateVersion = 1`
- `PlayerStateVersion = 1`
- `CombatStateVersion = 1`

Historical seed-12345 BaseWorldHash contracts:

- v1 = `2938f0ca42162bc9`
- v2 = `5f3a446fe0d2f974`
- v3 = `888c5dbd10c15886`

Do not bump GeneratorVersion for runtime systems. Do not bump SaveFormatVersion or
StateHash schema automatically; first prove that new authoritative persisted/hashed
state is required.

## 7. Persistence fixtures

All `.flsave` files are binary (`*.flsave -text binary`). Provenance is documented in
`tests/fixtures/README.md`; trust that file and authentic writer history over filenames.

Fixture inventory includes v1, v2, v3, v4, v5, v6, two v7 history fixtures, v8, v9,
v10 and v11. Important recent fixtures:

- `tests/fixtures/save_v8_phase14.flsave`: authentic Phase 14 writer; format v8;
  1,006,078 bytes; SHA-256
  `df1574a61f4005a182a7bf8dec87e7fbca4993a8076dcae9df65a0c947376f08`.
- `tests/fixtures/save_v9_phase15.flsave`: authentic Phase 15 format-v9 fixture from
  the unmodified Phase 15 StoryGraph writer in a temporary worktree at `e60833b`;
  1,006,174 bytes; SHA-256
  `d376e7f56e4787685cacff4c64402f143ea2e5aff455868ec85f6a875c355c7c`.
- `tests/fixtures/save_v10_phase16.flsave`: emitted by the reconstructed authentic v10
  Phase 16 writer, not patched bytes; 1,007,684 bytes; SHA-256
  `86faa3bdbea5a9950590a0617f2f824c9cb5d765a52d910d0823c2d7dfa5fecd`.
  The reproducible source delta is `tests/fixtures/phase16_writer.patch`.
- `tests/fixtures/save_v11_phase18.flsave`: emitted before Phase 19 edits from clean
  commit `76d0b26` using `faultline_characters --seed 12345 --ticks 1200`; 970,887
  bytes; SHA-256
  `2ec6b2353547c8e15b84da24f149659fb2be3eb7d2ad5050ca8abbbfc2fc3019`.
- `tests/fixtures/save_v7_completed_history.flsave`: authentic completed-history v7
  writer fixture replacing an invalid synthetic truncation method; 1,000,894 bytes;
  SHA-256 `15272adffbbc6b3ff108583a7423106caf3e96e062f07d585d69d24bb09a32ce`.

There is no frozen v12 fixture currently required: v12 is the current writer. If Phase
20 introduces v13, first freeze an authentic v12 artifact using current commit code;
never derive it from v13 output.

## 8. Determinism / persistence lessons

- Phase 17 exposed a continuation divergence caused by transient NPC bodies being
  stripped at save while the uninterrupted branch retained a different physical
  residency shape. The fix persists logical `NpcResidency` allocation leases and
  reconstructs bodies before tick subsystems through `restorePlayerResidency`.
- Reconstruction reuses reserved EntityIds and must not consume new IDs. EntityRegistry
  allocator/high-water state remains persisted and hashed; do not remove it to hide a
  mismatch.
- `persistentSnapshot` synchronizes semantic NPC position/health and removes transient
  body representation. Semantic state plus leases remain authoritative.
- Physical player/NPC displacement uses exactly 50 ms per tick. A prior bug used
  accelerated game time (2250 ms) and made movement roughly 45 times too fast.
- Save/load continuation tests are strong contracts. On failure, compare original and
  restored branches one tick at a time and identify the first divergent field/system.
  Do not reduce them to immediate round-trip tests or exclude authoritative state from
  hashes merely to get green.
- Process determinism tests must execute the real behavior under test. Earlier misuse
  of unrelated tools produced false confidence; current player, Character AI, combat,
  and Map2D process gates run their real pipelines.
- Same semantic state must produce canonical save bytes. Sort semantic collections by
  stable ID; never rely on unordered iteration or transient cache layout.

## 9. Map2D debug viewer

Executable: `faultline_map2d`. Source: `tools/map2d/main.cpp`. Documentation:
`docs/MAP2D_DEBUG_VIEWER.md` and `docs/MAP2D_AUDIT.md`.

- The viewer is **non-authoritative**. It does not advance ticks, consume gameplay RNG,
  change an input save, or mutate gameplay authority.
- `--seed 12345` renders raw `GeneratedWorld`: player absent, active bodies absent, major
  semantic NPCs present.
- `--seed 12345 --runtime` constructs canonical `GameState`, creates the player through
  `createPlayerEntity`, then performs one zero-input streaming residency commit without
  advancing the clock. This creates real runtime markers, not debug fakes.
- `--load FILE` uses the real persistence layer and `restorePlayerResidency`; the viewer
  asserts semantic StateHash is unchanged by reconstruction.
- Player position comes from controlled-Entity Transform and facing from CharacterBody.
  Active NPC markers carry both NpcId and transient EntityId. Semantic major NPC markers
  use current runtime NpcState in load mode and distinguish dead actors.
- No canonical faction HQ coordinate exists. `homeTerritory` is not an HQ point; fake
  home-territory-centre “HQ” markers were removed.
- Runtime territory color/state comes from `territoryControl()`. Raw mode uses generated
  initial assignment. Missions and visual proximity never determine control.
- Labels use stable-ID ordering and a bounded deterministic candidate-offset placer.
- Transform for arbitrary origins/non-square worlds:
  `svgX = mapLeft + (worldX - worldMinX) * scale` and
  `svgY = mapTop + (worldZ - worldMinZ) * scale`, where scale is the minimum axis fit.
- Known limitations: static SVG; debug road widths rather than physical widths; no scale
  bar/north indicator; no HQ marker; six-position label placement with deterministic
  fallback; only canonically resident bodies are shown.

## 10. Current verification baseline

Latest verification on the committed Map2D/current gameplay tree:

- Full CTest: **36/36 PASS**, 155.96 seconds.
- All doctest executables: **307 cases, 1,142,129 assertions PASS**.
- Fast suite: **24/24 PASS**, 14.31 seconds.
- Map2D focused: **32 cases, 24,299 assertions PASS**; Map2D focused CTest 2/2 PASS.
- Map2D process gate validates actual emitted raw/runtime/v12-save SVG, emitted layer
  counts, CLI failures, layer switches, source-addressable player/NPC markers, diagnostic
  byte invariance, and cross-process byte identity.
- Phase 19 combat baseline before Map2D added 34 CTest gates, 275 doctest cases and
  1,117,830 assertions. The viewer added two CTest gates and 32 cases/24,299 assertions.

Canonical Release command:

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

Do not claim these results for a later modified tree without rerunning appropriate gates.

## 11. Known limitations / intentionally unfinished systems

- Static collision is ground-plane/building/water AABB collision; no general physics,
  dynamic crowd collision, interiors, slopes, ragdolls, or advanced navigation.
- Combat is foundational: no witness reporting, police response, Wanted FSM, cover AI,
  advanced tactics, or vehicle combat.
- No vehicle driving, Respect progression, ASCII renderer/HUD, or advanced mission phase
  has been implemented. These are roadmap omissions, not current regressions.
- The Map2D limitations are listed in section 9 and are debug-tool concerns, not canon.

## 12. Next canonical phase

**NEXT: Phase 20 — Witness + Police.** Do not start Phase 21 Vehicles.

Read `automation/phases/phase-20.md` and canonical
`..\POLICE_WANTED_STATE_MACHINE_SPEC.md` first, plus the NPC/runtime/save specs it cites.

Required conceptual pipeline:

```text
Crime Event
-> Witness
-> Report
-> Police Knowledge
-> Wanted FSM
-> Police Response
```

Important invariants:

- Police knowledge must arise from perception/reporting and last-known facts; no magical
  omniscience or direct access to the player’s current Transform without a valid source.
- Witness semantic identity uses NpcId. A physical police body uses transient EntityId.
- Streaming owns police materialization. Physical police movement should reuse the
  Character AI/movement foundation. Police damage must reuse CombatSystem/Health.
- Do not add vehicle systems or begin Phase 21.
- Arrest advances world time rather than resetting the world.
- Save/load mid-report/search/pursuit continuation and cross-process determinism are
  mandatory.
- Phase 20 will likely require persistent state, but a SaveFormat/StateHash bump must be
  justified from the implemented schema, not assumed in advance. If bumped, preserve an
  authentic v12 fixture generated by the actual current writer.

## 13. Recommended next-agent startup procedure

1. Read this handoff.
2. Run `git status --short`, `git diff --check`, `git diff --stat`, and
   `git log -10 --oneline --decorate`.
3. Read `AGENTS.md`.
4. Read current `DEVELOPMENT_STATUS.md`, `README.md`, `docs/FILE_INDEX.md`, and recent
   verification documents.
5. Read `automation/phases/phase-20.md` if Phase 20 is requested.
6. Read `..\POLICE_WANTED_STATE_MACHINE_SPEC.md` and its cited canonical specs.
7. Inspect actual runtime, NPC, Character AI, Combat, streaming, event, StateHash, and
   persistence implementations before designing or editing.
8. Verify every handoff version/result against current code.
9. Never trust this handoff over current code or canonical specifications.
10. Do not modify canonical parent specifications.

Use focused builds/tests during development; run the canonical Release command once the
phase is stable. Stop at the requested phase boundary.

## 14. Critical warnings

- Do not conflate NpcId with EntityId or persist transient physical identity as semantic
  actor identity.
- Do not let police, combat, missions, StoryGraph, renderer, or debug tools take authority
  from the system that owns the underlying fact.
- Do not let physical systems run on every semantic NPC at 20 Hz; only active resident
  bodies receive high-frequency execution.
- Do not use accelerated game time as physical `dt`; fixed physical `dt` is 50 ms.
- Do not weaken save/load continuation, authentic-fixture, StateHash, allocator, or
  process-determinism assertions to make a new phase pass.
- Do not regenerate or “repair” historical fixtures with the current writer.
- Do not fabricate player, active NPC, faction HQ, witness, report, or police knowledge
  for visualization/testing. Use repository-native initialization and real causal events.
- Do not change GeneratorVersion 3/worldgen output for runtime or aesthetic work. Any
  canonical worldgen change requires explicit review and a deliberate new generator version.
- Do not start future phases early merely to satisfy current tests.
