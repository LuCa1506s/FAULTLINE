# Phase 14 — Story Graph Foundation

## Goal
Create the mutable high-level procedural story graph derived from world state.

## Canonical documents to read
- `..\MISSION_GRAPH_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Represent generated high-level story possibilities separately from per-mission objective graphs.
- Reference semantic stable IDs only and remain serializable/deterministic.
- Support prerequisites, branches, world-aware invalidation and repair/substitution after important NPC/world changes.
- Ensure story graph is mutable possibility, not destiny; world truth always wins.
- Provide deterministic generation/validation and save migration if persistent schema changes.
- Test major-NPC death repair, faction/world-state mutation and same-state reproducibility.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE14_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- full MissionSystem objective execution
- FIRST DELIVERY runtime
- UI/dialogue rendering
- Phase 15 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
