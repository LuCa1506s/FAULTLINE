"""Repeatable CPU evaluation over a declared world-seed partition."""

from collections.abc import Callable
from math import ceil
from operator import index
from pathlib import Path
from random import Random
from statistics import mean
import sys
from time import perf_counter
from typing import Any

from .agents import heuristic_action, planning_action, random_action
from .llm import TextAgent, local_backend
from .navigation import NavigationObservation
from .replay import record_episode, runtime_identity, verify_trace, write_trace

AGENTS = ("random", "heuristic", "planning", "local-text")
EVALUATION_START = 10_000
MAX_EPISODES = 1000


def _peak_process_kib() -> int | None:
    if sys.platform != "linux":
        return None
    from resource import RUSAGE_SELF, getrusage

    return getrusage(RUSAGE_SELF).ru_maxrss


def _agent(name: str, seed: int) -> Callable[[NavigationObservation], int]:
    if name == "random":
        rng = Random(seed)
        return lambda obs: random_action(obs, rng)
    if name == "heuristic":
        return heuristic_action
    if name == "planning":
        return planning_action
    if name == "local-text":
        return TextAgent(local_backend)
    raise ValueError("Unknown benchmark agent")


def run_benchmark(scenario: str, agent: str, episodes: int, seed: int, *, split: str = "development",
                  trace_directory: Path | None = None) -> dict[str, Any]:
    """Evaluate contiguous seeds, then verify the recorded actions of every episode."""
    if type(episodes) is not int or not 1 <= episodes <= MAX_EPISODES:
        raise ValueError(f"episodes must be between 1 and {MAX_EPISODES}")
    if type(seed) is not int or not 0 <= seed <= 2**64 - episodes:
        raise ValueError("seed range must fit in unsigned 64-bit world seeds")
    if not ((split == "development" and seed + episodes <= EVALUATION_START) or
            (split == "evaluation" and seed >= EVALUATION_START)):
        raise ValueError("seed range does not fit the chosen development/evaluation split")

    rows = []
    for world_seed in range(seed, seed + episodes):
        policy = _agent(agent, world_seed)
        invalid = 0
        agent_seconds = 0.0

        def decide(obs: NavigationObservation) -> int:
            nonlocal invalid, agent_seconds
            started = perf_counter()
            action = policy(obs)
            agent_seconds += perf_counter() - started
            try:
                value = index(action) if not isinstance(action, bool) else -1
            except TypeError:
                value = -1
            if not 0 <= value <= 4:
                invalid += 1
                return 0
            return value

        started = perf_counter()
        trace = record_episode(scenario, world_seed, decide)
        run_seconds = perf_counter() - started
        started = perf_counter()
        terminal = verify_trace(trace)
        replay_seconds = perf_counter() - started
        decisions = len(trace["steps"])
        minimum = sum(ceil(max(0, abs(coordinate) - 1) / 2) for coordinate in trace["initial"]["goal"])
        rows.append({"scenario": scenario, "agent": agent, "seed": world_seed, "success": terminal["success"],
                     "reward": sum(step["reward"] for step in trace["steps"]), "decisions": decisions,
                     "invalid_actions": invalid, "ticks": terminal["tick"],
                     "efficiency": minimum / decisions if terminal["success"] else 0.0,
                     "agent_seconds": agent_seconds, "environment_and_record_seconds": run_seconds - agent_seconds,
                     "run_seconds": run_seconds, "replay_seconds": replay_seconds, "replay_verified": True,
                     "state_hash": terminal["state_hash"]})
        if trace_directory is not None:
            trace_directory.mkdir(parents=True, exist_ok=True)
            write_trace(trace_directory / f"{scenario}-{agent}-{world_seed}.json", trace)

    summary = {"success_rate": mean(row["success"] for row in rows),
               "replay_rate": mean(row["replay_verified"] for row in rows)}
    for field in ("reward", "decisions", "invalid_actions", "ticks", "efficiency", "agent_seconds",
                  "environment_and_record_seconds", "run_seconds", "replay_seconds"):
        summary[f"mean_{field}"] = mean(row[field] for row in rows)
    return {"version": 1, "runtime": runtime_identity(), "scenario": scenario, "agent": agent,
            "split": split, "first_seed": seed, "episodes": episodes, "summary": summary,
            "peak_process_rss_kib": _peak_process_kib(), "results": rows}
