# Phase 25 — Mission Advanced

## Goal
Complete dynamic mission behavior and world integration.

## Canonical documents to read
- `..\MISSION_GRAPH_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Add expiration, RESOLVED_BY_NPC flows, dynamic causal mutation, secret issuers and favors/debts as specified.
- Generate missions from real faction operations, market pressure, NPC needs, police/story/world state.
- Prevent mission flooding with deterministic eligibility/scoring/budgets.
- Ensure ignored missions never freeze underlying world operations.
- Repair/invalidate missions coherently when referenced NPC/world facts change.
- Persist/migrate and test save/load during mutation, hidden issuer reveal, expiration and NPC resolution.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE25_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- UI-centric mission hacks
- mission-created fake territory/police truth
- Phase 26 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
