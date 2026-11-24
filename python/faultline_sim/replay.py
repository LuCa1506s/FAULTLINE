"""Bounded, versioned action traces and independent replay verification."""

from argparse import ArgumentParser
from base64 import b64encode
from collections.abc import Callable, Iterable
from functools import cache
from hashlib import sha256
from importlib.metadata import version
import json
from pathlib import Path
import platform
from tempfile import NamedTemporaryFile
from typing import Any

from . import SnapshotError, _native
from .adaptation import AdaptationEnv
from .continuity import ContinuityEnv
from .navigation import DECISION_TICKS, SCENARIO_VERSION, NavigationEnv, NavigationObservation

TRACE_VERSION = 1
MAX_TRACE_BYTES = 1_048_576
_SCENARIOS = {"navigation": NavigationEnv, "continuity": ContinuityEnv, "adaptation": AdaptationEnv}


def _scenario_version(scenario: str) -> int:
    return 2 if scenario == "adaptation" else SCENARIO_VERSION


def scenario_names() -> tuple[str, ...]:
    return tuple(_SCENARIOS)
Policy = Callable[[NavigationObservation], int]


class TraceError(ValueError):
    """Raised for an invalid, incompatible or divergent trace."""


@cache
def _extension_digest() -> str:
    return sha256(Path(_native.__file__).read_bytes()).hexdigest()


def runtime_identity() -> dict[str, str]:
    return {
        "engine": "0.1.0",
        "distribution": version("faultline-sim"),
        "extension_sha256": _extension_digest(),
        "state_hash_schema": "FAULTLINE.state.v16",
        "platform": platform.system(),
        "architecture": platform.machine(),
        "python": platform.python_version(),
        "gymnasium": version("gymnasium"),
        "numpy": version("numpy"),
    }


def _hash(env: NavigationEnv) -> str:
    assert env._sim is not None
    return f"{env._sim.state_hash():016x}"


def _rollout(env: NavigationEnv, seed: int, actions: Callable[[NavigationObservation], int],
             compare: dict[str, Any] | None = None) -> dict[str, Any]:
    try:
        obs, _ = env.reset(seed=seed)
        assert env._sim is not None
        initial = {"goal": obs["goal"].tolist(), "tick": 0, "state_hash": _hash(env),
                   "snapshot": b64encode(env._sim.snapshot()).decode("ascii")}
        if compare is not None and _canonical(compare["initial"]) != _canonical(initial):
            raise TraceError("Replay diverged at the initial state")
        steps = []
        terminated = truncated = False
        records = compare["steps"] if compare is not None else [None] * env.max_decisions
        for index, expected in enumerate(records, start=1):
            if terminated or truncated:
                raise TraceError(f"Trace contains a decision after termination at step {index}")
            action = actions(obs) if expected is None else expected["action"]
            obs, reward, terminated, truncated, info = env.step(action)
            record = {"action": int(action), "tick": info["tick"], "reward": reward,
                      "terminated": terminated, "truncated": truncated, "success": info["success"],
                      "state_hash": _hash(env), "position": obs["position"].tolist()}
            if isinstance(env, AdaptationEnv):
                record["exclusion"] = obs["exclusion"].tolist()
            if expected is not None and _canonical(expected) != _canonical(record):
                differing = sorted(key for key in record if _canonical(record[key]) != _canonical(expected[key]))
                raise TraceError(f"Replay diverged at decision {index} (tick {info['tick']}): {', '.join(differing)}")
            steps.append(record)
            if compare is None and (terminated or truncated):
                break
        if not (terminated or truncated):
            raise TraceError("Trace ended before its terminal or truncated boundary")
        checkpoints = env.checkpoint_count if isinstance(env, ContinuityEnv) else 0
        return {"initial": initial, "steps": steps, "checkpoints": checkpoints}
    finally:
        env.close()


def record_episode(scenario: str, seed: int, policy: Policy, *, max_decisions: int = 256) -> dict[str, Any]:
    """Record the ordered accepted actions taken by a policy and their outcomes."""
    if scenario not in _SCENARIOS:
        raise ValueError("Unknown replay scenario")
    if type(seed) is not int or not 0 <= seed < 2**64:
        raise ValueError("A replay requires an explicit unsigned 64-bit seed")
    return {
        "version": TRACE_VERSION, "runtime": runtime_identity(),
        "scenario": scenario, "scenario_version": _scenario_version(scenario), "seed": seed,
        "config": {"max_decisions": max_decisions, "decision_ticks": DECISION_TICKS, "profile": "compact"},
        **_rollout(_SCENARIOS[scenario](max_decisions), seed, policy),
    }


def _canonical(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("ascii")


def write_trace(path: str | Path, payload: dict[str, Any]) -> None:
    _validate(payload)
    encoded = _canonical({"payload": payload, "sha256": sha256(_canonical(payload)).hexdigest()}) + b"\n"
    if len(encoded) > MAX_TRACE_BYTES:
        raise TraceError("Trace exceeds its byte limit")
    destination = Path(path)
    staged = None
    try:
        with NamedTemporaryFile(dir=destination.parent, prefix=".faultline-", delete=False) as stream:
            staged = Path(stream.name)
            stream.write(encoded)
        staged.replace(destination)
    finally:
        if staged is not None:
            staged.unlink(missing_ok=True)


def _unique_object(entries: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in entries:
        if key in result:
            raise TraceError("Duplicate key in trace")
        result[key] = value
    return result


def read_trace(path: str | Path) -> dict[str, Any]:
    try:
        with Path(path).open("rb") as stream:
            data = stream.read(MAX_TRACE_BYTES + 1)
        if len(data) > MAX_TRACE_BYTES:
            raise TraceError("Trace exceeds its byte limit")
        envelope = json.loads(data, object_pairs_hook=_unique_object)
        if type(envelope) is not dict or set(envelope) != {"payload", "sha256"}:
            raise TraceError("Invalid trace envelope")
        payload = envelope["payload"]
        if sha256(_canonical(payload)).hexdigest() != envelope["sha256"]:
            raise TraceError("Trace checksum mismatch")
        _validate(payload)
        return payload
    except TraceError:
        raise
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise TraceError(f"Cannot read trace: {error}") from error


def _validate(payload: Any) -> None:
    if type(payload) is not dict or set(payload) != {
        "version", "runtime", "scenario", "scenario_version", "seed", "config", "initial", "steps", "checkpoints"
    }:
        raise TraceError("Invalid trace payload fields")
    if type(payload["version"]) is not int or payload["version"] != TRACE_VERSION:
        raise TraceError("Unsupported trace version")
    if (type(payload["scenario"]) is not str or payload["scenario"] not in _SCENARIOS or
            type(payload["scenario_version"]) is not int or
            payload["scenario_version"] != _scenario_version(payload["scenario"])):
        raise TraceError("Unsupported trace scenario/version")
    if payload["runtime"] != runtime_identity():
        raise TraceError("Trace was recorded with a different runtime")
    if type(payload["seed"]) is not int or not 0 <= payload["seed"] < 2**64:
        raise TraceError("Invalid trace seed")
    config = payload["config"]
    if (type(config) is not dict or set(config) != {"profile", "decision_ticks", "max_decisions"} or
            config["profile"] != "compact" or type(config["decision_ticks"]) is not int or
            config["decision_ticks"] != DECISION_TICKS or
            type(config["max_decisions"]) is not int or not 1 <= config["max_decisions"] <= 512):
        raise TraceError("Invalid trace configuration")
    if type(payload["checkpoints"]) is not int or payload["checkpoints"] not in (0, 1):
        raise TraceError("Invalid trace checkpoint count")
    steps = payload["steps"]
    if type(steps) is not list or not 1 <= len(steps) <= config["max_decisions"]:
        raise TraceError("Invalid trace decision count")
    keys = {"action", "tick", "reward", "terminated", "truncated", "success", "state_hash", "position"}
    if payload["scenario"] == "adaptation":
        keys.add("exclusion")
    for step in steps:
        if type(step) is not dict or set(step) != keys or type(step["action"]) is not int or not 0 <= step["action"] <= 4:
            raise TraceError("Invalid trace decision or action")


def verify_trace(payload: dict[str, Any]) -> dict[str, Any]:
    """Replay a trace from seed and compare every recorded result and engine hash."""
    _validate(payload)
    try:
        env = _SCENARIOS[payload["scenario"]](payload["config"]["max_decisions"])
        actual = _rollout(env, payload["seed"], lambda _: 0, compare=payload)
    except TraceError:
        raise
    except (SnapshotError, RuntimeError, ValueError) as error:
        raise TraceError(f"Replay could not continue: {error}") from error
    if actual["checkpoints"] != payload["checkpoints"]:
        raise TraceError("Replay diverged in checkpoint count")
    return actual["steps"][-1]


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    try:
        result = verify_trace(read_trace(args.trace))
    except TraceError as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
