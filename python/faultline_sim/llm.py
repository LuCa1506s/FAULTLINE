"""Provider-independent text policy and an offline deterministic backend."""

from collections.abc import Callable
import json

from .navigation import NavigationObservation


class TextAgent:
    """Pass a bounded public observation to a caller-supplied text backend."""

    def __init__(self, backend: Callable[[str], str]) -> None:
        self.backend = backend

    def __call__(self, observation: NavigationObservation) -> int:
        request = {"instruction": 'Return a JSON object with one integer field "action".',
                   "actions": ["idle", "east", "west", "south", "north"],
                   "observation": {name: value.tolist() for name, value in observation.items()}}
        response = self.backend(json.dumps(request, separators=(",", ":")))
        if type(response) is not str or len(response) > 128:
            return -1
        try:
            fields = json.loads(response, object_pairs_hook=list)
        except (ValueError, RecursionError):
            return -1
        if (type(fields) is not list or len(fields) != 1 or type(fields[0]) is not tuple or
                fields[0][0] != "action" or type(fields[0][1]) is not int or not 0 <= fields[0][1] <= 4):
            return -1
        return fields[0][1]


def local_backend(request: str) -> str:
    """Deterministic reference backend using the same public text request."""
    obs = json.loads(request)["observation"]
    dx, dz = [goal - position for goal, position in zip(obs["goal"], obs["position"])]
    action = 0
    if abs(dx) > 1:
        action = 1 if dx > 0 else 2
    elif abs(dz) > 1:
        action = 3 if dz > 0 else 4
    return json.dumps({"action": action})
