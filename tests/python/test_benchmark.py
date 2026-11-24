import json
import subprocess
import sys

from gymnasium.utils.env_checker import check_env
import pytest

from faultline_sim.adaptation import AdaptationEnv, CHANGE_DECISION
from faultline_sim.agents import heuristic_action, planning_action
from faultline_sim.benchmark import run_benchmark
from faultline_sim.llm import TextAgent, local_backend
from faultline_sim.navigation import NavigationEnv
from faultline_sim.replay import record_episode, verify_trace


def test_adaptation_requires_a_route_change():
    env = AdaptationEnv()
    try:
        check_env(env, skip_render_check=True, skip_close_check=True)
    finally:
        env.close()
    greedy = record_episode("adaptation", 42, heuristic_action)
    plan = record_episode("adaptation", 42, planning_action)
    assert greedy["steps"][:CHANGE_DECISION] == plan["steps"][:CHANGE_DECISION]
    assert plan["steps"][CHANGE_DECISION - 2]["exclusion"] == [0, 0, 0, 0]
    assert any(plan["steps"][CHANGE_DECISION - 1]["exclusion"])
    assert greedy["steps"][-1]["reward"] == -25 and not greedy["steps"][-1]["success"]
    assert verify_trace(plan)["success"]

    def z_first(obs):
        dx, dz = obs["goal"] - obs["position"]
        if abs(dz) > 1:
            return 3 if dz > 0 else 4
        return 1 if dx > 0 else 2

    vertical = record_episode("adaptation", 42, z_first)
    assert vertical["steps"][-1]["reward"] == -25
    assert vertical["steps"][CHANGE_DECISION - 1]["exclusion"] != greedy["steps"][CHANGE_DECISION - 1]["exclusion"]


def test_llm_adapter_uses_only_public_values():
    env = NavigationEnv()
    try:
        observation, _ = env.reset(seed=42)
        def backend(raw):
            request = json.loads(raw)
            assert set(request["observation"]) == set(observation)
            assert set(request) == {"instruction", "actions", "observation"}
            return local_backend(raw)

        assert TextAgent(backend)(observation) == heuristic_action(observation)
        for invalid in ('{"action":1,"action":2}', '{"action":true}', '{"action":5}', 'x' * 129):
            assert TextAgent(lambda _: invalid)(observation) == -1
    finally:
        env.close()


def test_benchmark_seed_partition_and_verified_rows():
    with pytest.raises(ValueError, match="split"):
        run_benchmark("navigation", "heuristic", 2, 9999)
    with pytest.raises(ValueError, match="split"):
        run_benchmark("navigation", "heuristic", 1, 42, split="evaluation")
    report = run_benchmark("continuity", "local-text", 2, 42)
    assert report["summary"]["success_rate"] == report["summary"]["replay_rate"] == 1
    assert [episode["seed"] for episode in report["results"]] == [42, 43]
    assert all(row["ticks"] == 8 * row["decisions"] for row in report["results"])


def test_cli_exports_and_compares(tmp_path):
    def invoke(*args):
        return json.loads(subprocess.run([sys.executable, "-I", "-m", "faultline_sim", *map(str, args)],
                                        capture_output=True, text=True, timeout=30, check=True).stdout)

    data = tmp_path / "data.json"
    csv = tmp_path / "data.csv"
    assert set(invoke("list-scenarios")["scenarios"]) == {"navigation", "continuity", "adaptation"}
    assert invoke("simulate", "--ticks", 8)["tick"] == 8
    report = invoke("benchmark", "--scenario", "navigation", "--agent", "planning",
                    "--json", data, "--csv", csv, "--trace-dir", tmp_path)
    assert report["summary"]["success_rate"] == 1
    assert len(csv.read_text().splitlines()) == 2
    trace = tmp_path / "navigation-planning-42.json"
    assert invoke("validate-trace", trace)["valid"]
    assert invoke("replay", trace)["success"]
    assert invoke("compare", data, data)["right_minus_left"]["success_rate"] == 0
