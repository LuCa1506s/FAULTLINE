"""Run a custom observation-only policy using the installed Gymnasium extra."""

from faultline_sim.navigation import NavigationEnv, NavigationObservation


def decide(observation: NavigationObservation) -> int:
    """Follow the current road along X, then turn toward the target on Z."""
    dx, dz = observation["goal"] - observation["position"]
    if abs(dx) > 1:
        return 1 if dx > 0 else 2
    if abs(dz) > 1:
        return 3 if dz > 0 else 4
    return 0


def main() -> None:
    env = NavigationEnv()
    try:
        observation, _ = env.reset(seed=42)
        total_reward = 0.0
        for decision in range(1, env.max_decisions + 1):
            observation, reward, terminated, truncated, info = env.step(decide(observation))
            total_reward += reward
            if terminated or truncated:
                print(f"Success {info['success']}, decisions {decision}, ticks {info['tick']}, reward {total_reward:g}")
                return
    finally:
        env.close()


if __name__ == "__main__":
    main()
