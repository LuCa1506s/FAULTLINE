# Phase 26 — ASCII Renderer Foundation

## Goal
Implement replaceable 3D ASCII/ANSI presentation on top of RenderSnapshot.

## Canonical documents to read
- `..\RENDERER_INTERFACE_SPEC.md`
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement IRenderer/RenderSnapshot boundary or the repository's equivalent immutable presentation API.
- Implement NullRenderer/headless compatibility first.
- Add terminal framebuffer, projection, depth/z handling and basic semantic glyph mapping.
- Renderer cannot mutate GameState or determine AI perception/collision/gameplay.
- Keep terminal/ANSI/platform types outside core gameplay.
- Test snapshot extraction and renderer replacement/headless operation.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE26_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- HUD (Phase 28)
- renderer-owned gameplay
- photorealistic/general-purpose rendering engine
- Phase 27 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
