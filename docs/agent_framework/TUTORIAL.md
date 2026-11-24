# Run your first external agent

Start with a wheel for your platform and CPython version. See the
[quick start](QUICK_START.md) for the build and installation procedure.

## Inspect the engine

Copy `examples/python_quickstart.py` to a directory outside the source checkout
and run it with the Python interpreter from your environment. The example
moves the native player four meters, advances 100 idle ticks and restores a
checkpoint. The qualified builds reported tick 116 and `143ab1cb59979e51`.

## Supply a policy

`examples/navigation_agent.py` shows the full Gymnasium loop. Its `decide`
function receives the current position, goal and road axes. You can replace the
function with a learned policy or use the environment through `gymnasium.make`.
The [scenario specification](FL_NAVIGATION.md) documents array shapes, reward
and legal actions. Use a separate evaluation range after tuning on development
seeds.

## Integrate a text backend

The [text policy example](../../examples/text_policy.py) supplies a deterministic
local callback to `TextAgent`. A provider integration implements the same
`Callable[[str], str]` contract and owns its request timeout and credentials.
The adapter sends the public observation and expects one JSON action value.

```sh
python examples/text_policy.py episode.json
faultline-sim validate-trace episode.json
faultline-sim replay episode.json
```

The callback is used only to record the run. The verification process applies
the recorded actions. For a C++ application, use the independent
[CMake consumer](CPP_SDK.md) and its public header.
