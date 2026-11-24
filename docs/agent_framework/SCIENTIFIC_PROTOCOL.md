# Route challenges protocol 1

The frozen 0.1.0b1 benchmark keeps its original Navigation v1, Continuity v1
and Adaptation v2 contracts. The new protocol uses a separate runner, trace
format and qualification. It measures route planning, observable changes and
checkpoint continuation in the compact native city.

The audit found identical decision rules for heuristic and planning in the
old Navigation and Continuity scenarios. Both averaged 79.68 decisions and
road efficiency 1.0. Old Adaptation planning averaged 75.14 decisions and
efficiency 0.9374, while heuristic stopped after 10.86 decisions on average.
The audited delay-and-detour strategy completed 10/10 fresh trials without
reading the exclusion. These results motivate route constraints that are fixed
before policy execution and revealed according to a declared schedule.

## Task and levels

| Level | Sequential stops | Excluded road junctions per leg |
| --- | ---: | ---: |
| EASY | 1 | 0, or 1 for Adaptation |
| MEDIUM | 2 | 1 |
| HARD | 3 | 2 |
| STRESS | 4 | 3 |

Stops alternate between a delivery point and the starting junction. The
scenario chooses delivery points and closure candidates on the native compact
road grid using a domain-separated seed. Junction coordinates snap at most
one meter to the two-meter movement lattice. Candidates that disconnect the
leg on the road graph are rejected. Every position, collision and tick comes
from the C++ controller.

The policy sees only current position, current stop, road axes, active
closures, count of stops completed and total, and remaining decisions. Future
stops and unannounced closures are hidden. Navigation announces closures at
leg start. Continuity uses the same route conditions and checkpoints the native
engine after decision four of each leg. Adaptation announces its preselected
closures after decision four of each leg. An action cannot affect the closure
selection or the graph of potential roads.

An episode earns −1 per decision and 99 at its successful final stop. Entering
an active exclusion costs −25 and terminates the episode. An incapacitated
player or exhausted budget ends the episode unsuccessfully. The decision
budget is twice the summed constrained road reference, plus four decisions
per leg for announcements. The budget gives enough room for a second traversal
of the same length. Baselines receive the same actions and observations.

## Frozen qualification criteria

- Use exactly seeds 31,000–31,099 for every combination of scenario, level and
  baseline. Development ranges end at 30,999. The Random policy stream is
  determined by run ordinal and a separate domain, independent of world seed.
- Use uniform random actions, the original X-then-Z heuristic rule, the original
  single-exclusion planner, and a shortest-road graph planning policy. The legacy
  planner reads the first announced exclusion in the supplied order because its
  original observation contract supports one rectangle. All fields it receives
  are available to other policies.
- Report success and 95% Wilson intervals. Interpret intervals as conditional
  binomial summaries of this seed set, with paired comparisons using the same
  seeds. Report mean reward, ticks, decisions, completed stops, checkpoints,
  invalid outputs and failure modes. Report road-reference efficiency on
  successful runs and zero for failed runs. Native shortcuts outside the road
  reference can yield values above one.
- Separate policy time, environment and recording time, and replay time.
  Verify each trace with no policy invocation, comparing all outcomes and
  native hashes every 16 decisions and at the terminal state. Retain raw rows
  and independent replay samples. Prove successful instances physically
  reachable through recorded actions. Run the wait-and-route feasibility policy
  on planner failures to distinguish policy failure from task construction.
- Qualify each level by actual feasibility and replay. Report remaining
  ceiling and floor effects. The protocol targets these tasks and decision
  rules. Separate empirical measures are needed for other forms of reasoning.

`python -m faultline_sim.scientific run` exports JSON and CSV. Use
`python -m faultline_sim.scientific replay` with a saved protocol trace.
