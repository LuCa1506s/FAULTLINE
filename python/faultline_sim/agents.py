"""Small CPU policies that operate on public navigation observations."""

from random import Random
from itertools import pairwise

from .adaptation import AdaptationObservation
from .navigation import GOAL_TOLERANCE, NavigationObservation


def random_action(observation: NavigationObservation, rng: Random) -> int:
    """Sample one of the five discrete actions using a caller-owned RNG."""
    return rng.randrange(5)


def heuristic_action(observation: NavigationObservation) -> int:
    """Follow the spawn row toward the goal column, then follow that column."""
    dx, dz = observation["goal"] - observation["position"]
    if abs(dx) > GOAL_TOLERANCE:
        return 1 if dx > 0 else 2
    if abs(dz) > GOAL_TOLERANCE:
        return 3 if dz > 0 else 4
    return 0


def _intersects_route(route, exclusion) -> bool:
    lo_x, lo_z, hi_x, hi_z = exclusion
    # Allow for the one-meter tolerance of the quantized player commands.
    return any(max(a[0], b[0]) >= lo_x - 1 and min(a[0], b[0]) <= hi_x + 1 and
               max(a[1], b[1]) >= lo_z - 1 and min(a[1], b[1]) <= hi_z + 1
               for a, b in pairwise(route))


def planning_action(observation: NavigationObservation | AdaptationObservation) -> int:
    """Choose the shortest feasible route via a publicly observed road axis."""
    exclusion = observation.get("exclusion")
    if exclusion is None or not exclusion.any():
        return heuristic_action(observation)
    x, z = map(float, observation["position"])
    gx, gz = map(float, observation["goal"])
    xs, zs = [axis[:count] for axis, count in zip(observation["road_axes"], observation["axis_counts"])]
    routes = []
    if any(abs(z - axis) <= GOAL_TOLERANCE for axis in zs):
        routes.extend([(x, z), (float(axis), z), (float(axis), gz), (gx, gz)] for axis in xs)
    if any(abs(x - axis) <= GOAL_TOLERANCE for axis in xs):
        routes.extend([(x, z), (x, float(axis)), (gx, float(axis)), (gx, gz)] for axis in zs)
    legal = [route for route in routes if not _intersects_route(route, exclusion)]
    if not legal:
        return 0
    route = min(legal, key=lambda path: sum(abs(a[0] - b[0]) + abs(a[1] - b[1]) for a, b in pairwise(path)))
    for tx, tz in route[1:]:
        if abs(tx - x) > GOAL_TOLERANCE:
            return 1 if tx > x else 2
        if abs(tz - z) > GOAL_TOLERANCE:
            return 3 if tz > z else 4
    return 0
