# Contributing to FAULTLINE

Thank you for your interest in FAULTLINE, an independent C++ game and world simulation project. Contributions to the engine, tests, technical documentation, and tooling are welcome.

FAULTLINE is under active development. The design specifications contain both implemented behavior and future goals; please check the current source and tests before relying on a specification as an implementation guarantee.

## Getting started

1. Read the root [README.md](README.md) and the relevant technical specification.
2. Check existing GitHub issues and pull requests before starting work. For substantial architectural changes, open an issue first to discuss scope and trade-offs.
3. Fork the repository, clone your fork, and create a focused branch from the default branch.
4. Build and test the project. The current game source is in `FAULTLINE_GAME_V1/`; use its build configuration and existing test instructions.

A conventional CMake workflow, run from the repository root, is shown below. This is a starting point, not a verified clean-install recipe; dependency setup and supported configurations must be confirmed against the current source.

```sh
cmake -S FAULTLINE_GAME_V1 -B FAULTLINE_GAME_V1/build -DCMAKE_BUILD_TYPE=Release
cmake --build FAULTLINE_GAME_V1/build --config Release --parallel
ctest --test-dir FAULTLINE_GAME_V1/build -C Release --output-on-failure
```

## Engineering guidelines

- Follow the existing C++ style and architecture. Keep changes small, purposeful, and easy to review.
- Preserve deterministic behavior where it is required. Use explicit seeds, stable ordering, and controlled inputs in tests. Avoid introducing nondeterministic state transitions.
- Treat persistent state, entity identity, serialization, schema evolution, and world-streaming boundaries as correctness-sensitive interfaces.
- Add regression tests for bugs and new behavior. Include relevant edge cases and failure paths, not only successful scenarios.
- For performance changes, provide the workload, environment, measurement method, and before/after results. Do not claim unmeasured improvements.
- Update relevant documentation when behavior changes. Distinguish planned behavior from implemented and tested behavior.
- Avoid committing generated builds, local configuration, credentials, or unnecessary binary files.

## Reporting bugs

Include the commit or version, operating system and compiler, steps to reproduce, expected and actual behavior, relevant logs, and a minimal reproduction if possible. For simulation failures, include the seed, tick count, configuration, and relevant state hashes when available. Remove private information from logs and attachments.

## Pull requests

Before submitting a PR:

- Describe the problem, proposed solution, and scope.
- Explain any architectural or persistence-format changes and compatibility implications.
- Report which tests were run, the results, and any limitations. Do not report tests as passed if they were not executed.
- Link relevant issues and specifications.
- Confirm you have the rights to submit your changes under the project's applicable license.

Reviews may request additional tests, documentation, or narrower changes before merging. A passing test suite does not automatically establish correctness for every simulation scenario.

## Licensing and assets

Original code, tests, and technical documentation are contributed under Apache-2.0, as described in [LICENSING.md](LICENSING.md). Do not submit third-party code, assets, music, textures, or generated material unless you have the necessary rights and can document the applicable terms. Preserve all required copyright and license notices. Proprietary creative assets are outside the default code license.

See [LICENSE](LICENSE) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for more information.
