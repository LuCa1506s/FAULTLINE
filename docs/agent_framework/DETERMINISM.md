# Determinism contract

The C++ engine owns state, clock, geometry, RNG and persistence. Given the same
build, platform, runtime conditions, seed, configuration, initial state and
ordered actions, the SDK compares semantic StateHash at completed tick
boundaries. The current engine uses GeneratorVersion 3, SaveFormatVersion 14,
GameVersion 0.1.0 and `FAULTLINE.state.v16`. See the
[engine determinism contract](../../FAULTLINE_GAME_V1/docs/DETERMINISM.md).

Gymnasium episodes additionally depend on the scenario version, seed and
environment dependencies. Replay validates a tested runtime identity and
matches the initial snapshot and decision outcomes. Timings are separate
measurements. Model inference may vary independently of recorded actions.
Cross-platform measurements apply to the builds and seeds tested in the
[verification ledger](TEST_EVIDENCE.md).
