from random import Random

import gymnasium as gym
from gymnasium.utils.env_checker import check_env
import numpy as np
import pytest

from faultline_sim.agents import heuristic_action, random_action
from faultline_sim.navigation import DECISION_TICKS, ENV_ID, NavigationEnv


def test_gymnasium_environment_checker():
    env = gym.make(ENV_ID)
    try:
        check_env(env.unwrapped)
    finally:
        env.close()


def test_idle_cannot_collect_reward_and_observations_are_isolated():
    env = NavigationEnv(max_decisions=3)
    try:
        obs, info = env.reset(seed=42)
        assert set(obs) == {"position", "goal", "road_axes", "axis_counts"}
        assert set(info) == {"tick", "success"}
        start = obs["position"].copy()
        obs["goal"][:] = start
        obs["road_axes"][:] = 0
        for invalid in (-1, 5, "east", True, float("nan")):
            with pytest.raises(ValueError):
                env.step(invalid)
        for decision in range(1, 4):
            obs, reward, terminated, truncated, info = env.step(np.int64(0))
            assert np.array_equal(obs["position"], start)
            assert not np.array_equal(obs["goal"], start)
            assert info == {"tick": decision * DECISION_TICKS, "success": False}
            assert reward == -1.0 and not terminated
            assert truncated is (decision == 3)
        with pytest.raises(RuntimeError):
            env.step(0)
    finally:
        env.close()


def play(env, seed, decide):
    obs, _ = env.reset(seed=seed)
    decisions = 0
    while True:
        obs, _, terminated, truncated, info = env.step(decide(obs))
        decisions += 1
        assert env.observation_space.contains(obs)
        if terminated or truncated:
            return decisions, bool(info["success"])


def test_heuristic_reaches_generated_goals():
    env = NavigationEnv()
    try:
        for seed in (0, 1, 2, 3, 42, 12345, 2**64 - 1):
            decisions, success = play(env, seed, heuristic_action)
            assert success and 1 < decisions < env.max_decisions, seed
    finally:
        env.close()


def test_seeded_random_agent_is_reproducible_and_bounded():
    env = NavigationEnv(max_decisions=32)
    try:
        first_rng = Random(13)
        first = play(env, 42, lambda obs: random_action(obs, first_rng))
        second_rng = Random(13)
        assert play(env, 42, lambda obs: random_action(obs, second_rng)) == first
        assert first == (32, False)
    finally:
        env.close()
