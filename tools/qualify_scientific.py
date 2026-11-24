"""Run the frozen route-challenge matrix with the installed wheel."""

from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
from itertools import product
import json
from pathlib import Path
from shutil import copyfile
import subprocess
import sys
from time import perf_counter

from faultline_sim.qualification import LEVELS, SCENARIOS
from faultline_sim.scientific import AGENTS, HOLDOUT_COUNT, HOLDOUT_START, feasibility_witness


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("protocol", type=Path)
    parser.add_argument("--workers", type=int, default=3, choices=range(1, 9))
    args = parser.parse_args()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    copyfile(args.protocol, directory / "protocol.md")
    preregistration = sha256((directory / "protocol.md").read_bytes()).hexdigest()
    print("Frozen protocol SHA-256", preregistration, flush=True)
    command = [sys.executable, "-I", "-m", "faultline_sim.scientific"]

    def run(case):
        scenario, level, agent = case
        label = f"{scenario}-{level}-{agent}"
        output = directory / f"{label}.json"
        base = [*command, "run", "--scenario", scenario, "--level", level, "--agent", agent,
                "--split", "evaluation", "--seed", str(HOLDOUT_START)]
        with (directory / f"{label}.log").open("w", encoding="utf-8") as log:
            subprocess.run([*base, "--episodes", str(HOLDOUT_COUNT), "--json", str(output), "--csv",
                            str(output.with_suffix(".csv"))], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
            samples = directory / "sample-traces"
            subprocess.run([*base, "--traces", str(samples)], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
            subprocess.run([*command, "replay", str(samples / f"{label}-{HOLDOUT_START}.json")],
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
        data = json.loads(output.read_bytes())
        infeasible = []
        if agent == "planning":
            for result in data["results"]:
                if not result["success"] and not feasibility_witness(scenario, level, result["seed"])["success"]:
                    infeasible.append(result["seed"])
        print(label, data["summary"]["success_rate"], data["summary"]["replay_rate"], flush=True)
        return {"case": label, "runtime": data["runtime"], "summary": data["summary"], "infeasible": infeasible}

    started = perf_counter()
    with ThreadPoolExecutor(args.workers) as executor:
        results = list(executor.map(run, product(SCENARIOS, LEVELS, AGENTS)))
    summary = {"protocol_sha256": preregistration, "seed_start": HOLDOUT_START, "episodes_per_case": HOLDOUT_COUNT,
               "workers": args.workers, "wall_seconds": perf_counter() - started, "cases": results}
    (directory / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    if any(row["infeasible"] or row["summary"]["replay_rate"] != 1 for row in results):
        parser.exit(1, "Qualification found infeasible routes or replay divergences\n")


if __name__ == "__main__":
    main()
