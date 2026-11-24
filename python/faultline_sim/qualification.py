"""Versioned route challenges over the native FAULTLINE player controller."""

from bisect import bisect_left
from dataclasses import dataclass
from functools import lru_cache
from hashlib import sha256
from heapq import heappop, heappush
from itertools import product
from random import Random

from . import Config, Simulation, Status, WorldProfile
from .navigation import DECISION_TICKS, _ACTIONS

PROTOCOL = "route-challenges-1"
SCENARIOS = ("navigation", "continuity", "adaptation")
LEVELS = {"EASY": (1, 0), "MEDIUM": (2, 1), "HARD": (3, 2), "STRESS": (4, 3)}
REVEAL = 4
MAX_DECISIONS = 4096
Point = tuple[int, int]
Zone = tuple[int, int, int, int]


def _distance(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def _action(start, end):
    if end[0] != start[0]:
        return 1 if end[0] > start[0] else 2
    if end[1] != start[1]:
        return 3 if end[1] > start[1] else 4
    return 0


def _clear(a: Point, b: Point, zones: tuple[Zone, ...]) -> bool:
    return all(max(a[0], b[0]) < x0 or min(a[0], b[0]) > x1 or
               max(a[1], b[1]) < z0 or min(a[1], b[1]) > z1 for x0, z0, x1, z1 in zones)


def _neighbors(point, axes):
    for axis in (0, 1):
        positions = axes[axis]
        index = bisect_left(positions, point[axis])
        on_axis = index < len(positions) and point[axis] == positions[index]
        if point[1 - axis] not in axes[1 - axis]:
            continue
        for next_index in (index - 1, index + int(on_axis)):
            if 0 <= next_index < len(positions):
                next_point = list(point)
                next_point[axis] = positions[next_index]
                yield tuple(next_point)


@lru_cache(maxsize=128)
def _distances(axes, target, zones):
    distances = {target: 0}
    queue = [(0, target)]
    while queue:
        cost, point = heappop(queue)
        if cost != distances[point]:
            continue
        for neighbor in _neighbors(point, axes):
            if not _clear(point, neighbor, zones):
                continue
            proposed = cost + _distance(point, neighbor)
            if proposed < distances.get(neighbor, float("inf")):
                distances[neighbor] = proposed
                heappush(queue, (proposed, neighbor))
    return distances


def graph_action(obs):
    """Route through currently observed native road junctions and exclusions."""
    position, target = obs["position"], obs["goal"]
    axes, zones = obs["road_axes"], obs["closures"]
    distances = _distances(axes, target, zones)
    choices = [(distances[node] + _distance(position, node), node)
               for node in _neighbors(position, axes) if node in distances and _clear(position, node, zones)]
    if not choices:
        return 0
    return _action(position, min(choices)[1])


def axis_action(obs):
    """The original heuristic's X-then-Z route rule."""
    x, z = obs["position"]
    gx, gz = obs["goal"]
    if abs(gx - x) > 1:
        return 1 if gx > x else 2
    if abs(gz - z) > 1:
        return 3 if gz > z else 4
    return 0


@dataclass(frozen=True)
class Leg:
    goal: Point
    closures: tuple[Zone, ...]
    road_decisions: int


def _routes(axes, seed, level, scenario):
    legs, closed_count = LEVELS[level]
    if scenario == "adaptation":
        closed_count = max(1, closed_count)
    nonce = sha256(f"{PROTOCOL}:conditions:{seed}".encode()).digest()
    rng = Random(nonce)
    origin = (0, 0)
    start = origin
    output = []
    # Out-and-back work includes an outbound stop, a return and another stop.
    pools = []
    for axis in axes:
        limit = [value for value in axis if 16 <= abs(value) <= 112]
        fallback = [value for value in axis if abs(value) >= 16]
        pools.append(limit or [min(fallback, key=abs)])
    for index in range(legs):
        goal = (rng.choice(pools[0]), rng.choice(pools[1])) if index % 2 == 0 else origin
        x, z = start
        gx, gz = goal
        corners = [(gx, z), (x, gz)]
        rng.shuffle(corners)
        remaining = [point for point in product(*axes) if point not in (start, goal, *corners)]
        rng.shuffle(remaining)
        zones = []
        for cx, cz in corners + remaining:
            if len(zones) == closed_count:
                break
            zone = (cx - 2, cz - 2, cx + 2, cz + 2)
            if not _clear(start, start, (zone,)) or not _clear(goal, goal, (zone,)):
                continue
            candidate = tuple(zones + [zone])
            if start in _distances(axes, goal, candidate):
                zones.append(zone)
        if len(zones) != closed_count:
            raise RuntimeError("The generated road graph cannot meet the frozen closure count")
        distance = _distances(axes, goal, tuple(zones))[start]
        output.append(Leg(goal, tuple(zones), distance // 2))
        start = goal
    return tuple(output)


class RouteChallenge:
    """Evaluate typed decisions against native movement and a fixed route schedule."""

    def __init__(self, scenario: str, level: str, seed: int) -> None:
        if scenario not in SCENARIOS or level not in LEVELS or type(seed) is not int or not 0 <= seed < 2**64:
            raise ValueError("Unknown scenario, level or invalid world seed")
        self.scenario = scenario
        self.level = level
        self.engine = Simulation(seed, Config(WorldProfile.COMPACT))
        try:
            nodes = self.engine.road_map().nodes
            self.axes = tuple(tuple(sorted({2 * round(getattr(node, coord) / 2) for node in nodes})) for coord in ("x", "z"))
            if len({(node.x, node.z) for node in nodes}) != len({node.x for node in nodes}) * len({node.z for node in nodes}):
                raise RuntimeError("This challenge protocol requires the native compact road grid")
            self.legs = _routes(self.axes, seed, level, scenario)
            self.reference = sum(leg.road_decisions for leg in self.legs)
            self.budget = 2 * self.reference + REVEAL * len(self.legs)
            if self.budget > MAX_DECISIONS:
                raise RuntimeError("The generated route exceeds the protocol decision limit")
        except BaseException:
            self.engine.close()
            raise
        self.completed = self.decisions = self.leg_decisions = self.checkpoints = 0
        self.closed = False

    def _closures(self):
        if self.scenario == "adaptation" and self.leg_decisions < REVEAL:
            return ()
        return self.legs[min(self.completed, len(self.legs) - 1)].closures

    def observation(self):
        position = self.engine.observe().position
        leg = self.legs[min(self.completed, len(self.legs) - 1)]
        return {"position": (position.x, position.z), "goal": leg.goal, "road_axes": self.axes,
                "closures": self._closures(), "stops_completed": self.completed,
                "stops_total": len(self.legs), "remaining_decisions": self.budget - self.decisions}

    def step(self, action: int):
        if self.closed or type(action) is not int or not 0 <= action < len(_ACTIONS):
            raise ValueError("Closed episode or invalid discrete action")
        observed = self.engine.step(_ACTIONS[action], DECISION_TICKS).observation
        self.decisions += 1
        self.leg_decisions += 1
        if self.scenario == "continuity" and self.leg_decisions == REVEAL:
            checkpoint = self.engine.snapshot()
            before = self.engine.state_hash()
            self.engine.close()
            self.engine.restore(checkpoint)
            if self.engine.state_hash() != before:
                raise RuntimeError("Native state changed after a continuity checkpoint")
            self.checkpoints += 1
        actual = (observed.position.x, observed.position.z)
        prohibited = not _clear(actual, actual, self._closures())
        leg = self.legs[self.completed]
        if actual == leg.goal and not prohibited:
            self.completed += 1
            self.leg_decisions = 0
        success = self.completed == len(self.legs) and observed.status is Status.RUNNING
        failure = "closure" if prohibited else "incapacitated" if observed.status is Status.INCAPACITATED else None
        if not success and not failure and self.decisions >= self.budget:
            failure = "budget"
        self.closed = success or failure is not None
        return {"action": action, "tick": observed.tick, "position": actual, "stops_completed": self.completed,
                "reward": 99 if success else -25 if prohibited else -1, "success": success, "failure": failure}

    def close(self) -> None:
        self.closed = True
        self.engine.close()
