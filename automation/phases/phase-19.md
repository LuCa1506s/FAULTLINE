# Phase 19 — Combat

## Goal
Implement the minimal deterministic V1 combat loop.

## Canonical documents to read
- `..\GDD_FAULTLINE_v0.1.md`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\entity_domain_data_model.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement pistol, shotgun and SMG definitions plus aim/fire/ammo/reload/damage/death foundations needed by V1.
- Keep definitions separate from runtime item/weapon instances.
- Integrate character health/death and faction/NPC semantic consequences through domain events.
- Important NPC soft narrative protection applies only to low-level abstract randomness, not physical invulnerability.
- Keep combat authoritative and renderer-independent.
- Add deterministic headless combat tests, save/load tests and regression coverage for death/cleanup/resource state.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE19_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- complex weapon customization
- advanced ballistics unless required
- gore/visual effects authority
- police/witness implementation
- Phase 20 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
