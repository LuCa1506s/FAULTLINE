# Phase 29 — Controller + Aim Assist

## Goal
Add controller input and deterministic aim-assist behavior through the existing input abstraction.

## Canonical documents to read
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Map controller hardware only in platform/input adapter; gameplay consumes abstract actions/axes.
- Keep keyboard behavior intact.
- Implement bounded aim assist suitable for controller without changing world truth or granting impossible targets.
- Make aim-assist configuration testable and independent of renderer frame rate.
- Add deterministic input/aim selection tests.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE29_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- hardware key/button codes in gameplay
- renderer-dependent aiming
- Phase 30 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
