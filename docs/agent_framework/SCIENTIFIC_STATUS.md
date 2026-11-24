# Scientific and distribution qualification

The 0.1.0b1 development ledger records the earlier local beta decision. The
follow-up below uses independent gates and preserves that kit as an audit input.

| Phase | Gate | State |
| --- | --- | --- |
| 08 | RC_AUDIT | RC_AUDIT_PASS. Historical benchmark validity limits recorded. |
| 09 | BENCHMARK_VALIDATION | BENCHMARK_VALIDATION_PASS. 4,800 held-out episodes and all replays verified. |
| 10 | LLM_INTEGRATION | LLM_INTEGRATION_PASS. 37 installed tests passed on Windows and Linux. |
| 10 | REAL_LLM_EVALUATION | REAL_LLM_EVALUATION_NOT_VERIFIED. Workspace model/backend inventory found no usable local instance. |
| 11 | DISTRIBUTION | DISTRIBUTION_PARTIAL. CPython 3.12 and 3.14 tested on Windows and Linux. GLIBC 2.43 prevented manylinux qualification. |
| 12 | PERFORMANCE | PERFORMANCE_QUALIFIED. Matching native and Python state, process scaling and bounded fuzz corpus verified. |
| 13 | EXTERNAL_CONSUMER | EXTERNAL_CONSUMER_PASS. Independent consumer, export, success and failure replays verified on Windows and Linux. |
| 14 | FUNDING_DOSSIER | FUNDING_DOSSIER_READY. Four program drafts, maintainer roadmap, release draft, isolated demo and verified plots ready locally. |

See the [audit](RC_AUDIT.md), [file inventory](SCIENTIFIC_FILES.md) and
[baseline development ledger](PHASE_STATUS.md). The initial b2 local maintainer
kit is `build-scientific-distribution/kit/`. The table preserves those verdicts
at the end of Phase 14. See the [release qualification ledger](RELEASE_QUALIFICATION.md)
for Phases 15–19 and the replacement publication kit.
