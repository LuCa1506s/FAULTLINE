# Phase 23 — Respect + Progression

## Goal
Implement canonical 0–100 player Respect, progression pacing foundations, leadership threshold and OG.

## Canonical documents to read
- `..\RESPECT_PROGRESSION_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Use centralized RespectEvent processing; other systems must not scatter direct respect edits.
- Keep global Respect separate from faction/personal relationships.
- Implement bounded impact classes, contextual scaling and diminishing returns for trivial repetition.
- Support meaningful but less-punitive losses from failure/death/arrest/strategic responsibility.
- Implement approximate leadership access around canonical ~65 without forcing management play.
- Implement first-reach-100 OG world achievement, historical max and recovery behavior per spec.
- Persist/migrate progression and test bounds, grind resistance, leadership and OG save/load.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE23_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- pay-to-win cross-world advantages
- final balance tuning (Phase 35)
- Phase 24 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
