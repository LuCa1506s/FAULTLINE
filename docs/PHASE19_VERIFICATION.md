# Phase 19 verification

Phase 19 implements the authoritative Combat foundation and stops before Witness +
Police. Baseline was clean commit `76d0b26`.

## Versions

- GameVersion 0.1.0; GeneratorVersion 3; HistoryVersion 1.
- SaveFormatVersion 12; CombatStateVersion 1.
- StateHash `FAULTLINE.state.v14`.
- Authentic v11 fixture: 970887 bytes, SHA-256
  `2ec6b2353547c8e15b84da24f149659fb2be3eb7d2ad5050ca8abbbfc2fc3019`.

## Executed gates

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

Fresh final Release result: exit 0, 34/34 CTest PASS, 153.80 seconds. Project compilation
reported no warnings. The doctest executables contain 275 cases and 1,117,830
assertions after the final two combat cases; focused Combat is 16 cases / 67
assertions.

Independent seed-12345 runs with 30 scripted rounds emitted identical StateHash
`9ddfe3da2118b2d8` and byte-identical 972094-byte saves, SHA-256
`9dd0eaa8cab26c37a19ac529ea508196377024ffdcfc333c421c59d3a1ba7357`.
The v11 fixture loaded twice into identical v12 state and canonical bytes.

The 100-world stress gate executed 2700 real attacks, 2700 hits, 300 NPC deaths,
zero save, continuation or StateHash mismatches, BatchHash `9d204db1cd0449f0`,
in 23.988 seconds. Focused tests separately cover out-of-range and blocked misses,
stale/self/abstract targets, cooldown, reload, player damage, terminal Health,
leader death/succession, abstraction/rematerialization, and stable same-tick order.

Worldgen verification for seed 12345 remains exactly:

- v1 `2938f0ca42162bc9`
- v2 `5f3a446fe0d2f974`
- v3 `888c5dbd10c15886`

No canonical parent document changed. No Witness, Police, Wanted or vehicle-combat
system was added.
