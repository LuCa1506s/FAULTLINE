# Phase 18 verification

Phase 18 implements active-only Basic Character AI without changing generated
world identity or the persistence schema. The dedicated suite covers physical
initialization, idle/move/follow/flee/operation intents, local perception, fixed
50 ms movement, facing, static collision, signed/multiple cell crossings, stable
multi-NPC ordering, player isolation, abstraction/rematerialization, validation,
save/load continuation and long execution.

The headless tool executes real active Character AI:

```powershell
build-release/faultline_characters.exe --seed 12345 --ticks 1200 --save build-release/phase18-character.flsave
build-release/faultline_characters.exe --range 1 10 --ticks 1000
ctest --test-dir build-release -R "character_ai_(tests|process_determinism|batch_10)" --output-on-failure -j 3
```

Seed 12345 / 1200 ticks produced 1 active NPC, 1179 physical movements, 3 cell
crossings, one abstraction and one rematerialization, with zero invalid links,
duplicate bodies, save mismatches, continuation mismatches or StateHash mismatches.
Final StateHash: `65888fedbbdc1dc1`; BatchHash: `d5d16acd102dcac0`.

Seeds 1..10 / 1000 ticks produced 16 activated NPCs, 14934 physical movements,
18 cell crossings, 7 abstractions and 7 rematerializations. All mismatch/invalid
counters were zero; BatchHash `5d5d0e9c66897497`. The three focused CTest gates
passed in 16.60 seconds wall time when run concurrently. Independent processes
compare full stdout and canonical save SHA-256.

Cross-process save size is 970887 bytes with SHA-256
`2ec6b2353547c8e15b84da24f149659fb2be3eb7d2ad5050ca8abbbfc2fc3019`.
The 10000-tick seed-12345 stress run completed in 15.8 seconds and reported 3659
movements, 6 cell crossings, 2 abstractions, 1 rematerialization and zero invalid,
duplicate or determinism failures. A second abstraction is causal: the moving NPC
leaves the stationary player's deactivation radius near the end of the run.

Final canonical command:

```powershell
$env:CTEST_PARALLEL_LEVEL='4'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Configuration Release
```

Exit 0. Configure/build/CTest took 155.634 seconds; CTest itself took 114.08
seconds. All 31/31 gates passed, including all prior worldgen/history/story/market/
simulation/player stress gates. Doctest totals were 259 cases and 1,113,191
assertions, all passing; the Character AI suite contributed 11 cases and 561,769
assertions. The fast subset passed 20/20 in 8.05 seconds.

SaveFormatVersion remains 11, StateHash remains `FAULTLINE.state.v13`,
GeneratorVersion remains 3, and no Phase 19 combat implementation is included.
