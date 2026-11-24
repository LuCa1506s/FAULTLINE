"""Exercise a generic text callback and create a trace with an installed wheel."""

from argparse import ArgumentParser
from pathlib import Path

from faultline_sim.llm import TextAgent, local_backend
from faultline_sim.replay import record_episode, write_trace


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    policy = TextAgent(local_backend)
    trace = record_episode("continuity", 42, policy)
    write_trace(args.trace, trace)
    terminal = trace["steps"][-1]
    print(f"Success {terminal['success']}, tick {terminal['tick']}, hash {terminal['state_hash']}")


if __name__ == "__main__":
    main()
