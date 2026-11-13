# Phase 30 — Save Hardening

## Goal
Harden persistence for real V1 use.

## Canonical documents to read
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Add atomic temporary-write + replace semantics appropriate to Windows/current platform abstraction.
- Add integrity/checksum validation and clear corrupt-save diagnostics.
- Complete/clean migration framework for all SaveFormatVersions created so far.
- Add debug dump/inspection support and controlled autosave points.
- Test interrupted/truncated/corrupt writes, version migration, atomic replacement behavior and deterministic continuation.
- Do not serialize transient caches merely for speed.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE30_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- cloud saves
- platform account sync
- rewriting semantic persistence model
- Phase 31 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
