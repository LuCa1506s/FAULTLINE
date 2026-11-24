import copy

import pytest

from faultline_sim.qualification import LEVELS, REVEAL, SCENARIOS, RouteChallenge, graph_action
from faultline_sim.scientific import evaluate, feasibility_witness, read_replay, run_episode, verify_episode, wilson, write_replay


@pytest.mark.parametrize("scenario", SCENARIOS)
@pytest.mark.parametrize("level", LEVELS)
def test_all_levels_physically_solvable(scenario, level):
    result = feasibility_witness(scenario, level, 14)
    assert result["success"]
    assert result["stops_completed"] == LEVELS[level][0]


def test_closure_schedule_independent_of_player_actions():
    stationary = RouteChallenge("adaptation", "STRESS", 14)
    moving = RouteChallenge("adaptation", "STRESS", 14)
    try:
        fields = {"position", "goal", "road_axes", "closures", "stops_completed", "stops_total", "remaining_decisions"}
        for _ in range(REVEAL):
            observation = stationary.observation()
            assert set(observation) == fields and not observation["closures"]
            stationary.step(0)
            moving.step(graph_action(moving.observation()))
        assert stationary.observation()["closures"] == moving.observation()["closures"]
        assert len(stationary.observation()["closures"]) == LEVELS["STRESS"][1]
        assert stationary.observation()["position"] != moving.observation()["position"]
    finally:
        stationary.close()
        moving.close()


def test_replay_and_checkpoint_trace(tmp_path):
    trace, _, _ = run_episode("continuity", "HARD", 14, graph_action)
    assert trace["checkpoints"] == 3
    path = tmp_path / "scientific trace.json"
    write_replay(path, trace)
    assert verify_episode(read_replay(path))["success"]
    wrong = copy.deepcopy(trace)
    wrong["steps"][15]["state_hash"] = "0" * 16
    with pytest.raises(ValueError, match="decision 16"):
        verify_episode(wrong)


def test_summary_intervals_and_partition():
    assert wilson(0, 100) == pytest.approx([0, 0.0369935])
    assert wilson(100, 100) == pytest.approx([0.9630065, 1])
    with pytest.raises(ValueError, match="partition"):
        evaluate("navigation", "EASY", "planning", 30999, 2)
    report = evaluate("navigation", "MEDIUM", "planning", 14, 1)
    assert report["summary"]["replay_rate"] == report["summary"]["success_rate"] == 1
