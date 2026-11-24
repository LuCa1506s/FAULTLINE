"""FL-Adaptation, a navigation task with a changing route constraint."""

import numpy as np
from gymnasium import spaces
from numpy.typing import NDArray

from . import Observation
from .navigation import NavigationEnv, NavigationObservation, WORLD_BOUND

CHANGE_DECISION = 4


class AdaptationObservation(NavigationObservation):
    exclusion: NDArray[np.int32]


class AdaptationEnv(NavigationEnv):
    """Navigate the generated city after a region becomes off limits."""

    def __init__(self, max_decisions: int = 256) -> None:
        super().__init__(max_decisions)
        if max_decisions <= CHANGE_DECISION:
            raise ValueError("FL-Adaptation requires enough decisions after the condition changes")
        self.observation_space = spaces.Dict({
            **self.observation_space.spaces,
            "exclusion": spaces.Box(-WORLD_BOUND, WORLD_BOUND, (4,), np.int32),
        })

    def reset(self, *, seed=None, options=None):
        self._zone = np.zeros(4, dtype=np.int32)
        _, info = super().reset(seed=seed, options=options)
        eligible_axes = []
        for values, count in zip(self._road_axes, self._axis_counts):
            eligible = [int(coordinate) for coordinate in values[:count] if 24 <= abs(coordinate) <= 120]
            if not eligible:
                eligible = [int(coordinate) for coordinate in values[:count] if abs(coordinate) >= 24]
                closest = min(abs(coordinate) for coordinate in eligible)
                eligible = [coordinate for coordinate in eligible if abs(coordinate) == closest]
            eligible_axes.append(eligible)
        goal_x, goal_z = [values[int(self.np_random.integers(len(values)))] for values in eligible_axes]
        self._goal = np.asarray([goal_x, goal_z], dtype=np.float32)
        assert self._sim is not None
        return self._observation(self._sim.observe()), info

    def _observation(self, native: Observation) -> AdaptationObservation:
        base = super()._observation(native)
        exclusion = self._zone.copy() if self._decisions >= CHANGE_DECISION else np.zeros(4, dtype=np.int32)
        return {**base, "exclusion": exclusion}

    def step(self, action):
        obs, reward, terminated, truncated, info = super().step(action)
        if self._decisions == CHANGE_DECISION:
            along_goal = obs["position"] * np.sign(self._goal)
            axis = int(along_goal[1] > along_goal[0])
            target = int(self._goal[axis])
            center = min(24, abs(target) - 8) * (1 if target > 0 else -1)
            bounds = [center - 2, -4, center + 2, 4] if axis == 0 else [-4, center - 2, 4, center + 2]
            self._zone = np.asarray(bounds, dtype=np.int32)
            obs["exclusion"] = self._zone.copy()
        if self._decisions >= CHANGE_DECISION:
            x, z = obs["position"]
            low_x, low_z, high_x, high_z = self._zone
            if low_x <= x <= high_x and low_z <= z <= high_z:
                self._active = False
                return obs, -25.0, True, False, {**info, "success": False}
        return obs, reward, terminated, truncated, info
