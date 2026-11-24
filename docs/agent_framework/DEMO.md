# Public demo runbook

The demo uses the local wheel and synthetic worlds. Run it with the `gym` extra
from a clean directory. The walkthrough takes the audience from policy action
to trace verification and benchmark comparison.

```sh
faultline-sim simulate --seed 42 --ticks 100
faultline-sim benchmark --scenario continuity --agent heuristic --seed 42 --episodes 1 --trace-dir traces
faultline-sim replay traces/continuity-heuristic-42.json
faultline-sim benchmark --scenario adaptation --agent heuristic --seed 42 --episodes 10 --json heuristic.json
faultline-sim benchmark --scenario adaptation --agent planning --seed 42 --episodes 10 --json planning.json
faultline-sim compare heuristic.json planning.json
```

1. Show the simulator's tick and StateHash from the same seeded engine used by the game.
2. Explain the engine checkpoint after the fourth decision. Play the trace in a fresh process and show the successful boundary at tick 496.
3. Explain the newly visible route constraint. Compare two policies over the same seed range and show the raw decision and replay metrics.
4. Open the [qualified benchmark table](BENCHMARK_RESULTS.md) and the original
   [365-day world demo](../../demo/README.md). The benchmark screenshot or terminal
   recording should display the exact command and packaged runtime used.

## 0.1.0b2 route challenge demo

Use the [independent consumer](EXTERNAL_CONSUMER.md) to show installed-wheel
benchmark export, a working custom policy and an idle policy that exhausts the
budget. Record the two independent replays and discuss why their outcomes differ.
Then show the success intervals and decision-cost plots from the frozen results.

```sh
python -I review.py output
python -m faultline_sim.scientific replay output/idle.json
python tools/plot_scientific.py build-scientific-variants/evaluation/summary.json plots
```
