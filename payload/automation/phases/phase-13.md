# Phase 13 — Four-Year Strategic History

## Goal
Implement deterministic compressed strategic history capable of producing the canonical post-timeskip unstable five-faction world.

## Canonical documents to read
- `..\WORLD_GENERATION_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\GDD_FAULTLINE_v0.1.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Also read `AGENTS.md`, `DEVELOPMENT_STATUS.md`, current save/version documentation, and all source/tests touched by this phase.

## Required work / Definition of Done
- Use strategic/coarse simulation, never 20 Hz for four years.
- Produce coherent territorial, faction, dealer/market and relationship evolution from explicit causes.
- Ensure all five factions participate in the fictional market after the history interval and remain viable in an unstable equilibrium.
- Store compressed meaningful history, not every simulation tick.
- Preserve the canonical FIRST DELIVERY -> rival discovery -> four-year structural transition; implement history machinery without rewriting that story rule.
- Keep results deterministic and versioned according to the existing architecture.
- Mass-test seeds for impossible dominance, dead-on-arrival factions, invalid references and reproducibility.

In addition:
- build with the canonical `tools/build.ps1`;
- run relevant fast/unit/integration/determinism/persistence tests;
- run the phase-specific stress/long-run/mass-seed gates implied above and by the roadmap;
- preserve all prior BaseWorldHash/version fixtures unless this phase explicitly and canonically changes worldgen;
- preserve backward save compatibility required by the current persistence policy;
- update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and `docs/PHASE13_VERIFICATION.md`;
- stop after this phase.

## Explicitly out of scope
- runtime FIRST DELIVERY mission implementation (Phase 16)
- full story graph
- physical gameplay
- Phase 14 implementation.

## Block instead of guessing
If this phase exposes a real contradiction between canonical documents, or satisfying it requires changing a canonical rule, report a blocker. Do not silently revise canon.
