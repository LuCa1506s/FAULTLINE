# Phase 8 / Save Foundation verification

Verified on 2026-10-16 in the repository's Windows Debug environment. Canonical
documents in the parent directory were read and left untouched. Phase 9 is not
implemented. The complete schema/policies are in [SAVE_FORMAT_V1.md](SAVE_FORMAT_V1.md).

1. **Created files:** `src/persistence/save.hpp`, `binary.hpp`, `schema_v1.hpp`,
   `save.cpp`; `tests/persistence_tests.cpp`; `tools/savecheck/main.cpp`;
   `docs/SAVE_FORMAT_V1.md`; this verification document.
   **Modified files:** `CMakeLists.txt`; `src/world/state/stores.hpp`,
   `world_state.hpp`, `world_state.cpp`; `src/simulation/runtime/game_state.hpp`,
   `game.hpp`, `game.cpp`; `README.md`; `DEVELOPMENT_STATUS.md`;
   `docs/DETERMINISM.md`; `docs/FILE_INDEX.md`. Generated build/test logs remain
   under `build/`. No worldgen source files were modified.

2. **SaveFormatVersion:** 1, centralized as `currentSaveFormatVersion`.
   GameVersion is the independent stable tuple 0.1.0. GeneratorVersion remains 3.

3. **Format:** explicit binary, fixed-width primitives, framed sections,
   length-prefixed UTF-8 strings and arrays; no C++ object memory dumps or new
   serialization dependency. Public APIs support memory buffers and files.

4. **Magic:** 16 bytes, ASCII `FAULTLINE_SAVE` followed by two zero bytes.
   The file extension is not used for recognition.

5. **Byte order:** little-endian, including integer and IEEE-754 binary32 bits;
   signed zero is canonicalized and nonfinite floats rejected.

6. **Serialized sections:** Runtime, Generated, Entities, Districts, Territories,
   Factions, NPCs, Vehicles, Missions, Relationships. All existing semantic fields
   and generated configuration/baselines are explicit in `schema_v1.hpp`. No
   future player/market/story/police fields were invented.

7. **Exclusions:** active NPC bodies/links after synchronization; SpatialGrid
   membership; application input/command queues; Simulation event/structural
   buffers; FixedStepRunner accumulation; engine services and handles. Remaining
   data-only entity/components and vehicle links persist; spatial caches rebuild.
   Generated/runtime/transient/derived classification is documented separately.

8. **RNG:** all four xoshiro256** words for each of the seven ordered runtime
   domains persist and restore exactly. Tests consume values before the snapshot
   and compare 100 future values per stream, without reseeding: PASS.

9. **Allocators:** Entity, NPC, Faction, Territory, Vehicle, Mission and District
   high-water marks persist. Restoring validates IDs and marks; exhausted domains
   remain exhausted. Destroyed Entity IDs are not reused. All seven continuation
   checks and explicit collision/null/duplicate tests: PASS.

10. **StateHash:** semantic snapshot schema `FAULTLINE.state.v3`; seed 12345/v3
    before/after load both `1c38e03df408cb09`. Full round-trip tests pass for
    generator versions 1, 2 and 3. Runtime fingerprint schema changed; the three
    BaseWorldHash schemas did not.

11. **Continuation:** run 1000 headless ticks, save, then 1000 more with identical
    test-driven RNG consumption/object allocations, versus restored continuation:
    equal StateHash and save bytes, PASS. A fresh `Game(restored)` also resumes
    matching ticks. This tests the currently implemented runtime; no faction AI
    behavior is claimed.

12. **Canonical bytes:** repeated serialization and save/load/save comparisons
    pass for each generator version. Reversed relationship insertion orders and
    signed-zero variants produce equal bytes. NPC snapshot abstraction preserves
    the semantic hash and load/save bytes; rematerialization retains NPC identity.

13. **Malformed input:** invalid magic, unsupported save/game/generator versions,
    section count/order/length/trailing bytes, every truncated prefix of a minimal
    save and sampled truncations through the large generated section all reject.
    Huge count/string declarations, allocation budgets, bad UTF-8, nonfinite
    floats, zero RNG state, tick overflow/time mismatch, allocator reuse,
    duplicate/null IDs, orphan components and missing leader/faction/owner/issuer/
    macro-zone references reject explicitly. Typed errors include section, offset
    and version context. Dead records and valid references remain intact. PASS.

14. **Generator compatibility:** v1/v2/v3 round-trip semantically without adding
    nonexistent older-version actor/urban data. Load does not rerun generation.
    Unsupported versions reject instead of being interpreted as current.

15. **Exact seed 12345 BaseWorldHash verification:** direct CLI runs, automated
    regression vectors, loaded-world comparisons and separate-process CTest:

    | Version | BaseWorldHash |
    |---|---|
    | 1 | `2938f0ca42162bc9` |
    | 2 | `5f3a446fe0d2f974` |
    | 3 | `888c5dbd10c15886` |

    V3 output remains: 8000 x 8000 m, WaterRegions 1, MacroZones 8, RoadNodes 210,
    RoadEdges 391, Districts 18, Parcels 8007, Buildings 8007, Territories 41,
    Factions 5, MajorNPCs 22, Validation PASS.

16. **Build:** canonical command executed successfully:
    `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`.
    Out-of-source Ninja / GCC 16.2.0 / C++20 / Debug, project warning flags enabled.
    No unresolved project warnings/errors. Early executable startup was briefly
    blocked by Windows Application Control; later suites ran without security
    policy changes. That failed attempt was not counted as a passing run.

17. **CTest:** final complete five-test gate PASS in 152.52 s, including
    persistence, foundation, worldgen checkpoint, 1000 seeds and separate-process
    determinism. Final persistence test time 5.12 s; fast-label total 14.09 s.
    Logs are in `build/phase8-build.log` and `build/Testing/Temporary/LastTest.log`.

18. **Doctest:** persistence 15 cases / 2,955 assertions; existing foundation
    49 cases / 210,838 assertions. Total 64 cases / 213,793 assertions PASS.

19. **Labels:** existing `fast`, `determinism`, `stress`, `worldgen`, `mass` remain;
    save tests add `persistence` and belong to fast/determinism. No coverage was
    removed. Use `ctest --test-dir build -L persistence --output-on-failure` for
    focused iterations; full build script still runs every test.

20. **Mass regression:** all seeds 1..1000 terminated and validated in both full
    Phase 8 runs. Final tool time: 138.418 s / 138.418 ms per seed, with unchanged
    actor distributions and zero failed seeds.

21. **Save size:** seed 12345/v3 is 965,599 bytes, without compression.

22. **Indicative performance:** standalone Debug `faultline_savecheck --seed
    12345`: encode 107.851 ms including validation/snapshot; load 96.547 ms
    including validation/cache rebuild. In-memory measurements, excluding
    filesystem and initial generation; one sample, no benchmark claim. Output is
    recorded in `build/phase8-savecheck.log`. File read/write/error/cleanup APIs
    are independently covered by the automated tests.

23. **Known limits:** full generated baseline duplication; no checksum, atomic
    replacement, compression or migrations yet. Only save v1/game 0.1.0 loads.
    Save limits are 64 MiB, 65,536 UTF-8 bytes/string, 100,000 elements/collection
    and 300,000 elements total; large custom generator configurations can exceed
    these initial limits. Safe boundary is single-threaded completed state; queued
    external intent must be supplied again. Continuation coverage is headless;
    no future AI/physics/market/story simulation is implied. Streaming may later
    allocate new physical EntityIds while keeping persistent NpcIds. Cross-platform
    floating-point gameplay bit identity remains outside the supported contract.

24. **Next:** Phase 9, Headless Faction Simulation (treasury/manpower/cohesion,
    basic utility decisions, attack/defend operations and resource commitments),
    only after review. STOP at the Phase 8 checkpoint.
