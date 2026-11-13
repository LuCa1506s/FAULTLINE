# Phase 12 — Market M1

## Goal
Implement the abstract fictional district market and the first sustainable faction revenue loop.

## Canonical documents to read
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Add district demand, supply, bounded price, dealer capacity and faction market share using abstract strategic units only.
- Keep territory control distinct from market control.
- Integrate existing Dealer roles without realistic drug quantities, production, consumption or trafficking instructions.
- Create faction income from market participation and connect it to authoritative treasury without duplicate accounting.
- Use coarse deterministic market scheduling and damped/bounded price response.
- Persist/migrate market runtime state and extend StateHash.
- Test dealer loss/disruption, territory modifiers, bounded values, deterministic save continuation and multi-year headless stability.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE12_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- real-world drug mechanics
- story/mission generation
- advanced diplomacy/collapse
- physical dealer AI
- Phase 13 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
