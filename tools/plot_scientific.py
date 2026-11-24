"""Plot success intervals and graph planner cost from the qualified JSON."""

from argparse import ArgumentParser
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCENARIOS = ("navigation", "continuity", "adaptation")
LEVELS = ("EASY", "MEDIUM", "HARD", "STRESS")
AGENTS = (("random", "Random", "#65768a"), ("heuristic", "Heuristic", "#bf622a"),
          ("legacy-planning", "Legacy planner", "#775698"), ("planning", "Graph planner", "#196757"))


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("summary", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = json.loads(args.summary.read_bytes())
    rows = {case["case"]: case["summary"] for case in source["cases"]}
    if len(rows) != 48 or source["episodes_per_case"] != 100:
        raise ValueError("Expected the complete 48-cell qualification summary")

    figure, panels = plt.subplots(1, 3, figsize=(13, 4.4), sharey=True)
    for axis, scenario in zip(panels, SCENARIOS):
        for index, (key, label, color) in enumerate(AGENTS):
            values = [rows[f"{scenario}-{level}-{key}"] for level in LEVELS]
            success = [value["success_rate"] * 100 for value in values]
            lower = [max(0, rate - value["wilson_95"][0] * 100) for rate, value in zip(success, values)]
            upper = [max(0, value["wilson_95"][1] * 100 - rate) for rate, value in zip(success, values)]
            axis.bar([j + (index - 1.5) * 0.19 for j in range(4)], success, width=0.17,
                     label=label, color=color, yerr=(lower, upper), capsize=2, linewidth=0, zorder=3)
        axis.set_title(scenario.title())
        axis.set_xticks(range(4), LEVELS)
        axis.set_ylim(0, 108)
        axis.grid(axis="y", alpha=0.25)
    panels[0].set_ylabel("Success rate (%)")
    handles, labels = panels[0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="lower center", ncol=4, frameon=False)
    figure.suptitle("Route challenge success and 95% Wilson intervals · 100 seeds per cell", y=0.96)
    figure.tight_layout(rect=(0, 0.1, 1, 0.9))
    figure.savefig(args.output / "success-with-intervals.png", dpi=180)
    plt.close(figure)

    figure, axis = plt.subplots(figsize=(7.5, 4.2))
    for scenario, color, marker, style in zip(SCENARIOS, ("#4675a1", "#775698", "#196757"), ("o", "x", "s"), ("-", "--", "-")):
        durations = [rows[f"{scenario}-{level}-planning"]["mean_decisions"] for level in LEVELS]
        axis.plot(LEVELS, durations, color=color, marker=marker, linestyle=style, label=scenario.title())
    axis.set_title("Mean decisions for the graph planner")
    axis.set_ylabel("Decisions per episode")
    axis.grid(axis="y", alpha=0.25)
    axis.legend(frameon=False)
    figure.tight_layout()
    figure.savefig(args.output / "planning-decisions.png", dpi=180)
    plt.close(figure)


if __name__ == "__main__":
    main()
