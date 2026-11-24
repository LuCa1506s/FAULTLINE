"""Run and replay the versioned scientific route challenges."""

from argparse import ArgumentParser
import csv
from hashlib import sha256
import json
from math import sqrt
from pathlib import Path
from random import Random
from statistics import mean
from tempfile import NamedTemporaryFile
from time import perf_counter

import numpy as np

from .agents import planning_action as legacy_planning_action
from .qualification import LEVELS, MAX_DECISIONS, PROTOCOL, SCENARIOS, RouteChallenge, axis_action, graph_action
from .replay import _unique_object, runtime_identity

AGENTS = ("random", "heuristic", "legacy-planning", "planning")
HOLDOUT_START = 31000
HOLDOUT_COUNT = 100
HASH_INTERVAL = 16
MAX_TRACE_BYTES = 2**20


def run_episode(scenario, level, seed, policy, recorded_steps=None):
    """Execute one episode, returning a trace and separate policy/runtime timings."""
    start = perf_counter()
    active = RouteChallenge(scenario, level, seed)
    policy_seconds = 0.0
    trace = {"protocol": PROTOCOL, "runtime": runtime_identity(), "scenario": scenario, "level": level,
             "seed": seed, "initial_hash": f"{active.engine.state_hash():016x}", "budget": active.budget,
             "reference_decisions": active.reference, "steps": []}
    try:
        records = (None for _ in range(active.budget)) if recorded_steps is None else recorded_steps
        for index, expected in enumerate(records, 1):
            if active.closed:
                raise ValueError(f"Actions after the terminal boundary at decision {index}")
            observation = active.observation()
            if recorded_steps is None:
                before = perf_counter()
                requested = policy(observation)
                policy_seconds += perf_counter() - before
                invalid = type(requested) is not int or not 0 <= requested <= 4
                action = 0 if invalid else requested
            else:
                action, invalid = expected["action"], expected["invalid"]
            outcome = active.step(action)
            outcome["position"] = list(outcome["position"])
            outcome["invalid"] = invalid
            outcome["state_hash"] = (f"{active.engine.state_hash():016x}"
                                     if active.closed or index % HASH_INTERVAL == 0 else None)
            if recorded_steps is not None and (type(invalid) is not bool or (invalid and action != 0) or
                                               _canonical(outcome) != _canonical(expected)):
                raise ValueError(f"Replay diverged at decision {index}, tick {outcome['tick']}")
            trace["steps"].append(outcome)
            if active.closed and recorded_steps is None:
                break
        if not active.closed:
            raise ValueError("Replay ended before the terminal boundary")
        trace["checkpoints"] = active.checkpoints
        return trace, policy_seconds, perf_counter() - start
    finally:
        active.close()


def verify_episode(trace):
    if trace["protocol"] != PROTOCOL or trace["runtime"] != runtime_identity():
        raise ValueError("Incompatible protocol or runtime")
    if type(trace["steps"]) is not list or not 1 <= len(trace["steps"]) <= MAX_DECISIONS:
        raise ValueError("Invalid trace decision count")
    actual, _, _ = run_episode(trace["scenario"], trace["level"], trace["seed"], None, trace["steps"])
    if _canonical(actual) != _canonical(trace):
        raise ValueError("Initial state, configuration or checkpoint count diverged")
    return actual["steps"][-1]


def _canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("ascii")


def write_replay(path, trace):
    payload = _canonical({"trace": trace, "sha256": sha256(_canonical(trace)).hexdigest()})
    if len(payload) > MAX_TRACE_BYTES:
        raise ValueError("Trace exceeds its byte limit")
    destination = Path(path)
    staged = None
    try:
        with NamedTemporaryFile(dir=destination.parent, prefix=".faultline-", delete=False) as stream:
            staged = Path(stream.name)
            stream.write(payload)
        staged.replace(destination)
    finally:
        if staged is not None:
            staged.unlink(missing_ok=True)


def read_replay(path):
    with Path(path).open("rb") as stream:
        payload = stream.read(MAX_TRACE_BYTES + 1)
    if len(payload) > MAX_TRACE_BYTES:
        raise ValueError("Trace exceeds its byte limit")
    envelope = json.loads(payload, object_pairs_hook=_unique_object)
    if type(envelope) is not dict or set(envelope) != {"trace", "sha256"}:
        raise ValueError("Invalid trace envelope")
    trace = envelope["trace"]
    if sha256(_canonical(trace)).hexdigest() != envelope["sha256"]:
        raise ValueError("Trace checksum mismatch")
    return trace


def wilson(successes, count):
    z = 1.959963984540054
    p = successes / count
    adjusted = 1 + z*z / count
    center = (p + z*z/(2*count)) / adjusted
    radius = z * sqrt((p*(1-p) + z*z/(4*count)) / count) / adjusted
    return [max(0.0, center - radius), min(1.0, center + radius)]


def _legacy_observation(obs):
    if not obs["closures"]:
        return axis_action(obs)
    axes = obs["road_axes"]
    width = max(map(len, axes))
    return legacy_planning_action({"goal": np.asarray(obs["goal"]), "position": np.asarray(obs["position"]),
                                   "road_axes": np.asarray([axis + (0,) * (width - len(axis)) for axis in axes]),
                                   "axis_counts": np.asarray([len(axis) for axis in axes]),
                                   "exclusion": np.asarray(obs["closures"][0])})


def evaluate(scenario, level, agent, first_seed, count, *, split="development", trace_dir=None):
    if agent not in AGENTS or type(count) is not int or not 1 <= count <= 1000:
        raise ValueError("Invalid agent or episode count")
    if type(first_seed) is not int or not 0 <= first_seed <= 2**64 - count:
        raise ValueError("Invalid unsigned seed range")
    if not ((split == "development" and first_seed + count <= HOLDOUT_START) or
            (split == "evaluation" and first_seed >= HOLDOUT_START)):
        raise ValueError("World seeds overlap the other partition")
    rows = []
    for offset in range(count):
        seed = first_seed + offset
        rng = Random(sha256(f"{PROTOCOL}:policy:{offset}".encode()).digest())
        policy = {"random": lambda _: rng.randrange(5), "heuristic": axis_action,
                  "legacy-planning": _legacy_observation, "planning": graph_action}[agent]
        trace, policy_time, duration = run_episode(scenario, level, seed, policy)
        replay_start = perf_counter()
        terminal = verify_episode(trace)
        replay_time = perf_counter() - replay_start
        steps = trace["steps"]
        rows.append({"seed": seed, "success": terminal["success"], "failure": terminal["failure"],
                     "reward": sum(row["reward"] for row in steps), "decisions": len(steps),
                     "ticks": terminal["tick"], "stops": terminal["stops_completed"],
                     "checkpoints": trace["checkpoints"], "errors": sum(row["invalid"] for row in steps),
                     "road_efficiency": trace["reference_decisions"] / len(steps) if terminal["success"] else 0.0,
                     "policy_seconds": policy_time, "environment_seconds": duration - policy_time,
                     "replay_seconds": replay_time, "state_hash": terminal["state_hash"], "replay_verified": True})
        if trace_dir is not None:
            trace_dir.mkdir(parents=True, exist_ok=True)
            write_replay(trace_dir / f"{scenario}-{level}-{agent}-{seed}.json", trace)
    successes = sum(row["success"] for row in rows)
    summary = {"success_rate": successes / count, "wilson_95": wilson(successes, count),
               "failures": {kind: sum(row["failure"] == kind for row in rows) for kind in ("closure", "incapacitated", "budget")},
               "replay_rate": mean(row["replay_verified"] for row in rows)}
    for field in ("reward", "decisions", "ticks", "stops", "checkpoints", "errors", "road_efficiency", "policy_seconds",
                  "environment_seconds", "replay_seconds"):
        summary[f"mean_{field}"] = mean(row[field] for row in rows)
    return {"protocol": PROTOCOL, "runtime": runtime_identity(), "scenario": scenario, "level": level, "agent": agent,
            "split": split, "first_seed": first_seed, "count": count, "summary": summary, "results": rows}


def feasibility_witness(scenario, level, seed):
    """Wait for each announced restriction, then route with public observations."""
    actions = 0
    stops = 0

    def policy(observation):
        nonlocal actions, stops
        if observation["stops_completed"] != stops:
            stops = observation["stops_completed"]
            actions = 0
        actions += 1
        if scenario == "adaptation" and actions <= 4:
            return 0
        return graph_action(observation)

    trace, _, _ = run_episode(scenario, level, seed, policy)
    return verify_episode(trace)


def main():
    parser = ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run")
    run.add_argument("--scenario", choices=SCENARIOS, required=True)
    run.add_argument("--level", choices=LEVELS, required=True)
    run.add_argument("--agent", choices=AGENTS, required=True)
    run.add_argument("--seed", type=int, default=42)
    run.add_argument("--episodes", type=int, default=1)
    run.add_argument("--split", choices=("development", "evaluation"), default="development")
    run.add_argument("--json", type=Path)
    run.add_argument("--csv", type=Path)
    run.add_argument("--traces", type=Path)
    commands.add_parser("replay").add_argument("path", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "replay":
            result = verify_episode(read_replay(args.path))
        else:
            result = evaluate(args.scenario, args.level, args.agent, args.seed, args.episodes, split=args.split, trace_dir=args.traces)
            if args.json:
                args.json.write_text(json.dumps(result, indent=2), encoding="utf-8")
            if args.csv:
                with args.csv.open("w", newline="", encoding="utf-8") as stream:
                    writer = csv.DictWriter(stream, fieldnames=list(result["results"][0]))
                    writer.writeheader()
                    writer.writerows(result["results"])
            result = result["summary"]
    except (KeyError, OSError, TypeError, ValueError, RecursionError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
