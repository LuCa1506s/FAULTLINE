# Phase 7 verification — 2026-10-16

Verified checkpoint: Worldgen M3 / generator version 3. Phase 8 is not started.

## Executed commands

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
./build/faultline_worldgen.exe --seed 12345 --validate --dump-factions
./build/faultline_worldgen.exe --seed 12345 --validate --dump-npcs
```

The build script compiled the C++20 Debug changes with GCC 16.2.0 and Ninja,
with no remaining project warnings, and executed the full CTest suite. Earlier
conversion warnings in the new validator were corrected before this final run.
`git diff --check` also completed without whitespace errors.

Final CTest: 4/4 PASS, 150.32 s total:

| Test | Result | Seconds |
|---|---|---:|
| foundation_tests | PASS | 10.34 |
| worldgen_checkpoint | PASS | 0.26 |
| worldgen_mass_1000 | PASS | 138.65 |
| worldgen_process_determinism | PASS | 1.06 |

Doctest: 49 cases, 210,838 assertions, all passed. Tests include reference hashes,
whole actor result reproducibility, independent stage calls, NPC stream changes,
family duplication variation, bounded name collision fallback, 15–25 NPCs,
viability, detached territory islands, excessive ownership, malformed actors,
relationships, runtime imports and streaming identity/health continuity.

The cross-process CTest launches two separate processes for each of versions 1,
2 and 3 and compares complete output, requiring both exit codes zero. The same
six-process comparison was executed again after the full suite using Python
subprocesses, with the expected hash checked for each version:

| Version | Seed | BaseWorldHash | Result |
|---|---:|---|---|
| 1 | 12345 | 2938f0ca42162bc9 | Preserved; identical outputs |
| 2 | 12345 | 5f3a446fe0d2f974 | Preserved; identical outputs |
| 3 | 12345 | 888c5dbd10c15886 | Identical outputs |

## Seed 12345

```text
FAULTLINE WORLD GENERATOR (M3)

Seed: 12345
Generator Version: 3
World: 8000 x 8000 m
WaterRegions: 1
MacroZones: 8
RoadNodes: 210
RoadEdges: 391
Districts: 18
Parcels: 8007
Buildings: 8007
Territories: 41
Factions: 5
MajorNPCs: 22
Validation: PASS
BaseWorldHash: 888c5dbd10c15886
```

| ID | Name | Canonical family | Home territory | Leader | Major NPCs | Treasury | Total manpower |
|---:|---|---|---:|---|---:|---:|---:|
| 1 | Kuroda Syndicate | Asian criminal organization / Yakuza-like | 35 | #1 Takumi Kuroda | 5 | 203801 | 92 |
| 2 | Eastside Brotherhood | African-American street gang | 1 | #6 Damon Johnson | 4 | 199402 | 87 |
| 3 | Vitale Syndicate | Italian Mafia | 6 | #10 Antonio Conti | 5 | 197103 | 115 |
| 4 | Kuroda Consortium | Asian criminal organization / Yakuza-like | 40 | #15 Daichi Mizuno | 5 | 208604 | 109 |
| 5 | Cumbre Cartel | Cartel | 18 | #20 Rafael Ortega | 3 | 203705 | 95 |

## Actual 1000-seed run

`faultline_worldgen --range 1 1000 --validate` ran inside CTest. All 1000 worlds
terminated, validated five main factions, valid homes/leaders, geometry and
relationships. Zero failed seeds or crashes. CLI-measured elapsed time was
138.641 s, or 138.641 ms/seed; CTest wall time was 138.65 s. This is an indicative
Debug batch measurement, not a cross-session performance guarantee.

Duplicated-family frequencies:

| Family | Worlds |
|---|---:|
| African-American street gang | 246 |
| Italian Mafia | 251 |
| Asian criminal organization / Yakuza-like | 262 |
| Cartel | 241 |

Major NPC count distribution:

| Count | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Worlds | 7 | 16 | 61 | 124 | 184 | 191 | 204 | 115 | 74 | 20 | 4 |

Individual faction territory share over all 5000 generated factions ranged from
535 to 2350 basis points: 5.35%–23.50% of city territory area. Neutral territory
is permitted. All factions passed the explicit viability/connected-presence check.

Directed faction relationship summaries (20,000 total): FRIENDLY 925, NEUTRAL
7849, TENSE 10148, HOSTILE 1078. No war simulation or four-year history ran.

## Algorithms, scope and next task

See WORLDGEN_M3.md for the family shuffle, geography-aware home selection,
resource-funded connected growth, per-faction NPC generation and causal directed
relationships. All previous geometry is preserved. Limits include a finite name
catalogue, aggregate garrisons, soft home affinities, possible neutral gaps and
initial faction-affiliated NPCs only. No runtime strategy, market, history, story
or missions are implemented by this phase. No canonical parent documents changed.

Next roadmap task, after review: Phase 8 semantic save/load, version header,
runtime RNG and allocator persistence, reconstruction/validation, and a matching
StateHash after save/load and continued identical inputs.
