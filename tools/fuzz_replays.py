"""Run deterministic byte and structured mutations against the route trace reader."""

from copy import deepcopy
import json
from pathlib import Path
from random import Random
from tempfile import TemporaryDirectory

from faultline_sim.qualification import graph_action
from faultline_sim.scientific import MAX_TRACE_BYTES, read_replay, run_episode, verify_episode, write_replay


def main():
    rng = Random(40679)
    trace, _, _ = run_episode("continuity", "EASY", 42, graph_action)
    if not trace["steps"][-1]["success"]:
        raise RuntimeError("The replay corpus requires a successful reference episode")
    rejected = accepted = 0
    with TemporaryDirectory() as directory:
        path = Path(directory) / "candidate.json"
        write_replay(path, trace)
        reference = path.read_bytes()
        for test in range(641):
            if test < 128:
                candidate = rng.randbytes(rng.randrange(4096))
            elif test < 256:
                candidate = reference[:rng.randrange(len(reference))]
            elif test < 512:
                edited = bytearray(reference)
                edited[rng.randrange(len(edited))] ^= 1 << rng.randrange(8)
                candidate = bytes(edited)
            elif test < 640:
                structured = deepcopy(trace)
                step = rng.choice(structured["steps"])
                field = rng.choice(("tick", "reward", "state_hash", "action"))
                step[field] = "bad"
                write_replay(path, structured)
                candidate = path.read_bytes()
            else:
                candidate = b" " * (MAX_TRACE_BYTES + 1)
            path.write_bytes(candidate)
            try:
                actual = read_replay(path)
                verify_episode(actual)
            except (KeyError, TypeError, ValueError, RecursionError):
                rejected += 1
            else:
                if actual != trace:
                    raise RuntimeError("Changed semantic data passed replay verification")
                accepted += 1
    print(json.dumps({"cases": accepted + rejected, "rejected": rejected, "unchanged_semantics": accepted}))


if __name__ == "__main__":
    main()
