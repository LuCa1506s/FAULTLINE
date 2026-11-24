# Benchmark suite

Install the wheel with the `gym` extra. The `faultline-sim`
executable and `python -m faultline_sim` expose the same interface.

```sh
faultline-sim list-scenarios
faultline-sim simulate --seed 42 --ticks 100
faultline-sim benchmark --scenario navigation --agent heuristic --episodes 100 --seed 42
faultline-sim benchmark --scenario adaptation --agent planning --split evaluation --episodes 100 --seed 11000 --json planning.json --csv planning.csv
faultline-sim validate-trace trace.json
faultline-sim replay trace.json
faultline-sim compare heuristic.json planning.json
```

The development partition covers seeds 0–9,999. Evaluation uses seeds from
10,000 onward. The qualification uses 11,000–11,099 for each of the three
scenarios and each baseline. The split guards against overlap, and known public
evaluation seeds are suitable for regression comparisons. Evaluate against a
fresh evaluation range for a final assessment of a tuned policy.

| Scenario | Task and environmental condition |
| --- | --- |
| FL-Navigation | Reach a generated junction using authoritative movement and collision. |
| FL-Continuity | Reach the same kind of junction after the host closes and restores the engine. |
| FL-Adaptation | After decision four, a rectangular exclusion zone becomes visible along the axis with the most progress toward the goal. Entering it ends the episode at −25 reward. The planner must switch routes using the generated road axes. |

The adaptation zone is a scenario constraint on authoritative player positions.
Buildings and roads retain the engine's geometry and collision behavior. Before
activation, the scenario returns zeros for `exclusion` and uses ordinary
FL-Navigation rewards. Ties in progress close the route along X. After
activation, it returns `[min_x, min_z, max_x,
max_z]` in meters. Each decision moves at most 2 m, and the zone is wide enough
that a single action cannot move from one side to the other without detection.

The random agent uniformly selects a discrete action. The heuristic follows
the spawn row before turning toward the goal. The planner scores feasible
routes using the observed road axes and exclusion zone. The optional
`local-text` agent exercises the generic text backend with deterministic,
offline responses. Custom text callbacks receive public observations and a
five-action schema. Replay uses the accepted actions recorded by the evaluator.

Each JSON and CSV result contains success, cumulative reward, decisions,
invalid actions, simulated ticks, efficiency and verified replay. Invalid
policy outputs count as idle commands and consume one decision. Efficiency is
the lower bound on unobstructed cardinal decisions divided by the actual count
for successful episodes, or zero for failures. Adaptation detours can reduce it.

Policy time, environment plus recording time and replay time are measured
separately. The environment measurement includes resets, simulation, observation
and hashing. Linux reports process peak RSS in KiB. On other platforms that
field is null. Timings can vary across runs. State outcomes and hashes are
compared within the declared runtime contract.

`--trace-dir` saves one replay file per episode, and `compare` requires the
same runtime, scenario and seed range in both reports. See the
[trace contract](REPRODUCIBILITY.md) and [local verification evidence](TEST_EVIDENCE.md).
