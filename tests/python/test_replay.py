import copy
import json
import subprocess
import sys

import pytest

from faultline_sim.agents import heuristic_action
from faultline_sim.continuity import CHECKPOINT_DECISION, ContinuityEnv
from faultline_sim.replay import MAX_TRACE_BYTES, TraceError, read_trace, record_episode, verify_trace, write_trace


def test_continuous_and_restored_runs_match():
    continuous = record_episode("navigation", 42, heuristic_action)
    restored = record_episode("continuity", 42, heuristic_action)
    assert continuous["checkpoints"] == 0
    assert restored["checkpoints"] == 1
    assert restored["initial"] == continuous["initial"]
    assert restored["steps"] == continuous["steps"]
    assert restored["steps"][-1]["success"]


def test_policy_can_restart_from_public_observation():
    env = ContinuityEnv()
    try:
        observation, _ = env.reset(seed=42)
        # Drop the policy object at the interruption. The new callable's input is
        # the public observation; engine and scenario state are owned by the host.
        policy = lambda obs: heuristic_action(obs)
        for _ in range(CHECKPOINT_DECISION):
            observation, *_ = env.step(policy(observation))
        assert env.checkpoint_count == 1
        del policy
        while True:
            observation, _, terminated, truncated, info = env.step(heuristic_action(observation))
            if terminated or truncated:
                assert info["success"]
                break
    finally:
        env.close()


def test_independent_process_replay(tmp_path):
    path = tmp_path / "episode.json"
    trace = record_episode("continuity", 12345, heuristic_action)
    write_trace(path, trace)
    assert read_trace(path) == trace
    completed = subprocess.run(
        [sys.executable, "-I", "-m", "faultline_sim.replay", str(path)],
        capture_output=True, text=True, check=True, timeout=30,
    )
    assert json.loads(completed.stdout) == trace["steps"][-1]


def test_invalid_and_divergent_traces_are_rejected(tmp_path):
    path = tmp_path / "episode.json"
    trace = record_episode("navigation", 42, heuristic_action)
    wrong_version = copy.deepcopy(trace)
    wrong_version["version"] += 1
    with pytest.raises(TraceError, match="version"):
        verify_trace(wrong_version)
    wrong_runtime = copy.deepcopy(trace)
    wrong_runtime["runtime"]["extension_sha256"] = "0" * 64
    with pytest.raises(TraceError, match="different runtime"):
        verify_trace(wrong_runtime)
    wrong_step = copy.deepcopy(trace)
    wrong_step["steps"][1]["state_hash"] = "0" * 16
    write_trace(path, wrong_step)
    with pytest.raises(TraceError, match="decision 2 .*state_hash"):
        verify_trace(read_trace(path))
    envelope = json.loads(path.read_bytes())
    envelope["payload"]["seed"] += 1
    path.write_text(json.dumps(envelope), encoding="utf-8")
    with pytest.raises(TraceError, match="checksum"):
        read_trace(path)
    path.write_bytes(b" " * (MAX_TRACE_BYTES + 1))
    with pytest.raises(TraceError, match="byte limit"):
        read_trace(path)
