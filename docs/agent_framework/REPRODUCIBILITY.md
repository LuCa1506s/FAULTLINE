# Reproduce an experiment

Use an installed wheel with the `gym` extra and an explicit unsigned 64-bit
seed. Record accepted actions from a policy, then verify the trace separately.

```python
from faultline_sim.agents import heuristic_action
from faultline_sim.replay import record_episode, write_trace

trace = record_episode("continuity", 42, heuristic_action)
write_trace("episode.json", trace)
```

```sh
python -m faultline_sim.replay episode.json
```

Replay reconstructs the initial world from seed and native configuration,
compares the canonical starting snapshot and goal, then applies the recorded
actions. It compares tick, position, reward, completion flags and StateHash at
every decision and reports the first divergence. A trace records the scenario
and schema versions, runtime identity and engine version. Traces are limited to
1 MiB and 512 decisions. A JSON envelope carries a SHA-256 integrity checksum.
The checksum detects corruption and carries no authentication guarantee.

Runtime identity includes the compiled extension SHA-256, platform, architecture,
CPython, Gymnasium and NumPy versions. Use the same build and dependencies for
replay. See the [engine determinism contract](../../FAULTLINE_GAME_V1/docs/DETERMINISM.md)
for the scope of StateHash. A replay compares recorded actions and requires no
policy, API credentials or model inference. Model reproducibility depends on its
own weights, provider, sampling and execution conditions.

## FL-Continuity v1

FL-Continuity uses the FL-Navigation goal and reward. After decision four the
host serializes engine state, closes the native session and restores into a new
native state instance. The episode must still reach a junction after the restart.
The host keeps its target and decision counter in memory. The trace includes the
checkpoint count and verification compares engine hashes as well as outcomes.

The default run keeps the policy alive. A host can separately replace a policy
at the interruption and give it the next public observation. The stateless
heuristic reconstructs its next action from the current position and goal. This
measures recovery through observation. Policy-specific memory serialization
would be a separate experiment with its own state and measurements.
