# Phase 17 — Player Physical Controller

## Goal
Create the player's physical controllable representation while preserving abstract input and headless authority.

## Canonical documents to read
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\entity_domain_data_model.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Create player entity/body state, deterministic movement, basic collision and interaction intent.
- Use abstract input actions/commands; gameplay code must not contain hardware key codes.
- Keep camera/presentation outside authoritative simulation.
- Integrate spatial streaming/materialization without making graphics authoritative.
- Provide headless command-driven movement tests, collision tests, save/load and deterministic replay/continuation tests.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE17_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- advanced combat
- vehicle driving
- renderer-owned gameplay
- controller-specific logic
- Phase 18 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
