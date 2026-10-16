# Phase 31 — Achievements

## Goal
Implement world/global achievement architecture including canonical OG semantics.

## Canonical documents to read
- `..\GDD_FAULTLINE_v0.1.md`
- `..\RESPECT_PROGRESSION_SPEC.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Separate seed/world achievements from global profile achievements.
- Ensure OG unlock integrates with Respect and remains unlocked for that world after Respect falls.
- Global achievements must not provide major new-world gameplay advantage.
- Persist/version achievement/profile data cleanly and test duplicate/unlock/save/load behavior.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE31_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- platform-specific achievement SDK unless already required
- pay-to-win bonuses
- Phase 32 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
