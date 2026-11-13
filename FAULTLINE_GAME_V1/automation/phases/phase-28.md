# Phase 28 — HUD

## Goal
Implement renderer-independent semantic HUD data and the ASCII HUD.

## Canonical documents to read
- `..\RENDERER_INTERFACE_SPEC.md`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Expose presentation-safe HUD state for health, weapon/ammo, wanted, witnesses, mission/dialogue status, map, Public Suspicion if present, Respect, faction/territory notifications.
- Keep HUD data extraction separate from terminal coordinates/glyphs.
- Notifications derive from real domain events and cannot mutate gameplay.
- Ensure different terminal sizes degrade gracefully.
- Test HUD snapshot correctness independently of ASCII rendering.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE28_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- gameplay authority in UI
- cinematic dialogue system
- Phase 29 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
