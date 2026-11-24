"""Command-line entry point for FAULTLINE."""

import argparse
import csv
import json
from pathlib import Path

from . import Simulation


def _commands() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="faultline-sim", description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list-scenarios")
    simulate = commands.add_parser("simulate")
    simulate.add_argument("--seed", type=int, default=42)
    simulate.add_argument("--ticks", type=int, default=100)
    for name in ("validate-trace", "replay"):
        commands.add_parser(name).add_argument("trace", type=Path)
    bench = commands.add_parser("benchmark")
    bench.add_argument("--scenario", choices=("navigation", "continuity", "adaptation"), required=True)
    bench.add_argument("--agent", choices=("random", "heuristic", "planning", "local-text"), required=True)
    bench.add_argument("--episodes", type=int, default=1)
    bench.add_argument("--seed", type=int)
    bench.add_argument("--split", choices=("development", "evaluation"), default="development")
    bench.add_argument("--json", dest="json_path", type=Path)
    bench.add_argument("--csv", dest="csv_path", type=Path)
    bench.add_argument("--trace-dir", type=Path)
    compare = commands.add_parser("compare")
    compare.add_argument("left", type=Path)
    compare.add_argument("right", type=Path)
    return parser


def _compare(left_path: Path, right_path: Path) -> dict:
    reports = []
    for path in (left_path, right_path):
        with path.open("rb") as stream:
            data = stream.read(4_194_305)
        if len(data) > 4_194_304:
            raise ValueError("Benchmark report exceeds the byte limit")
        reports.append(json.loads(data))
    left, right = reports
    matching = ("version", "runtime", "scenario", "split", "first_seed", "episodes")
    if any(left[key] != right[key] for key in matching):
        raise ValueError("Compare requires matching runtime, scenario and seeds")
    return {"left": left["agent"], "right": right["agent"], "episodes": left["episodes"],
            "right_minus_left": {key: right["summary"][key] - value for key, value in left["summary"].items()}}


def _execute(args: argparse.Namespace) -> dict:
    if args.command == "simulate":
        with Simulation(args.seed) as sim:
            obs = sim.advance(args.ticks).observation
            return {"tick": obs.tick, "position": [obs.position.x, obs.position.z], "state_hash": f"{sim.state_hash():016x}"}
    if args.command == "compare":
        return _compare(args.left, args.right)
    from .replay import read_trace, scenario_names, verify_trace

    if args.command == "list-scenarios":
        return {"scenarios": scenario_names()}
    if args.command == "validate-trace":
        trace = read_trace(args.trace)
        return {"valid": True, "scenario": trace["scenario"], "decisions": len(trace["steps"])}
    if args.command == "replay":
        return verify_trace(read_trace(args.trace))

    from .benchmark import EVALUATION_START, run_benchmark

    default_seed = EVALUATION_START if args.split == "evaluation" else 42
    result = run_benchmark(args.scenario, args.agent, args.episodes,
                           args.seed if args.seed is not None else default_seed,
                           split=args.split, trace_directory=args.trace_dir)
    if args.json_path:
        args.json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.csv_path:
        with args.csv_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(result["results"][0]))
            writer.writeheader()
            writer.writerows(result["results"])
    return {key: value for key, value in result.items() if key != "results"}


def main() -> None:
    parser = _commands()
    try:
        result = _execute(parser.parse_args())
    except ModuleNotFoundError as error:
        if error.name not in ("gymnasium", "numpy"):
            raise
        parser.exit(1, "Install the faultline-sim gym extra for this operation.\n")
    except (OSError, KeyError, TypeError, RuntimeError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
