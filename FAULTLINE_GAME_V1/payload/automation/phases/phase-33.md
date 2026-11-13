# Phase 33 — Long-Run Simulation

## Goal
Stress the complete headless world across many seeds and years.

## Canonical documents to read
- `..\V1_IMPLEMENTATION_ROADMAP.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\MISSION_GRAPH_SPEC.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Run thousands of seeds where practical and multi-year simulations with deterministic reproduction of failures.
- Collect faction collapse/reformation, war frequency, territory turnover, market stability, mission validity, NPC/reference integrity and invalid-state statistics.
- Fix structural bugs discovered and add regression seeds.
- Separate statistical observations from hard invariants; do not balance by hiding failures.
- Produce machine-readable and human-readable long-run reports.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE33_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- premature final balancing based on one seed
- weakening invariants to pass stress
- Phase 34 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
