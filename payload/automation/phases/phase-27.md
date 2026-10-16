# Phase 27 — ASCII World Visualization

## Goal
Render the playable generated world coherently in the ASCII frontend.

## Canonical documents to read
- `..\RENDERER_INTERFACE_SPEC.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\entity_domain_data_model.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Render roads, buildings, characters, vehicles, doors/weapons/mission objects that already exist semantically.
- Use presentation-side culling/caching only; it must never become simulation perception/authority.
- Prioritize spatial readability and object recognition over decoration.
- Add debug overlays for IDs/cells/streaming/territory/AI where useful.
- Profile enough to avoid pathological terminal redraw behavior without premature engine-wide optimization.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE27_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- HUD feature set
- gameplay logic in glyph code
- new simulation features merely for visuals
- Phase 28 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
