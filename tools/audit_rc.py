"""Audit the frozen 0.1.0b1 kit and reproduce its results with an installed wheel."""

import argparse
import csv
from hashlib import sha256
from itertools import product
import json
from math import isclose
from pathlib import Path
from statistics import mean
import sys
from time import perf_counter
from zipfile import ZipFile


CASES = tuple(product(("navigation", "continuity", "adaptation"), ("random", "heuristic", "planning")))
SCORES = ("seed", "success", "reward", "decisions", "invalid_actions", "ticks", "efficiency", "replay_verified", "state_hash")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load(path):
    return json.loads(path.read_bytes())


def static_audit(kit, repository):
    from package_manifest import source_info, verify

    verify(kit)
    manifest = load(kit / "manifest.json")
    source = source_info(repository)
    require(source["source_sha256"] == manifest["source"]["source_sha256"], "Working-tree source fingerprint differs")
    if source["git_base"] is not None:
        require(source["git_base"] == manifest["source"]["git_base"], "Working-tree Git base differs")
    rows_checked = 0
    for platform in ("windows", "linux"):
        wheel, = (kit / platform).glob("*.whl")
        with ZipFile(wheel) as archive:
            binary, = [name for name in archive.namelist() if name.endswith((".pyd", ".so"))]
            digest = sha256(archive.read(binary)).hexdigest()
            for source in (repository / "python/faultline_sim").iterdir():
                if source.suffix in (".py", ".pyi") or source.name == "py.typed":
                    require(source.read_bytes() == archive.read(f"faultline_sim/{source.name}"), f"Wheel mismatch: {source.name}")
            for source in ("LICENSE", "NOTICE", "LICENSING.md", "THIRD_PARTY_LICENSES.md", "docs/agent_framework/third_party/pybind11.LICENSE"):
                member, = [name for name in archive.namelist() if name.endswith(f".dist-info/licenses/{source}")]
                require(archive.read(member) == (repository / source).read_bytes(), f"License mismatch: {source}")
        directory = kit / "benchmarks" / platform
        qualification = load(directory / "qualification-summary.json")
        headers = {}
        for scenario, agent in CASES:
            case = directory / f"{scenario}-{agent}"
            report = load(case.with_suffix(".json"))
            with case.with_suffix(".csv").open(newline="", encoding="utf-8") as stream:
                csv_rows = list(csv.DictReader(stream))
            rows = report.pop("results")
            require(report["runtime"]["extension_sha256"] == digest, f"Native digest mismatch: {case}")
            require(report["split"] == "evaluation" and report["first_seed"] == 11000 and report["episodes"] == 100,
                    f"Unexpected evaluation range: {case}")
            require([row["seed"] for row in rows] == list(range(11000, 11100)), f"Noncontiguous seeds: {case}")
            require(csv_rows == [{key: str(value) for key, value in row.items()} for row in rows], f"JSON/CSV mismatch: {case}")
            for key, value in report["summary"].items():
                metric = {"success_rate": "success", "replay_rate": "replay_verified"}.get(key, key.removeprefix("mean_"))
                require(isclose(value, mean(row[metric] for row in rows), rel_tol=1e-13), f"Wrong aggregate: {case} {key}")
            headers[(scenario, agent)] = report
            rows_checked += len(rows)
        require(headers == {(row["scenario"], row["agent"]): row for row in qualification["benchmarks"]},
                f"Qualification differs from per-case reports: {platform}")
    print(f"Static audit checked {rows_checked} JSON/CSV rows, all aggregates and both wheel digests")


def native_oracle(trace):
    """Check the scenario's recorded movement and reward through the public native API."""
    from faultline_sim import Action, Config, Simulation, Status, WorldProfile

    goal = trace["initial"]["goal"]
    zone = [0, 0, 0, 0]
    with Simulation(trace["seed"], Config(WorldProfile.COMPACT)) as engine:
        require(f"{engine.state_hash():016x}" == trace["initial"]["state_hash"], "Native initial hash differs")
        for decision, step in enumerate(trace["steps"], 1):
            native = engine.step(Action(step["action"]), 8).observation
            x, z = native.position.x, native.position.z
            require(step["position"] == [x, z] and step["tick"] == native.tick and
                    step["state_hash"] == f"{engine.state_hash():016x}", f"Native divergence at decision {decision}")
            prohibited = False
            if trace["scenario"] == "adaptation":
                if decision == 4:
                    axis = int(z * (1 if goal[1] > 0 else -1) > x * (1 if goal[0] > 0 else -1))
                    target = goal[axis]
                    center = min(24, abs(target) - 8) * (1 if target > 0 else -1)
                    zone = [-4, center - 2, 4, center + 2] if axis else [center - 2, -4, center + 2, 4]
                require(step["exclusion"] == zone, f"Exclusion differs at decision {decision}")
                prohibited = decision >= 4 and zone[0] <= x <= zone[2] and zone[1] <= z <= zone[3]
            success = native.status is Status.RUNNING and max(abs(x - goal[0]), abs(z - goal[1])) <= 1 and not prohibited
            terminated = success or prohibited or native.status is Status.INCAPACITATED
            truncated = not terminated and decision == trace["config"]["max_decisions"]
            reward = -25 if prohibited else 99 if success else -1
            require((step["reward"], step["success"], step["terminated"], step["truncated"]) ==
                    (reward, success, terminated, truncated), f"Reward or terminal state differs at decision {decision}")


def behavior(output):
    from faultline_sim.agents import heuristic_action, planning_action
    from faultline_sim.benchmark import run_benchmark
    from faultline_sim.replay import record_episode, verify_trace

    details = {}
    for scenario, keys in (("navigation", set()), ("continuity", set()), ("adaptation", {"exclusion"})):
        expected = {"position", "goal", "road_axes", "axis_counts"} | keys

        def mutate_after_action(observation):
            require(set(observation) == expected, "Unexpected field crossed policy boundary")
            action = planning_action(observation)
            for value in observation.values():
                value.fill(0)
            return action

        seed = 22003
        reference = record_episode(scenario, seed, planning_action)
        require(reference == record_episode(scenario, seed, mutate_after_action), "Observation aliased evaluator state")
        if not keys:
            require(reference == record_episode(scenario, seed, heuristic_action), "Baseline delegation differs")
        details[scenario] = {"observation_fields": sorted(expected), "observation_mutations_isolated": True}

    for split, first_seed in (("development", 9999), ("evaluation", 9999)):
        try:
            run_benchmark("navigation", "random", 2, first_seed, split=split)
        except ValueError as error:
            require("split" in str(error), "Wrong seed partition rejection")
        else:
            raise ValueError("Cross-partition seed range accepted")

    def predetermined_detour():
        actions = 0

        def policy(observation):
            nonlocal actions
            actions += 1
            if actions <= 4:
                return 0
            dx, dz = observation["goal"] - observation["position"]
            if abs(dz) > 1:
                return 3 if dz > 0 else 4
            if abs(dx) > 1:
                return 1 if dx > 0 else 2
            return 0

        return policy

    episodes = [record_episode("adaptation", seed, predetermined_detour()) for seed in range(22000, 22010)]
    details["adaptation"]["observation_blind_detour"] = sum(verify_trace(trace)["success"] for trace in episodes)
    details["adaptation"]["detour_episodes"] = len(episodes)
    output.write_text(json.dumps(details, indent=2), encoding="utf-8")
    print(json.dumps(details))


def exercise(kit, platform, output, case):
    import faultline_sim
    from faultline_sim.benchmark import run_benchmark
    from faultline_sim.replay import read_trace, verify_trace

    require(Path(faultline_sim.__file__).resolve().is_relative_to(Path(sys.prefix).resolve()), "Package is outside fresh environment")
    scenario, agent = case.split("-")
    require((scenario, agent) in CASES, "Unknown case")
    started = perf_counter()
    original = load(kit / "benchmarks" / platform / f"{case}.json")
    result = run_benchmark(scenario, agent, 100, 11000, split="evaluation")
    require(result["runtime"] == original["runtime"], "Fresh environment runtime differs from frozen reference")
    for reference, actual in zip(original["results"], result["results"], strict=True):
        require(all(reference[key] == actual[key] for key in SCORES), f"Case {case} deviated at seed {actual['seed']}")
    traces = output / "traces"
    new = run_benchmark(scenario, agent, 3, 22000, split="evaluation", trace_directory=traces)
    for seed in range(22000, 22003):
        trace = read_trace(traces / f"{case}-{seed}.json")
        native_oracle(trace)
        verify_trace(trace)
    report = {"historical_rows_matched": 100, "new_seed_runs": new, "seconds": perf_counter() - started}
    (output / f"{case}.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"{platform} {case}: 100 historical rows, three new traces and native reward checks passed")


def replay_archive(directory):
    from faultline_sim.replay import read_trace, verify_trace

    paths = sorted(directory.glob("*.json"))
    require(paths, f"No trace files in {directory}")
    for path in paths:
        verify_trace(read_trace(path))
    print(f"Independent process verified {len(paths)} traces in {directory}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kit", type=Path)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--platform", choices=("windows", "linux"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--case")
    parser.add_argument("--behavior", action="store_true")
    parser.add_argument("--replay-dir", type=Path)
    args = parser.parse_args()
    if args.behavior:
        if args.output is None:
            parser.error("--behavior requires --output")
        behavior(args.output)
    elif args.replay_dir:
        replay_archive(args.replay_dir)
    elif args.platform:
        if args.case is None or args.output is None:
            parser.error("--platform requires --case and --output")
        exercise(args.kit, args.platform, args.output, args.case)
    else:
        static_audit(args.kit, args.repo)


if __name__ == "__main__":
    main()
