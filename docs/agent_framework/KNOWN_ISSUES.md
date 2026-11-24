# Known issues and limits

- Engine determinism is scoped to the same build, platform, runtime conditions,
  initial state and ordered inputs. Floating-point equivalence across platforms
  has no general guarantee in the inherited contract.
- Coarse headless time advancement requires an empty physical entity registry.
  Player-controlled headless experiments use normal fixed ticks.
- The Phase 00 starting tree contains an existing CMake cleanup and two untracked
  probes. They belong to the pre-existing working state.
- Native sessions are thread-confined. A policy callback sees detached values,
  while code running in the evaluator's process belongs to the host trust boundary.
- The 0.1.0b2 publication candidates on Linux target `manylinux_2_39_x86_64`.
  Lower-glibc systems require a compatible source build or a separately
  qualified wheel. See [the matrix](DISTRIBUTION.md).
- Native sanitizer coverage consists of the standalone consumer integration run.
  Broader engine coverage and fuzzing arbitrary game saves remain future work.
