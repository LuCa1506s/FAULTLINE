# Decision log

| Phase | Decision | Evidence and consequences |
| --- | --- | --- |
| 00 | Qualify the existing working tree on a fresh build path. | Git began on `main` at `c4851564f8df893d1fa27d8a713916353cfc21da`, one commit ahead of `origin/main`, with a pre-existing engine CMake edit removing a redundant source registration and two untracked probe sources. |
| 00 | Treat physical simulation and coarse progression as distinct engine entry points. | `Simulation::advanceHeadless` requires an empty physical entity registry. A player-controlled SDK session uses `Simulation::tick` even for idle actions. CPU-only execution is independent of this choice. |
| 00 | Reuse existing snapshot encoding and state fingerprint. | The engine already preserves streaming leases and named RNG states. Scenario and policy metadata can travel alongside an engine save. |
| 00 | Keep agent observations separate from trusted host capabilities. | Passing a value observation to the policy avoids giving normal agent callbacks access to persistence, hashes, mutable engine memory or hidden evaluator configuration. |
| 01 | Add the new SDK from a root CMake project. | The original game CMake project remains a direct entry point. The SDK's `faultline::sdk` target can be consumed from a separate CMake project. |
| 01 | Spawn at the center of buildable land and support a compact profile. | The existing road generator places a junction there. The compact profile changes only engine generation configuration and uses the same movement pipeline. |
| 01 | Cap tick batches, session ticks, snapshot size and event output. | Trusted host input has explicit limits and rejected actions or snapshots preserve the current session. |
| 02 | Use distribution `faultline-sim` and import `faultline_sim`. | PyPI returned 404 for `faultline-sim` during the name check. The existing [faultline package](https://pypi.org/project/faultline/) uses the proposed `faultline` import and executable. Use a distinct import and future CLI to allow co-installation. |
| 03 | Derive the public compact road observation from the engine road graph. | A pair of bounded axes represents the rectangular world grid. The scenario owns its goal and reward. The engine owns collision, movement and tick advancement. |
| 04 | Record canonical starting bytes and the engine hash at every decision. | Replay checks the starting snapshot against a fresh seeded engine. It can localize divergence without an agent, model or external service. |
| 04 | Checkpoint the engine after the fourth decision of FL-Continuity. | The minimum starting goal distance prevents success before the interruption. Goal and decision count stay with the scenario host. Policy state is a separate capability. |
| 05 | Make the route condition depend on authoritative progress at decision four. | The first FL-Adaptation prototype always closed the X route, so a fixed Z-first policy could bypass it. Version 2 closes the axis with the most progress toward the goal. Both fixed-axis policies are now regression cases, and v2 qualification uses a fresh evaluation range. |
