"""Record a route episode with an explicitly selected local model server."""

import argparse
from dataclasses import asdict
import json
from pathlib import Path

from faultline_sim.model import Budget, HttpBackend, ModelAgent
from faultline_sim.scientific import run_episode, write_replay


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--provider", choices=("ollama", "llama.cpp", "openai"), required=True)
    parser.add_argument("--origin", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--model-version", required=True)
    parser.add_argument("--quantization", required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--inferences", type=Path, required=True)
    args = parser.parse_args()
    budget = Budget()
    agent = ModelAgent(HttpBackend(args.provider, args.origin, args.model), budget)
    trace, policy_seconds, duration = run_episode("navigation", "EASY", 42, agent)
    write_replay(args.trace, trace)
    report = {"provider": args.provider, "model": args.model, "model_version": args.model_version,
              "quantization": args.quantization, "budget": asdict(budget), "tokens_used": agent.tokens_used,
              "policy_seconds": policy_seconds, "episode_seconds": duration,
              "inferences": [asdict(event) for event in agent.logs], "terminal": trace["steps"][-1]}
    args.inferences.write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
