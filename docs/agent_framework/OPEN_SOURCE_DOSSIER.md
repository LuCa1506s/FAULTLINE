# Open source adoption dossier

FAULTLINE provides CPU-only simulation for policies that need observable
consequences, persistent state and reproducible action traces. C++ applications
can embed the native SDK. Python users can run standard Gymnasium episodes,
compare policies and verify them in independent processes. The three initial
scenarios cover spatial decisions, an engine restart and a changing route rule.

## Use cases

- Regression-test planning or reinforcement learning policies over controlled seed sets.
- Reproduce agent failures by replaying a sequence of discrete decisions from a checkpoint.
- Compare local text policy backends with offline reference policies.
- Extend an engine-backed task from public observations while retaining the game's persistence contract.

## Related frameworks

| Framework | Focus | How it relates to FAULTLINE |
| --- | --- | --- |
| Gymnasium | Standard environment and space API for reinforcement learning | FAULTLINE implements this API for its scenario layer. [Gymnasium Env API](https://gymnasium.farama.org/api/env/) |
| MiniGrid | Configurable discrete grid worlds for goal-directed reinforcement learning | Both support compact policies. FAULTLINE actions drive continuous position through generated city geometry and persistent native state. [MiniGrid project](https://github.com/Farama-Foundation/Minigrid) |
| Habitat Lab | Embodied tasks, configurable cameras and sensors, training and benchmarking | Habitat provides embodied sensing and interaction tasks. FAULTLINE's initial focus is CPU-only stateful rollouts and canonical checkpoint replay. [Habitat Lab documentation](https://aihabitat.org/docs/habitat-lab/quickstart) |

## Evidence for maintainer programs

The release dossier separates tests on the publication candidates from the
earlier scientific study. The [release qualification](RELEASE_QUALIFICATION.md)
records installed-wheel tests, independent consumer results and a real local
model experiment with independently replayable actions. The x64 matrix now
includes auditwheel-verified Linux wheels for CPython 3.12 and 3.14. In the
schema-constrained local run, SmolLM2 generated 120 legal actions and exhausted
its budget without reaching the goal. The
[scientific results](SCIENTIFIC_RESULTS.md) describe the earlier 4,800-episode
study. Read the [contribution tasks](GOOD_FIRST_ISSUES.md) for scoped ways to
strengthen the evidence and developer workflow.

## Scientific scope

The route tasks distinguish policies based on their handling of observed road
constraints. The tested graph planner reaches the ceiling and random reaches
the floor. Identical Navigation and Continuity outcomes establish engine
checkpoint consistency for the tested policies. The graph planner results say
how that algorithm performs on these tasks. The local LLM study uses one
development seed to exercise generation, validation and offline replay. Model
comparison would require a preregistered sampling plan, multiple seeds and
matched token and decision budgets. See the [audit](RC_AUDIT.md) and
[model scope](MODEL_INTEGRATION.md).

## Measured maintenance work and next milestones

- A full game rerun took 373.81 s on Windows and 525.10 s in WSL2 during the
  audit. The 48-case scientific run took 248.14 s with four workers. Exact
  interpreter installs, wheel metadata and replay validation add release work.
- A funded maintenance cycle would first reproduce the four x64 installs from
  a tagged source, then verify the consumer transcripts and refresh the SBOM.
  The next research cycle would compare structured-action configurations on
  a frozen set of public development seeds with matching budgets.
- Track reproducible issues with the action trace, first divergent tick and
  runtime fingerprint. Use the number of verified packaging targets, replayable
  failure reports and tested policy adapters as deliverable measures.

See the [roadmap](../../ROADMAP.md) and [performance report](PERFORMANCE.md).

OpenAI offers Codex for Open Source benefits for maintainers, including API
credits for eligible workflows through its fund. Anthropic's current program
page describes temporary Claude Ultra access for qualifying contributors.
Use the official terms when considering a future application. [Codex for Open Source](https://developers.openai.com/community/codex-for-oss), [Codex Open Source Fund](https://openai.com/form/codex-open-source-fund/), [Claude for Open Source](https://claude.com/contact-sales/claude-for-oss)

## Proposed use of eligible credits

| Share of awarded API credits | Workflow | Measurable deliverable |
| --- | --- | --- |
| 50% | Compare proposed regression tests and reviews on small synthetic failure cases | A reviewed test backlog with reproducible inputs and accepted test assertions |
| 30% | Evaluate provider-independent adapters with bounded public observations when terms permit | A fixed-seed report with accepted actions, invalid outputs, timeout counts and offline replay |
| 20% | Summarize release triage and compare documentation instructions on sanitized test outcomes | Versioned release support notes and a shorter reproduction path for contributors |

Start with a cap of 1,000 remote decisions and 20 episodes across scenarios and policies.
Measure input/output tokens, timeouts and invalid actions before extending the
pilot. Keep native builds, reference policies and trace replay offline. Remote
inputs would consist of controlled synthetic values and public documentation.
Keep source, snapshots and local experiment files with the project. Maintainer
subscription benefits can support reviewed local test design and documentation
work under the same data boundary.

For a 30-day pilot, the maintainer would review the test backlog in week one,
reproduce the distribution matrix in week two, run the approved evaluation
budget in week three and publish the validated artifacts and limitations in
week four. Record the number of resolved reproduction issues, accepted
regression fixtures and verified release targets alongside elapsed maintainer
time. This schedule is a proposal for a future funded cycle.

Program-specific application drafts are kept in the maintainer workspace.
