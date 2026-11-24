"""FL-Continuity, a navigation episode with a mandatory engine restart."""

from .navigation import NavigationEnv

CHECKPOINT_DECISION = 4


class ContinuityEnv(NavigationEnv):
    """Use a canonical checkpoint to restart the engine during the episode."""

    def __init__(self, max_decisions: int = 256) -> None:
        super().__init__(max_decisions)
        if max_decisions <= CHECKPOINT_DECISION:
            raise ValueError("FL-Continuity requires enough decisions to resume after its checkpoint")
        self.checkpoint_count = 0

    def reset(self, *, seed=None, options=None):
        result = super().reset(seed=seed, options=options)
        self.checkpoint_count = 0
        return result

    def step(self, action):
        result = super().step(action)
        if self._decisions == CHECKPOINT_DECISION:
            assert self._sim is not None
            before = self._sim.state_hash()
            checkpoint = self._sim.snapshot()
            self._sim.close()
            self._sim.restore(checkpoint)
            if self._sim.state_hash() != before:
                raise RuntimeError("Native checkpoint changed the completed state")
            self.checkpoint_count += 1
        return result
