"""Measure a matching Python SDK workload and process-level episode scaling."""

from argparse import ArgumentParser
from concurrent.futures import ProcessPoolExecutor
import json
import resource
from time import perf_counter

from faultline_sim import Action, Config, Simulation, WorldProfile
from faultline_sim.qualification import RouteChallenge, graph_action


def measure():
    config = Config(WorldProfile.COMPACT)
    initialization = 0.0
    for sample in range(68):
        began = perf_counter()
        sim = Simulation(42, config)
        if sample >= 4:
            initialization += perf_counter() - began
        sim.close()
    actions = (Action.EAST, Action.WEST, Action.SOUTH, Action.NORTH)
    with Simulation(42, config) as sim:
        began = perf_counter()
        for index in range(4096):
            sim.step(actions[index % 4], 8)
        step_seconds = perf_counter() - began
        native = sim.observe()
        hash_value = sim.state_hash()
        snapshot = sim.snapshot()
        began = perf_counter()
        for _ in range(128):
            sim.snapshot()
        save_seconds = perf_counter() - began
        began = perf_counter()
        for _ in range(128):
            sim.restore(snapshot)
        restore_seconds = perf_counter() - began
        assert sim.state_hash() == hash_value
        native_payload = json.dumps({"tick": native.tick, "position": [native.position.x, native.position.z],
                                     "health": native.health, "status": native.status.name}, separators=(",", ":"))
    example = RouteChallenge("adaptation", "HARD", 42)
    try:
        for _ in range(4):
            example.step(0)
        agent_payload = json.dumps(example.observation(), separators=(",", ":"))
    finally:
        example.close()
    return {"init_seconds": initialization / 64, "step_seconds": step_seconds / 4096,
            "ticks_per_second": native.tick / step_seconds, "ticks": native.tick, "decisions": 4096,
            "snapshot_bytes": len(snapshot), "native_observation_json_bytes": len(native_payload.encode()),
            "agent_observation_json_bytes": len(agent_payload.encode()), "snapshot_seconds": save_seconds / 128,
            "restore_seconds": restore_seconds / 128, "state_hash": f"{hash_value:x}",
            "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}


def episode(seed):
    env = RouteChallenge("adaptation", "HARD", seed)
    try:
        while not env.closed:
            result = env.step(graph_action(env.observation()))
        return (result["success"], result["tick"], f"{env.engine.state_hash():016x}")
    finally:
        env.close()


def scale(count):
    stats = []
    expected = None
    for workers in (1, 2, 4):
        start = perf_counter()
        with ProcessPoolExecutor(workers) as pool:
            results = list(pool.map(episode, range(500, 500 + count), chunksize=4))
        seconds = perf_counter() - start
        if expected is None:
            expected = results
        elif results != expected:
            raise RuntimeError("Process count changed episode outcome, tick or hash")
        stats.append({"workers": workers, "episodes": count, "seconds": seconds,
                      "episodes_per_second": count / seconds, "successes": sum(row[0] for row in results)})
    return stats


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--episodes", type=int)
    args = parser.parse_args()
    if args.episodes is not None and not 1 <= args.episodes <= 1000:
        parser.error("--episodes must be between 1 and 1000")
    print(json.dumps(measure() if args.episodes is None else scale(args.episodes), indent=2))


if __name__ == "__main__":
    main()
