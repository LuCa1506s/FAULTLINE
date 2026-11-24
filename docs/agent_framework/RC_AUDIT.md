# Phase 08 audit of 0.1.0b1

The starting working tree is based on `c4851564f8df893d1fa27d8a713916353cfc21da`.
The frozen kit is `build-framework-release/0.1.0b1`. Audit outputs use
`build-scientific-audit/`. The original license files, artifacts and gameplay
implementation are audit inputs.

## Static evidence

`python tools/audit_rc.py build-framework-release/0.1.0b1` passed. It verified
the 71-file manifest and source fingerprint, compared the Python files and
five license documents inside each wheel with the checkout, matched the native
extension digests with the benchmark runtime records, and recomputed the
aggregates from the 1,800 matching JSON and CSV rows. Each case uses exactly
11,000–11,099. The compiler rebuilds and clean installed consumer runs are
tracked separately.

The execution chain is `NavigationEnv.step` → pybind11 → `sdk::Simulation::step`
→ the engine's `Simulation::tick`, with the same player creation and movement
path used by the native engine. Python owns target selection, scoring and
constraints on the accepted native positions. The audit driver independently
advances the native API and checks movement, ticks, hashes and reward rules.

## Gate decision

`RC_AUDIT_PASS` qualifies the frozen kit as a reproducible functional fixture.
The Windows audit matched all 900 kit rows for deterministic metrics, retained
27 complete new traces and replayed those plus both kit samples in additional
clean processes. The original Windows game suite passed 42/42 in 373.81 s.
The two original wheels each passed 16/16 installed tests. The clean Windows
source rebuild produced a different archive digest and passed 16/16 installed
tests. Binary identity across rebuilds is outside the current claim.

Linux independently matched the other 900 historical rows and replayed 27 new
traces and two original samples from additional processes. Together the two
platforms independently computed native positions, rewards, terminal flags
and hashes for 54 new traces. The Linux game suite passed 42/42 in 525.10 s.
The Linux checkout rebuild overlapped the start of Phase 09 and its Python
packaging picked up the new modules. It is excluded as strict b1 rebuild
evidence. A rebuild from the fingerprint-verified frozen source snapshot is
in `build-scientific-audit/linux/frozen-rebuild/`. That rebuild matched all
11 packaged Python and typing files and passed 16/16 clean installed tests
in 3.63 s. Its archive digest is
`62cd59bad06f4a17c8c9826fef65c4f77f63c3b0f35053ce12553b8e7e6e19ab`.

```sh
python tools/audit_rc.py build-framework-release/0.1.0b1 --repo build-scientific-audit/frozen-source
```

## Findings under verification

| Finding | Severity | Status |
| --- | --- | --- |
| The original kit retained per-episode rows and sample traces. The 1,800 replay flags come from the same-process replay loop, with independent-process verification demonstrated by samples. | Moderate evidence gap | Replay all original cases again in fresh installed processes and retain new full traces. |
| Navigation and Continuity planning call the heuristic directly. The earlier success rates describe two names for one decision rule in both scenarios. | High scientific limitation | Preserve the v1 behavior. Design and qualify a distinct planner for new variants. |
| Adaptation v2 breaks fixed-axis routes that move immediately. Ties after waiting at the spawn select the X exclusion, permitting a predetermined Z-first detour that ignores the displayed condition. | High scientific limitation | Confirmed on 10/10 seeds per platform, with replay. The historic results page now describes this limit. The existing scenario stays versioned. |
| Python callbacks execute inside the evaluator process. Ordinary observations are detached arrays, while process isolation requires a separate execution boundary. | High for untrusted integrations | Field allowlist and mutation tests passed for all three scenarios on both platforms. Qualify a restricted policy transport before accepting untrusted agents. |
| The Random baseline is initialized with the world seed. Seed partitions enforce disjoint public ranges and provide no private holdout mechanism. | Moderate experimental limitation | Keep historic reproduction stable and separate agent RNG from the world seed for new evaluations. |
