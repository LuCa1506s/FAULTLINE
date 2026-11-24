"""FL-Navigation, a CPU-only Gymnasium environment over the native engine."""

from typing import TypedDict

import gymnasium as gym
from gymnasium import spaces
import numpy as np
from numpy.typing import NDArray

from . import Action, Config, Observation, Simulation, Status, WorldProfile


class NavigationObservation(TypedDict):
    position: NDArray[np.float32]
    goal: NDArray[np.float32]
    road_axes: NDArray[np.int32]
    axis_counts: NDArray[np.int32]


ENV_ID = "FL-Navigation-v0"
SCENARIO_VERSION = 1
DECISION_TICKS = 8
MAX_AXES = 32
WORLD_BOUND = 256
GOAL_TOLERANCE = 1.0
_ACTIONS = (Action.IDLE, Action.EAST, Action.WEST, Action.SOUTH, Action.NORTH)
_COMPACT = Config(WorldProfile.COMPACT)


class NavigationEnv(gym.Env[NavigationObservation, int]):
    """Reach a road junction using discrete commands through the native player."""

    metadata = {"render_modes": []}

    def __init__(self, max_decisions: int = 256) -> None:
        if type(max_decisions) is not int or not 1 <= max_decisions <= 512:
            raise ValueError("max_decisions must be an integer between 1 and 512")
        self.max_decisions = max_decisions
        self.action_space = spaces.Discrete(len(_ACTIONS))
        self.observation_space = spaces.Dict({
            "position": spaces.Box(-WORLD_BOUND, WORLD_BOUND, (2,), np.float32),
            "goal": spaces.Box(-WORLD_BOUND, WORLD_BOUND, (2,), np.float32),
            "road_axes": spaces.Box(-WORLD_BOUND, WORLD_BOUND, (2, MAX_AXES), np.int32),
            "axis_counts": spaces.Box(1, MAX_AXES, (2,), np.int32),
        })
        self.render_mode = None
        self._sim: Simulation | None = None
        self._decisions = 0
        self._active = False

    def reset(self, *, seed: int | None = None, options: dict | None = None) -> tuple[NavigationObservation, dict]:
        if seed is not None and (type(seed) is not int or not 0 <= seed < 2**64):
            raise ValueError("seed must fit in an unsigned 64-bit integer")
        if options:
            raise ValueError("FL-Navigation does not define reset options")
        super().reset(seed=seed)
        world_seed = seed if seed is not None else int(self.np_random.bit_generator.random_raw())
        self._active = False
        if self._sim is None:
            self._sim = Simulation(world_seed, _COMPACT)
        else:
            self._sim.reset(world_seed, _COMPACT)
        native = self._sim.observe()
        nodes = [(int(point.x), int(point.z)) for point in self._sim.road_map().nodes]
        axes = (sorted({x for x, _ in nodes}), sorted({z for _, z in nodes}))
        self._axis_counts = np.asarray([len(axis) for axis in axes], dtype=np.int32)
        if np.any(self._axis_counts > MAX_AXES):
            raise RuntimeError("Generated world exceeded the scenario road-axis limit")
        self._road_axes = np.zeros((2, MAX_AXES), dtype=np.int32)
        for index, axis in enumerate(axes):
            self._road_axes[index, :len(axis)] = axis

        offsets = [(x, z, abs(x - native.position.x), abs(z - native.position.z)) for x, z in nodes]
        goals = [(x, z) for x, z, dx, dz in offsets if min(dx, dz) >= 12 and dx + dz <= 224]
        if not goals:
            goals = [(x, z) for x, z, dx, dz in offsets if 12 <= dx + dz <= 256]
        self._goal = np.asarray(goals[int(self.np_random.integers(len(goals)))], dtype=np.float32)
        self._decisions = 0
        self._active = True
        return self._observation(native), {"tick": native.tick, "success": False}

    def _observation(self, native: Observation) -> NavigationObservation:
        return {
            "position": np.asarray([native.position.x, native.position.z], dtype=np.float32),
            "goal": self._goal.copy(),
            "road_axes": self._road_axes.copy(),
            "axis_counts": self._axis_counts.copy(),
        }

    def step(self, action: int) -> tuple[NavigationObservation, float, bool, bool, dict]:
        if self._sim is None or not self._active:
            raise RuntimeError("Reset the environment before stepping")
        if isinstance(action, bool) or not self.action_space.contains(action):
            raise ValueError("Action must be an integer in Discrete(5)")
        native = self._sim.step(_ACTIONS[int(action)], DECISION_TICKS).observation
        self._decisions += 1
        observation = self._observation(native)
        success = (native.status is Status.RUNNING and
                   bool(np.max(np.abs(observation["position"] - self._goal)) <= GOAL_TOLERANCE))
        terminated = success or native.status is Status.INCAPACITATED
        truncated = not terminated and (self._decisions >= self.max_decisions or native.status is Status.TICK_LIMIT)
        self._active = not (terminated or truncated)
        reward = 99.0 if success else -1.0
        return observation, reward, terminated, truncated, {"tick": native.tick, "success": success}

    def close(self) -> None:
        if self._sim is not None:
            self._sim.close()
            self._sim = None
        self._active = False


gym.register(ENV_ID, entry_point="faultline_sim.navigation:NavigationEnv")
