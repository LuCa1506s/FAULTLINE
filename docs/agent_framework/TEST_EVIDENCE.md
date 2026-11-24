# Local verification evidence

## Phase 00

Initial working-tree revision `c4851564f8df893d1fa27d8a713916353cfc21da`.
Measurements below are from the local qualification of that working tree.

| Platform | Release build | Inherited tests |
| --- | --- | --- |
| Windows 11 Pro x64, GCC 16.2.0, CMake 4.4.4, Ninja 1.13.2 | PASS, 94 build steps, 168.691 s configure + build with four jobs | 42 passed, zero failed or skipped, including 14 stress tests. CTest wall time 303.57 s at four jobs. |
| Ubuntu 26.04.1 x64 on WSL2, GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2 | PASS, 94 build steps, 4.787 s configure and 349.165 s build with three jobs | 42 passed, zero failed or skipped, including 14 stress tests. CTest wall time 187.83 s at three jobs. |

Both builds use `-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON` and a fresh
directory under `build-framework-baseline/`. The bootstrapped Windows compiler
comes from `FAULTLINE_GAME_V1/.tools`. Linux uses packages from Ubuntu. The
inherited test command is `ctest --test-dir <build> --parallel <jobs>
--output-on-failure --no-tests=error --output-junit <build>/ctest.xml`.

### Windows 365-day demo

Ran the [documented demo command sequence](../../demo/README.md) from the fresh
build, with artifacts in `build-framework-baseline/windows/`. All eight tools
exited zero. Wall times include process startup and overlap with builds.

| Command | Seconds | StateHash |
| --- | ---: | --- |
| Map2D seed 12345, day 0 | 0.561 | — |
| Simulator seed 12345 through day 30 | 0.693 | `c934880b12e33bd3` |
| Map2D day 30 checkpoint | 0.267 | `c934880b12e33bd3` |
| Simulator day 30 checkpoint plus 335 days | 0.681 | `2a00844980b17f7e` |
| Simulator seed 12345 through day 365 | 0.759 | `2a00844980b17f7e` |
| Map2D resumed day 365 | 0.231 | `2a00844980b17f7e` |
| Savecheck resumed day 365 | 0.468 | `2a00844980b17f7e` |
| Savecheck direct day 365 | 0.073 | `2a00844980b17f7e` |

BaseWorldHash `888c5dbd10c15886`. Both final files have SHA-256
`fe44d41903d1d9ded2f39c71fb996764835c170446f3d7121e59ac290df36557`.
Day 30 SHA-256 is `cdb6f7a17e8044935324bca51e34e84f9dd553b581d7cba319d30126730f1c01`.
The uninterrupted run reported 883 operations, 141 control changes and 385,418
NPC semantic updates. The logs retain the two continuation segments and both
final validations.

### Linux 365-day demo

Ran the same eight commands in Ubuntu under WSL2. All exited zero. In command
order their wall times were 0.226, 0.154, 0.236, 0.423, 0.435, 0.290, 0.068 and
0.066 seconds. The BaseWorldHash, StateHashes and three save digests matched the
Windows run above for this seed and these builds. Raw logs, SVGs, saves, digests
and per-command timings are under `build-framework-baseline/linux/`.

## Phase 01

Windows Release built the SDK test target in 68.991 s including CMake configure.
The first native consumer run passed in 0.49 s. The remaining 62 build steps
completed in 193.492 s. The integrated suite passed 43/43 tests, including every
original regression, in 252.68 s with four jobs, with zero failures or skips.
The final updated native consumer passed in 1.01 s. Raw logs are under
`build-framework-native/windows/`. CTest wrote the JUnit report at
`build-framework-native/windows/build-framework-native/windows/ctest.xml`
after resolving its relative output path from the build directory. The standalone WSL2
consumer completed its 36-step Release build in 134.22 s and its latest test in
0.57 s, including the SDK snapshot byte limit and compact world bound checks.
Both runs reported continuation StateHash `bf9ec6b06c08fd1a`. The WSL2 consumer
uses `tests/native_consumer` as the CMake source with `BUILD_TESTING=OFF` for
the engine dependency. It also passed reconfiguration after changing its option
name to `FAULTLINE_CHECKOUT`. The initial name collided with the legacy
`project(FAULTLINE)` CMake cache variable. Logs are under
`build-framework-native/linux-consumer/`.

An additional standalone Linux Debug build with GCC AddressSanitizer and
UndefinedBehaviorSanitizer passed the native consumer in 7.72 s with no sanitizer
reports. The build reports an inherited conversion warning in `persistence/binary.hpp`
under this configuration. Logs are under `build-framework-native/linux-sanitized/`.

## Phase 02

Windows x64 CPython 3.12.10 and MSVC 19.51.36260.0 built
`build-framework-python/windows/artifacts/faultline_sim-0.1.0b1-cp312-cp312-win_amd64.whl`
in 138.663 s, including dependency setup and packaging. The wheel SHA-256 is
`394e4c74168889341e4c293a2d4dc7aee105afc2a34404ea830a9a18bc01a4e3`.
It contains the compiled extension, type stubs, typing marker and four license
and notice files. A clean venv installed it without dependencies for the SDK.
Running pytest with `python -I` from a temporary directory outside the checkout
passed 4/4 tests in 0.49 s. The standalone quick start moved from `(0, 0)` to
`(4, 0)`, restored its checkpoint and reported tick 116 and StateHash
`143ab1cb59979e51`. Logs are under `build-framework-python/windows/`.

Ubuntu WSL2 x64 CPython 3.14.4 and GCC 15.2.0 built
`build-framework-python/linux/artifacts/faultline_sim-0.1.0b1-cp314-cp314-linux_x86_64.whl`
in 128.96 s, including dependency setup and packaging. The wheel SHA-256 is
`80e094c173ec56cdc862bd0e12f1b9c92e5b13ff356dc70ae0d74d9be6ee0f37`.
The wheel installed in a fresh Linux venv and passed 4/4 pytest checks in
0.07 s from a temporary directory with `python -I`. The quick start reported
the same observed movement, tick and StateHash as Windows. The Linux wheel
is qualified for this local system and Python version. A manylinux build is
outside this verification. Logs are under `build-framework-python/linux/`.

## Phase 03

Rebuilt Windows and Linux wheels with the Gymnasium extra. With Gymnasium
1.4.0 and NumPy 2.5.5, installed-wheel runs outside the checkout passed 8/8
pytest tests in 1.29 s on Windows and 0.35 s on Linux. These include Gymnasium's
checker, agent observations, idle reward, malformed actions, truncation and
seven successful heuristic episodes including both unsigned seed extremes.

On both platforms the installed custom agent completed seed 42 in 62 decisions,
496 native ticks and cumulative reward 38. A Windows development smoke run
over seven seeds each measured 7/7 heuristic successes and 0/7 random
successes. Every random episode reached a terminal or truncated boundary.
The measured per-episode rows are in
`build-framework-python/windows/navigation-smoke.json`. This smoke is outside
the evaluation seed sets and separate from the Phase 05 benchmark.

## Phase 04

The Windows replay wheel passed 12/12 tests in 1.44 s from outside the checkout.
Continuous and restored runs matched at every action and hash. A separate
process replayed `build-framework-python/windows/continuity-seed42.json` with
success at tick 496 and StateHash `ddfbb963dbd0f1f4`. The tests also verified
format version, runtime mismatch, altered checksum, byte limit and the first
divergent hash at decision two. Linux passed the same 12 tests in 0.70 s.

## Phase 05

The installed benchmark v2 wheels passed 16/16 tests on Windows (5.30 s) and
Linux (2.67 s). The tests cover FL-Adaptation's change, both fixed-axis policies,
the observation-only planner, offline text adapter, disjoint seed partitions,
all CLI commands and CSV/JSON export. Both platforms completed the evaluation
in `build-framework-python/{windows,linux}/evaluation-v2/` with seeds
11,000–11,099, 100 episodes for each of the nine scenario–agent pairs. All
1,800 recorded episodes passed replay verification. The superseded X-axis-only
prototype runs are under `evaluation/`. The final numbers and timing scope are
in [Benchmark results](BENCHMARK_RESULTS.md).

## Phase 06

Final local wheels in `build-framework-release/0.1.0b1/` include the exact
upstream pybind11 license. Both installed from scratch with the `gym,test`
extras. External `python -I` pytest runs passed 16/16 tests in 5.32 s on Windows
and 2.48 s on Linux. `pip check`, the copied SDK quick start, the copied custom
navigation policy and CLI replay all passed. Replays reported StateHash
`ddfbb963dbd0f1f4` and success at tick 496 on both platforms.

| Final wheel | SHA-256 |
| --- | --- |
| `faultline_sim-0.1.0b1-cp312-cp312-win_amd64.whl` | `ec4bee1d1e8070ff66117bc2fdb7bd0b19ca157936e76898da64c3f5abc66325` |
| `faultline_sim-0.1.0b1-cp314-cp314-linux_x86_64.whl` | `c69b3dfea62f85dc8fc0ed51026045ae92896c668364ec8f9cf5e59e0132cec9` |

The final Windows extension has a different binary digest from the Phase 05
wheel, so the nine evaluation cases were repeated on the final wheel in
54.36 s with three worker processes. It verified all 900 replays again. The
final Linux wheel has the same extension digest and packaged scenario sources
as its qualification counterpart. Each kit report and sample trace matches
the corresponding wheel's native digest.

`pip-audit` found zero known advisories for the seven pinned public package
versions listed in `security/audit-requirements.txt` inside the kit. Audit
scope and results are captured in `security/dependency-audit.json`. The root
security policy was resolved for `sdk` and `python/faultline_sim`. Native
regressions and focused sanitizer verification are recorded in Phases 00–01.
The new hosted wheel workflow was syntax-checked locally. Hosted runs remain
pending review and repository publication authorization.

## Phase 07

The installed-wheel text policy example and independent CLI replay passed on
Windows and Linux, with success at tick 496 and StateHash `ddfbb963dbd0f1f4`.
The Windows demo sequence compared 10 paired development seeds with a measured
100 percentage point FL-Adaptation success advantage for planning. Both policies'
replays passed, with no difference in invalid-action count. Demo logs and trace
samples are included in the local kit. All 900 final kit result pairs matched
across both platforms in seed, success, cumulative reward, decisions, invalid
actions, ticks, efficiency, replay status and final StateHash.
The final local manifest covers 71 files. `tools/package_manifest.py --verify`
passed on Windows and `sha256sum --quiet -c SHA256SUMS` passed under WSL2.

## Follow-up qualification

Phases 08–14 have a separate ledger for the frozen b1 audit, b2 empirical
results, packaging, SDK performance, independent consumer and funding drafts.
See [scientific and distribution status](SCIENTIFIC_STATUS.md).
