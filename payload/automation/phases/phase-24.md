# Phase 24 — Faction Advanced

## Goal
Complete advanced systemic faction behavior on top of territory, market, NPC and Respect foundations.

## Canonical documents to read
- `..\FACTION_SIMULATION_SPEC.md`
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\RESPECT_PROGRESSION_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Add recruitment, market-aware strategy, dealer protection/attack, alliances, war/retaliation, collapse/dormancy/reformation and succession/internal effects required by canonical spec.
- Use real resources, grievances, relationships and costs; no magic comeback buffs or arbitrary rubber-banding.
- Player leadership orders use the same operation/resource system and remain optional.
- Zero territory alone must not instantly delete a faction.
- Local gang/reformation architecture should fit the same system where required without exploding V1 scope.
- Persist/migrate new state and run multi-year headless/batch stability tests.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE24_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- fake anti-snowball modifiers
- management-only gameplay
- future multiplayer
- Phase 25 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
