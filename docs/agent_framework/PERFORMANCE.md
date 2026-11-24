# Performance and robustness qualification

Five fresh processes per SDK ran the same C++20 Release workload on Ubuntu
26.04.1 under WSL2. The Python run used the installed CPython 3.12 wheel from
the source distribution. The workload performs 64 compact-world initializations
after four warmups, then applies 4,096 cardinal decisions of eight ticks each.
It also measures 128 snapshot and restore operations. This is simulation work
and SDK call cost. Provider and model latency are separate measurements.

| Median over five runs | Native C++ | Python SDK |
| --- | ---: | ---: |
| Initialization | 1.378 ms | 1.440 ms |
| Eight-tick step | 75.98 µs | 80.00 µs |
| Simulated ticks per second | 105,295 | 99,996 |
| Serialize snapshot | 1.082 ms | 1.081 ms |
| Restore snapshot | 1.048 ms | 1.056 ms |
| Process peak RSS | 6,192 KiB | 38,568 KiB |

Each run finished at tick 32,768 with StateHash `cb22a105a3a65100` and a
38,885-byte snapshot. The tested C++ observation value uses 32 bytes.
Its small Python JSON projection was 69 bytes. The sampled HARD Adaptation
observation including announced closures was 252 bytes in compact JSON.
The median Python step took 4.03 µs more in this workload, including Python
dispatch and binding work. RSS includes the interpreter and imported modules.

For 120 HARD Adaptation episodes with the graph policy, one, two and four
processes reached 23.94, 44.03 and 76.19 episodes per second. Startup time is
included. All process counts produced the same successes, ticks and hashes.

## Robustness

An instrumented C++ harness tested 1,024 random inputs, truncations and bit flips.
It rejected 946 and accepted 78 structurally valid mutations with reproducible
restores. The rejection paths preserved live state. ASan and UBSan emitted no
diagnostics in this corpus. The replay harness rejected 641 malformed bytes,
truncations, checksum-valid semantic changes and oversized traces.

## Reproduce

Build `tests/native_consumer` in Release and instrumented configurations. Link
`tools/performance_native.cpp` with the resulting `faultline_sdk`, persistence,
simulation, world and core static archives in that order. Linux uses `g++`
with `-std=c++20 -O3 -DNDEBUG -Isdk/include` for measurement. Use
`-fsanitize=address,undefined -fno-omit-frame-pointer` and the corresponding
instrumented libraries for `performance_native fuzz`.

```sh
python -I tools/profile_framework.py
python -I tools/profile_framework.py --episodes 120
python -I tools/fuzz_replays.py
```

Raw data and build and sanitizer logs are under `build-scientific-performance/`.
