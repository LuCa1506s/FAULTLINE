# FAULTLINE — Deterministic Agent Simulation Framework

Build, evaluate and reproduce intelligent agents in persistent simulated worlds.

| Phase | Deliverable | Required evidence before advancing |
| --- | --- | --- |
| 00 | Engine baseline and SDK contract | Clean Release build, full inherited CTest suite, 365-day checkpoint comparison, architecture map |
| 01 | C++ SDK | Independent consumer, isolated instances, validated input, persistence and continuation, inherited regression suite |
| 02 | Python SDK | Wheels installed and tested outside the checkout on Windows and Linux, with platform gaps recorded |
| 03 | Gymnasium and FL-Navigation | Environment checker, random and heuristic policies, reachable goals, authoritative movement and bounded episodes |
| 04 | Replay and FL-Continuity | Independent-process replay, checkpoint equivalence, rejection of corrupt or incompatible traces |
| 05 | Benchmark suite and FL-Adaptation | Three measured scenarios and three baselines, isolated evaluation inputs, separate seed sets, reproducible reports |
| 06 | Local beta release candidate | Clean install, external examples, CLI, documentation, regressions and checksummed artifacts |
| 07 | Adoption materials | Tutorial, examples, contribution opportunities, comparison, demo and program dossier |

See [phase status](PHASE_STATUS.md), [measured evidence](TEST_EVIDENCE.md),
[known issues](KNOWN_ISSUES.md) and [resumption instructions](NEXT_ACTIONS.md).
