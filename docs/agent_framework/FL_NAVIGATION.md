# FL-Navigation v1

Install the `gym` extra from the built wheel. The environment implements the
[Gymnasium Env API](https://gymnasium.farama.org/api/env/) and is CPU-only.

```python
import gymnasium as gym
from faultline_sim.agents import heuristic_action

env = gym.make("faultline_sim.navigation:FL-Navigation-v0")
try:
    obs, _ = env.reset(seed=42)
    finished = False
    while not finished:
        obs, reward, terminated, truncated, info = env.step(heuristic_action(obs))
        finished = terminated or truncated
    print(info)
finally:
    env.close()
```

The native compact world spans 512 × 512 m. The agent starts at the generated
central junction. The scenario selects a goal on the generated road grid from
the seed. It prefers goals at least 12 m away along each axis and within 224 m
Manhattan distance. If none qualify, it accepts a junction 12–256 m away.

| Observation | Shape and dtype | Meaning |
| --- | --- | --- |
| `position` | `(2,)`, float32 | Current native position in meters, X then Z |
| `goal` | `(2,)`, float32 | Target junction in the same coordinate system |
| `road_axes` | `(2, 32)`, int32 | Sorted X and Z coordinates of the generated rectangular road grid, padded with zeros |
| `axis_counts` | `(2,)`, int32 | Count of valid entries for each axis |

Actions 0–4 are idle, east (+X), west (−X), south (+Z) and north (−Z). Each
decision holds its action for eight 50 ms engine ticks. Free cardinal motion
covers 2 m. Buildings and world bounds use native collision. Invalid actions
raise an error before the physical state advances.

The reward is −1 per decision, with a +100 bonus on success. Reaching within
1 m on both axes while active ends the episode successfully. Player
incapacitation ends the episode unsuccessfully. A decision limit truncates the
episode after 256 actions by default, configurable from 1 through 512.

`info` contains only the public completed native tick and success flag.
Policies receive detached observations. Engine memory, RNG internals, hashes,
snapshots and strategic state stay with the host. Code supplied to the host itself
must be trusted. With the same scenario build, platform, dependencies, seed and
actions, the engine and scenario reproduce their observations and outcomes.

See the [custom policy example](../../examples/navigation_agent.py).
