# Phase 18 — Basic Character AI

## Goal
Add minimal physical AI for materialized characters without replacing semantic NPC simulation.

## Canonical documents to read
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\entity_domain_data_model.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement local perception/movement and minimal follow, flee, attack-intent and protect behaviors needed by V1 foundation.
- Keep physical AI distinct from semantic activity; materialization/abstractification preserves meaningful state.
- Use priorities survival -> assignment/operation -> combat objective -> role -> personal/ambient.
- Do not grant AI omniscient mission/world knowledge.
- Test materialize/abstractify identity preservation, local decisions, streaming boundaries and deterministic command outcomes.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE18_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- complex behavior-tree framework
- detailed citizen schedules
- advanced combat tactics
- witness/police system
- Phase 19 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
