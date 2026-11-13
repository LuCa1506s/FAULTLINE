# Phase 20 verification

Phase 20 implements Witness + Police and stops before Vehicles. Starting HEAD was
clean `a19439e` on `master`; canonical gameplay baseline was Phase 19 commit
`33f798a` and the intervening commits were audited Map2D/documentation work.

## Versions and fixture

- GameVersion 0.1.0; GeneratorVersion 3.
- SaveFormatVersion 13; StateHash `FAULTLINE.state.v15`.
- PoliceStateVersion 1; all other domain versions remain 1.
- Authentic v12 fixture: 972142 bytes, SHA-256
  `681b6a97ec9d74bd183dbc8a5f1c5177069a48c534b8ece035e2c77e9e34cefc`,
  Phase 19 StateHash v14 `f83aa033ca06ffd4`, tick 15, seed 12345.

## Focused evidence

The focused suite covers neutral initialization, typed attribution and replay-safe
Crime IDs, unobserved crime, spatial witness eligibility, real static LoS, adjacent
cells, dead/abstract witnesses, exact report deadlines, multi-witness ordering,
direct police observation, last-known information, all 64 Wanted state pairs,
forbidden transitions, every deadline transition, save/load for every state,
response identity/residency, Character AI movement, CombatSystem-only damage,
120-day arrest, malformed v13 input, and authentic v12 migration/payload/RNG checks.

Focused result: **19 cases / 1080 assertions PASS**. Independent processes emitted
identical stdout and v13 save SHA-256
`fa99aed265bbb2a71d0a2f5974fea2d9d83e1853c78bf2b0f2f6b59266eb25b1`.
Fast regression: **26/26 CTest PASS** in 16.28 seconds, including Map2D.

The 100-seed stress gate executes real Combat -> crime -> witness -> report ->
knowledge -> response behavior plus a deliberately unwitnessed event per seed.
It processed 708 crimes (608 witnessed, 100 unwitnessed), 2324 delivered reports,
488 Wanted transitions (100 to search, 96 to pursuit), 300 responses and 96
reacquisitions. Duplicate facts,
illegal states, save mismatches, continuation mismatches and StateHash mismatches
were all zero. BatchHash was `33bbd4c4af04965d`.

## Final canonical Release gate

The required command completed successfully with **39/39 CTest PASS in 171.39
seconds** after a complete affected-target rebuild; no compiler warnings or errors
were present. All doctest executables contain **326
cases / 1,143,209 assertions**, including focused Police at 19/1080. The gate includes
1000-seed worldgen/history, 250-seed history/simulation, multi-year market, combat,
player, Character AI, persistence, process determinism, Map2D, and police stress.

Historical BaseWorldHash contracts remain v1 `2938f0ca42162bc9`, v2
`5f3a446fe0d2f974`, and v3 `888c5dbd10c15886`.

No parent specification, renderer authority, hardware keycode, vehicle, police car,
roadblock, helicopter, SWAT, advanced disguise/stealth, or Phase 21 system was added.

