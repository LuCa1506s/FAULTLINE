# Phase 21 — Vehicles

## Goal
Implement simple V1 cars and persistent vehicle identity.

## Canonical documents to read
- `..\entity_domain_data_model.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement civilian and police cars, persistent VehicleId distinct from active entity, enter/exit and basic deterministic driving.
- Integrate road/spatial/streaming systems and abstract distant vehicle state where appropriate.
- Support simple ownership and save/load identity preservation.
- Keep architecture extensible to future vehicle categories without implementing motorcycles/boats/aircraft now.
- Test spawn/materialize/drive/abstractify/save/load and police-car compatibility.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE21_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- advanced vehicle tuning
- motorcycles
- boats
- aircraft
- renderer-specific vehicle physics
- Phase 22 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
