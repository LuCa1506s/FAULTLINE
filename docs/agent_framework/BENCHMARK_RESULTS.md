# Local benchmark results

The final evaluation ran 100 seeds per cell, from 11,000 through 11,099.
Windows x64 and Ubuntu WSL2 x64 each executed 900 episodes. The benchmark
replayed all 1,800 episodes and verified each intermediate hash. Outcome,
reward, decisions, tick counts and final StateHash matched between these
two environments for all 900 paired cases.

| Scenario | Agent | Success per 100 | Mean reward | Mean decisions |
| --- | --- | ---: | ---: | ---: |
| FL-Navigation | Random | 0 | −256.00 | 256.00 |
| FL-Navigation | Heuristic | 100 | 20.32 | 79.68 |
| FL-Navigation | Planning | 100 | 20.32 | 79.68 |
| FL-Continuity | Random | 0 | −256.00 | 256.00 |
| FL-Continuity | Heuristic | 100 | 20.32 | 79.68 |
| FL-Continuity | Planning | 100 | 20.32 | 79.68 |
| FL-Adaptation v2 | Random | 0 | −242.28 | 238.92 |
| FL-Adaptation v2 | Heuristic | 0 | −34.86 | 10.86 |
| FL-Adaptation v2 | Planning | 100 | 24.86 | 75.14 |

These scenarios isolate specific behavior. Continuity measures successful
recovery of the engine and the ability to resume from public observations.
Adaptation distinguishes a policy that keeps its initial route from a planner
that incorporates the newly visible constraint. See the
[benchmark contract](BENCHMARKS.md) for the exact rules and replay scope.

The follow-up audit found that Navigation and Continuity use the same decision
rule under both heuristic and planning labels. A predetermined delay and detour
also completed all ten fresh Adaptation v2 trials without using the displayed
exclusion. The table describes the frozen baseline runs. The
[independent audit](RC_AUDIT.md) records these limits on interpretation.

The Windows qualification wall time was 88.70 s with three worker processes.
Linux under WSL2 took 99.03 s with two. They overlapped on the same eight-logical-
processor host. Linux CLI process peak RSS was 48,544–49,316 KiB across the
nine combinations. Windows per-episode mean recording time ranged from 0.031
to 0.239 s and Linux from 0.023 to 0.199 s across the tested workloads.

The JSON and CSV rows and summary are in
`build-framework-python/windows/evaluation-v2/` and
`build-framework-python/linux/evaluation-v2/`. The release kit includes their
copies for local review.
