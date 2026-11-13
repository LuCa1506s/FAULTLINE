# Phase 16 — Primary 001 — FIRST DELIVERY

## Goal
Implement the canonical prologue mission and transition into the four-year-later sandbox.

## Canonical documents to read
- `..\MISSION_GRAPH_SPEC.md`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\WORLD_GENERATION_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement PRIMARY 001 FIRST DELIVERY using the generic MissionSystem, not bespoke world hacks.
- Seed/world context chooses a procedural discovery incident; at least one rival must discover the fictional drug before the transition.
- Keep the fictional product abstract and non-instructional.
- After every valid prologue resolution permitted by canon, trigger the structural four-year transition through the strategic history mechanism.
- Enter the post-timeskip sandbox with all five factions participating in unstable equilibrium.
- Save/load and deterministic continuation must work before, during and after the prologue transition.
- Add end-to-end deterministic tests for multiple seeds and supported failure/alternate-resolution paths.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE16_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- cinematic dialogue system
- real drug detail
- future advanced mission features unless strictly required
- Phase 17 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
