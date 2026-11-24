# Phase status

Overall verdict is **BETA_RC_READY** for local review of 0.1.0b1.

| Phase | Verdict | Evidence |
| --- | --- | --- |
| 00 Baseline | PASS | Fresh Release builds and 42/42 inherited tests on Windows and Linux. Both platforms reproduced the 365-day checkpoint comparison. |
| 01 Native SDK | PASS | Windows integrated 43/43, independent Linux consumer and Linux ASan/UBSan checks passed. |
| 02 Python SDK | PASS | Windows and Linux native wheels installed in clean environments. Four tests and the outside-checkout quick start passed on both platforms. |
| 03 Gymnasium | PASS | Installed-wheel Gymnasium checker and eight tests passed on Windows and Linux. Random and heuristic smoke measured. |
| 04 Replay | PASS | Windows and Linux installed-wheel replay suites passed. Continuous/restored hashes, subprocess replay and corruption rejection verified. |
| 05 Benchmarks | PASS | Nine scenario–baseline pairs × 100 seeds on each platform. All 1,800 replays verified. |
| 06 Release candidate | PASS | Final Windows and Linux wheels passed 16 installed tests each. External examples, CLI replay, scoped dependency review and release manifest verified. |
| 07 Adoption readiness | PASS | Tutorial, independent examples, demo runbook, issue template, contribution tasks, comparison and maintainer dossier ready. Demo commands and text example verified. |

See [verified scope](TEST_EVIDENCE.md), [remaining limits](KNOWN_ISSUES.md)
and the [release kit](RELEASE.md). The [file inventory](FILE_CHANGES.md) lists
working-tree changes made for the framework.
