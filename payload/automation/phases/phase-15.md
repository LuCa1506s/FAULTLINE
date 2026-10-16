# Phase 15 — Mission Core

## Goal
Implement the canonical MissionSystem and objective graphs.

## Canonical documents to read
- `..\MISSION_GRAPH_SPEC.md`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\entity_domain_data_model.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Implement exactly the canonical mission states AVAILABLE, ACCEPTED, ACTIVE, COMPLETED, FAILED, EXPIRED, INVALIDATED, RESOLVED_BY_NPC.
- Separate MissionTemplate/definition data from runtime MissionInstance state.
- Implement serializable objective graphs with stable-ID conditions/transitions and basic GO_TO, INTERACT, DELIVER, PROTECT, ATTACK objective families as appropriate to current headless infrastructure.
- Progress missions from domain events/world queries; missions never overwrite world truth or directly own territory/police/faction authority.
- Support failure consequences, expiration, invalidation and NPC/world resolution while ignored.
- Persist/migrate mission state; deterministic save-mid-mission continuation must pass.
- Add graph validation against unreachable mandatory paths and invalid references.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE15_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- FIRST DELIVERY-specific content beyond hooks
- advanced mission mutation/secret issuers/favors
- renderer/UI
- Phase 16 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
