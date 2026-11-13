# Phase 35 — Balancing + V1 Gate

## Goal
Tune the now-stable V1 systems and prove the V1 definition of done.

## Canonical documents to read
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\RESPECT_PROGRESSION_SPEC.md`
- `..\POLICE_WANTED_STATE_MACHINE_SPEC.md`
- `..\MISSION_GRAPH_SPEC.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Tune Respect pacing against canonical speedrun/skilled/average/inexperienced targets without turning trivial grind optimal.
- Tune faction aggression/operation cadence, war duration, territory turnover, recruitment/collapse/reformation and anti-snowball costs using believable systemic costs.
- Tune market volatility, dealer significance, police pressure and mission rewards/availability using batch evidence.
- Verify canonical prologue, four-year transition, dynamic sandbox, branching/expiring missions, vehicles, combat, police, controller/aim assist, ASCII presentation, saves and achievements as one V1.
- Run final deterministic, persistence, stress, integration and performance gates and document known non-blocking limitations.
- Do not pull post-V1 features into release merely to polish scope.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE35_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- new feature expansion
- post-V1 vehicles/interiors/multiplayer/photorealism
- Phase 36 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
