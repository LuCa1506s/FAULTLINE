"""Use an installed FAULTLINE wheel from an independent consumer project."""

from argparse import ArgumentParser
import json
from pathlib import Path
import subprocess
import sys

import faultline_sim
from faultline_sim.scientific import run_episode, write_replay


def vertical_first(observation):
    """Follow the city column, then turn to the destination road."""
    x, z = observation["position"]
    gx, gz = observation["goal"]
    if z != gz:
        return 3 if z < gz else 4
    if x != gx:
        return 1 if x < gx else 2
    return 0


def invoke(*args):
    command = [sys.executable, "-I", "-m", "faultline_sim.scientific", *map(str, args)]
    completed = subprocess.run(command, check=True, capture_output=True, text=True, timeout=60)
    return json.loads(completed.stdout)


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(faultline_sim.__file__).resolve()
    if not root.is_relative_to(Path(sys.prefix).resolve()):
        raise RuntimeError("Expected FAULTLINE to be installed in the consumer environment")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    summary = invoke("run", "--scenario", "adaptation", "--level", "MEDIUM", "--agent", "planning",
                     "--seed", 42, "--episodes", 3, "--json", output / "benchmark.json", "--csv", output / "benchmark.csv",
                     "--traces", output / "benchmark-traces")
    if summary["success_rate"] != summary["replay_rate"] or summary["success_rate"] != 1:
        raise RuntimeError("Consumer benchmark contract failed")
    outcomes = {}
    for name, agent in (("custom", vertical_first), ("idle", lambda _: 0)):
        trace, _, _ = run_episode("navigation", "EASY", 42, agent)
        path = output / f"{name}.json"
        write_replay(path, trace)
        independent = invoke("replay", path)
        if independent != trace["steps"][-1]:
            raise RuntimeError("External replay reported a different terminal state")
        outcomes[name] = independent
    if not outcomes["custom"]["success"] or outcomes["idle"]["failure"] != "budget" or outcomes["idle"]["stops_completed"] != 0:
        raise RuntimeError("Consumer success or failure analysis did not match the task")
    report = {"benchmark": summary, "outcomes": outcomes, "idle_analysis": "No stop reached before spending the decision budget."}
    (output / "consumer-summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
