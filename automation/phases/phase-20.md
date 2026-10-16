# Phase 20 — Witness + Police

## Goal
Implement systemic crime perception/reporting, wanted/search/pursuit and arrest.

## Canonical documents to read
- `..\POLICE_WANTED_STATE_MACHINE_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Keep crime occurrence, witnessing, reporting, suspect identification and police knowledge distinct.
- Implement witness perception/report flow and canonical wanted/search state machine from the spec.
- Police use last-known information, not omniscient player position.
- Support abstract distant police response and relevant physical materialization.
- Implement arrest as a world-time-advancing consequence, not hard game-over.
- Connect district police pressure to already-defined strategic hooks without inventing authority outside PoliceSystem.
- Persist/migrate wanted/search state; deterministic escape/reacquisition/arrest and save-mid-search tests must pass.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE20_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- full forensic simulator
- surveillance systems not already specified
- mission authority over police truth
- Phase 21 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
