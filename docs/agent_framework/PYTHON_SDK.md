# Python SDK

The distribution is `faultline-sim`. Import `faultline_sim` to avoid a module
collision with the existing [faultline distribution](https://pypi.org/project/faultline/).
The Python extension uses pybind11 to call the same C++ simulation as the
[native SDK](CPP_SDK.md).

Build a wheel from the repository root with CPython, CMake, Ninja and a C++20
toolchain. On Windows, use an x64 Microsoft C++ developer shell and x64 CPython.

```sh
python -m pip wheel --no-deps --wheel-dir build-wheels .
python -m pip install path/to/faultline_sim-wheel.whl
```

```python
from faultline_sim import Action, Config, Simulation, WorldProfile

with Simulation(seed=42, config=Config(WorldProfile.COMPACT)) as sim:
    start = sim.observe()
    result = sim.step(Action.EAST, ticks=16)
    print(start.position.x, result.observation.position.x)
    sim.advance(ticks=100)
    print(f"{sim.state_hash():016x}")
    sim.save("experiment.flsave")
```

`advance` applies idle for the specified physical ticks. `observe` and
`road_map` return values. `snapshot` produces `bytes` that a host can pass to
`restore`. `save` and `load` accept a string or path object. `close` releases the
session and is also called when a `with` block exits. `reset`, `load` and
`restore` can reopen a closed session.

Unrecognized actions, out-of-range values and incompatible Python types raise
`ValueError` or `TypeError`. Snapshot failures raise `SnapshotError`, a subclass
of `RuntimeError`. Closed-session access and stepping after a terminal status
raise `RuntimeError`. The C++ guide defines limits, status and platform scope.

Run the standalone [Python quick start](../../examples/python_quickstart.py) to
check movement and restoration using an installed wheel.
