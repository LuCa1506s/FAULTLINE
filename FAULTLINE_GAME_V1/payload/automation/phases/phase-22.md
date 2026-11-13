# Phase 22 — Player Home + Garage

## Goal
Add minimal persistent player home/garage and owned-vehicle storage.

## Canonical documents to read
- `..\GDD_FAULTLINE_v0.1.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\entity_domain_data_model.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Represent player home and garage as semantic persistent locations/references using generated buildings/world data.
- Support storing/retrieving owned vehicles without identity duplication.
- Integrate death/arrest relocation and save/load.
- Ensure streaming/materialization cannot duplicate stored vehicles.
- Add deterministic persistence and relocation tests.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE22_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- large interiors
- property economy
- advanced garage customization
- Phase 23 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
