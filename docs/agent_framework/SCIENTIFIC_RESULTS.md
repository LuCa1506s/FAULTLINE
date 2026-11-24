# Route challenges qualification

The Windows CPython 3.12 wheel passed 31 installed tests. Qualification used
100 seeds per case at 31,000–31,099, 48 cases and 4,800 episodes. All 4,800
replays passed and each case provided a sample independently replayed from
disk. Every tested case was physically solvable by the graph policy.

| Scenario | Level | Random | Heuristic | Legacy planning | Graph planning |
| --- | --- | ---: | ---: | ---: | ---: |
| Navigation / Continuity | EASY | 0% | 100% | 100% | 100% |
| Navigation / Continuity | MEDIUM | 0% | 25% | 100% | 100% |
| Navigation / Continuity | HARD | 0% | 0% | 8% | 100% |
| Navigation / Continuity | STRESS | 0% | 0% | 3% | 100% |
| Adaptation | EASY | 0% | 52% | 100% | 100% |
| Adaptation | MEDIUM | 0% | 25% | 100% | 100% |
| Adaptation | HARD | 0% | 0% | 14% | 100% |
| Adaptation | STRESS | 0% | 0% | 4% | 100% |

At 100 episodes, Wilson 95% intervals for 0% and 100% are 0–3.70% and
96.30–100%. The 25% cases have an interval of 17.55–34.30%, and the
52% case has an interval of 42.32–61.54%. JSON files contain the interval
for each case.

Graph planning averaged 61.23 to 308.68 decisions from EASY to STRESS in
Navigation and Continuity, with road-reference efficiency 1.0. In Adaptation
the range was 62.83 to 323.98 decisions and the STRESS efficiency was 0.9517.
The drop reflects additional decisions under delayed observation of closures.
Closure failures dominate legacy planning on HARD and STRESS. The published
rows retain reward, decisions, partial task completion, error counts, ticks,
timing and replay status.

The protocol distinguishes route policies that handle one exclusion from
policies that incorporate multiple constraints. Random remains at the floor
and graph planning reaches the ceiling. Navigation and Continuity outcomes
match under these policies, which demonstrates engine checkpoint equivalence.
Additional policy-memory tasks and evaluated models are required to measure
advanced capability differences.

The public remaining-budget field is derived from the constrained route
reference. It can correlate with hidden route costs. This protocol measures
behavior under the declared observation contract and does not attempt to
eliminate all statistical clues about upcoming constraints.

The run took 248.14 s with four workers. Raw CSV and JSON, 48 independent
replay samples, and the copied protocol are under
`build-scientific-variants/evaluation/`. The frozen protocol SHA-256 is
`4b0098b902b84174c4e3c23a6ea21e53b33eaff06d6abf8f8c924ec1d810a057`.
The Windows wheel used for this evaluation is preserved under
`build-scientific-variants/qualification-wheel/`.
