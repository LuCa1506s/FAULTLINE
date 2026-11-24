from pathlib import Path
import sys

import pytest

import faultline_sim
from faultline_sim import Action, Config, Simulation, SnapshotError, Status, WorldProfile


COMPACT = Config(WorldProfile.COMPACT)


def test_installed_extension_and_context_manager():
    assert Path(faultline_sim.__file__).resolve().is_relative_to(Path(sys.prefix).resolve())
    assert Simulation.__module__ == "faultline_sim._native"
    with Simulation(seed=42) as sim:
        assert sim.advance(ticks=100).observation.tick == 100
        assert isinstance(sim.state_hash(), int)
    with pytest.raises(RuntimeError, match="closed"):
        sim.observe()


def test_movement_isolation_and_checkpoint(tmp_path):
    with Simulation(42, COMPACT) as sim, Simulation(42, COMPACT) as continuous:
        start = sim.observe().position
        moved = sim.step(Action.EAST, ticks=16)
        assert moved.observation.position.x == start.x + 4
        assert moved.observation.position.z == start.z
        assert moved.observation.status is Status.RUNNING
        assert continuous.observe().tick == 0
        continuous.step(Action.EAST, ticks=16)
        assert sim.state_hash() == continuous.state_hash()

        path = tmp_path / "simulation-é.flsave"
        sim.save(path)
        assert path.read_bytes() == sim.snapshot()
        sim.close()
        sim.load(path)
        sim.advance(ticks=100)
        continuous.advance(ticks=100)
        assert sim.state_hash() == continuous.state_hash()
        assert sim.snapshot() == continuous.snapshot()


def test_invalid_inputs_leave_state_unchanged():
    with Simulation(2**64 - 1, COMPACT) as sim:
        initial = sim.state_hash()
        for seed in (-1, 2**64):
            with pytest.raises(TypeError):
                sim.reset(seed, COMPACT)
        with pytest.raises(ValueError):
            sim.advance(257)
        with pytest.raises(TypeError):
            sim.step("east")
        with pytest.raises(SnapshotError):
            sim.restore(sim.snapshot()[:-1])
        with pytest.raises(TypeError):
            sim.restore(bytearray())
        assert sim.state_hash() == initial


def test_observation_and_map_are_detached_values():
    with Simulation(42, COMPACT) as sim:
        original = sim.observe()
        assert any((p.x, p.z) == (original.position.x, original.position.z) for p in sim.road_map().nodes)
        with pytest.raises(AttributeError):
            original.position.x = 1000
        road_map = sim.road_map()
        road_map.nodes.clear()
        assert len(road_map.nodes) > 0
        sim.step(Action.WEST, 4)
        assert original.tick == 0
        assert sim.observe().position.x == original.position.x - 1
