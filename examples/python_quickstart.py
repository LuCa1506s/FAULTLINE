"""Run an episode fragment and verify its checkpoint using the installed SDK."""

from pathlib import Path
from tempfile import TemporaryDirectory

from faultline_sim import Action, Config, Simulation, WorldProfile


def main() -> None:
    config = Config(WorldProfile.COMPACT)
    with TemporaryDirectory() as directory, Simulation(42, config) as sim:
        start = sim.observe().position
        arrival = sim.step(Action.EAST, ticks=16).observation.position
        sim.advance(ticks=100)
        checkpoint = Path(directory) / "experiment.flsave"
        sim.save(checkpoint)
        with Simulation(0, config) as restored:
            restored.load(checkpoint)
            assert restored.state_hash() == sim.state_hash()
        print(f"Moved from ({start.x:g}, {start.z:g}) to ({arrival.x:g}, {arrival.z:g})")
        print(f"Tick {sim.observe().tick}, StateHash {sim.state_hash():016x}")


if __name__ == "__main__":
    main()
