# Quick start

## Install a reviewed wheel

Use CPython 3.12 or 3.14 on an x64 target listed in the
[distribution matrix](DISTRIBUTION.md). Pick the exact wheel for your platform
and Python from the reviewed release kit. `cp312` and `cp314` identify the two
qualified interpreters. Check the kit against its SHA-256 manifest before
installing. The candidate can be installed from the kit before publication.

```sh
python -m venv .venv
```

Activate with `. .venv/bin/activate` on Linux or `.venv\Scripts\Activate.ps1`
in PowerShell. Replace `WHEEL_PATH` with the path to the wheel and keep the
`[gym]` extra to run the benchmark and replay commands.

```sh
python -m pip install 'WHEEL_PATH[gym]'
python -I -c "from faultline_sim import Simulation; sim = Simulation(seed=42); sim.advance(ticks=100); print(sim.state_hash()); sim.close()"
python -I -m faultline_sim.scientific run --scenario navigation --level EASY --agent planning --seed 42 --episodes 1 --json results.json --csv results.csv --traces traces
```

Replay the `.json` file generated in `traces` with
`python -I -m faultline_sim.scientific replay TRACE_PATH`. The replay command
reports where a trace diverges and checks the Python, extension and dependency
fingerprints recorded in it. See [replay details](REPRODUCIBILITY.md).

After the release appears on PyPI, use
`python -m pip install 'faultline-sim[gym]==0.1.0b2'` in a fresh environment.

## Build from source

Use CPython, CMake 3.20+, Ninja and a C++20 compiler. To build a wheel from a
checkout, run `python -m pip wheel --no-deps --wheel-dir build-wheels .` and
install the generated wheel using the steps above. The source archive can
also be built into a wheel using the [distribution guide](DISTRIBUTION.md).

For a native-only build, use CMake from the repository root.

```sh
cmake -S . -B build-framework -DCMAKE_BUILD_TYPE=Release
cmake --build build-framework --config Release --parallel 4
ctest --test-dir build-framework -C Release --output-on-failure --no-tests=error
```

Use the [standalone C++ consumer](CPP_SDK.md) in a separate project. To verify the
Python wheel, copy `tests/python/test_*.py` into a temporary directory, install
`pytest` and the wheel with its `gym` extra, then run `python -I -m pytest -q`
from that directory. See [verified artifacts and platform scope](TEST_EVIDENCE.md).
