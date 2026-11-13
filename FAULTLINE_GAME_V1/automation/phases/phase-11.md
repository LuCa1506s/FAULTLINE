# Phase 11 — Territory System

## Goal
Make runtime territory control emerge from authoritative faction presence and Phase 9 attack/defense consequences.

## Canonical documents to read
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\entity_domain_data_model.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement CONTROLLED / CONTESTED / UNCONTROLLED with deterministic, configurable thresholds and sensible hysteresis.
- Keep generated territory geometry separate from persistent runtime presence/control.
- Initialize runtime control from the Phase 7 generated map without changing BaseWorldHash.
- Consume attack/defense results as presence changes; FactionSystem must never directly flip territory ownership.
- Make Phase 9 targeting read current runtime control, not stale generated ownership.
- Persist/migrate new runtime territory state and include it in StateHash.
- Demonstrate repeated causal pressure can move A-controlled -> contested -> B-controlled and neutral territory can be claimed.
- Run 30-day and 365-day headless tests plus current batch/determinism/persistence regressions.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE11_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- market/dealer economics
- faction collapse
- dynamic diplomacy
- story/missions
- physical gameplay
- Phase 12 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
