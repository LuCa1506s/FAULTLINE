# Phase 32 — Systemic Integration Test

## Goal
Prove that the connected systems produce genuine causal world changes.

## Canonical documents to read
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement automated integration scenarios equivalent to: dealer harmed/removed -> market weakens -> faction reacts -> rival acts -> territory can change -> mission/story opportunity reacts -> police can respond -> save/load remains coherent.
- Use real systems, not test-only shortcuts that bypass authorities.
- Add regression scenarios for ignored player involvement and autonomous world progression.
- Run full build/test/persistence/determinism gates and document causal event traces.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE32_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- new major features unless a concrete integration gap requires the smallest fix
- Phase 33 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
