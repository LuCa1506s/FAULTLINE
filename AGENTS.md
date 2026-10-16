<!-- FAULTLINE_AUTOPILOT_BEGIN -->
# FAULTLINE autonomous development contract

## Repository goal
This repository contains the purpose-built C++20 engine/game implementation for FAULTLINE, a deterministic procedural single-player crime sandbox.

## Canonical sources
The canonical design/specification files live in the parent directory of this repository. They are source-of-truth and MUST NOT be modified by coding agents.

Important canonical files include:
- `..\GDD_FAULTLINE_v0.1.md`
- `..\ENGINE_ARCHITECTURE_SPEC.md`
- `..\ENGINE_CORE_RUNTIME_SPECIFICATION.md`
- `..\entity_domain_data_model.md`
- `..\WORLD_SPATIAL_STREAMING_SPEC.md`
- `..\WORLD_GENERATION_SPEC.md`
- `..\FACTION_SIMULATION_SPEC.md`
- `..\NPC_DATA_MODEL_AND_SIMULATION_SPEC.md.txt`
- `..\MISSION_GRAPH_SPEC.md`
- `..\DRUG_MARKET_SIMULATION_SPEC.md`
- `..\RESPECT_PROGRESSION_SPEC.md`
- `..\POLICE_WANTED_STATE_MACHINE_SPEC.md`
- `..\GAME_STATE_SAVE_FORMAT_SPEC.md`
- `..\RENDERER_INTERFACE_SPEC.md`
- `..\V1_IMPLEMENTATION_ROADMAP.md`

Before implementing a phase, read the phase file under `automation/phases/`, `DEVELOPMENT_STATUS.md`, and the relevant canonical specs.

If two canonical documents materially conflict:
1. do not silently invent a new rule;
2. prefer the GDD and the most conservative compatible interpretation for investigation only;
3. do not change canon;
4. report the conflict as a blocker so the orchestrator stops.

## Non-negotiable architecture
- C++20 + CMake.
- Simulation is authoritative and headless-first.
- Fixed simulation timestep baseline: 20 Hz / 50 ms.
- Renderer/input/platform never determine gameplay truth.
- Typed stable IDs; persistent semantic IDs are not interchangeable.
- `NpcId != EntityId`.
- Composition over deep inheritance.
- Faction/Territory/District/Mission/Market/StoryGraph are semantic domain objects, not forced into the entity registry.
- No global gameplay RNG. Deterministic named/sub-seeded streams only.
- Do not use `std::hash` as a persistent world-generation contract.
- Authoritative V1 simulation remains single-threaded unless a canonical spec explicitly says otherwise.
- Persistent state uses stable IDs, never raw pointers.
- Save semantic reality, not process memory/caches.
- Same supported deterministic state + inputs + RNG state must continue identically after save/load.
- Base world identity is seed + GeneratorVersion. Runtime changes do not alter BaseWorldHash.
- Mission state does not override world truth.
- Territory control must reflect actual systemic presence, not mission completion.
- Faction operations consume real strategic resources.
- Do not implement future phases early merely to make the current phase easier.

## Canonical game rules that must not drift
- Standard world: exactly five main factions generated from four canonical macro-families; exactly one family repeats.
- The central drug is completely fictional/abstract. Do not add real-world production, dosing, trafficking, consumption, or instructional details.
- Canonical prologue structure: FIRST DELIVERY -> at least one rival discovers the fictional drug through a procedural event -> structural four-year time skip -> five-faction unstable market equilibrium.
- The world continues when the player does nothing.
- Mission failure changes the world; it is not automatic game-over.
- Important NPCs have soft narrative protection against low-level abstract random death, not physical invulnerability.
- Leadership gameplay is optional and never replaces direct action.
- Death/arrest advance world time rather than hard-resetting the world.
- V1 is single-player.
- Do not pull post-V1 scope into a milestone unless required by an existing canonical requirement.

## Development discipline
For every phase:
1. inspect the real current tree and current code;
2. preserve valid prior work and backward compatibility required by specs;
3. implement the smallest coherent architecture satisfying the phase;
4. build;
5. run relevant fast/unit/integration/determinism/persistence tests;
6. run the phase's stress/mass-seed/long-run gates where required;
7. fix failures rather than bypassing them;
8. update `DEVELOPMENT_STATUS.md`, `docs/FILE_INDEX.md`, and a phase verification report;
9. stop at the end of the requested phase.

Do not claim a command/test passed unless you actually ran it.
Do not replace failing tests with weaker assertions merely to get green.
Do not hide essential missing behavior behind TODOs/stubs.
A deterministic bug should become a regression test whenever practical.

## Build
Canonical build entry point:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`

Keep project warnings clean.

## Persistence/version policy
- `GeneratorVersion` changes only when canonical generated base-world output changes.
- `SaveFormatVersion` changes when persistent serialized schema changes and must preserve required older-version migration paths.
- `GameVersion` follows repository policy and is independent of both.
- Historical BaseWorldHash fixtures must remain unchanged unless a canonical worldgen version intentionally changes.

## Automation
The local orchestrator under `automation/` drives one phase at a time.
When invoked by it:
- perform real repository edits;
- stay inside the requested phase;
- produce the structured result requested by the orchestrator;
- a reviewer may reject the phase and issue a repair task;
- never advance yourself to the next phase.
<!-- FAULTLINE_AUTOPILOT_END -->

