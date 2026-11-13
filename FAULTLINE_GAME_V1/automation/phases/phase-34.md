# Phase 34 — Performance Pass

## Goal
Profile the integrated V1 and optimize measured bottlenecks without changing semantics.

## Canonical documents to read
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Measure simulation tick cost, active NPC/AI cost, spatial queries, streaming/materialization, renderer, save/load size/time and worldgen.
- Preserve deterministic outcomes and all existing tests while optimizing.
- Follow order: profile -> algorithmic pathology -> lower justified update frequency -> abstraction -> data layout -> parallelism only if proven necessary.
- Add repeatable benchmarks/profiling documentation.
- Do not introduce complex multithreading without measured need and deterministic merge design.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE34_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- speculative micro-optimization
- semantic changes disguised as optimization
- Phase 35 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
