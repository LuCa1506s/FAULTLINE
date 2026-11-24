# Good first issues

These are scoped contribution tasks for the current tree. Check the existing
issue list for related work before opening a ticket.

| Task | Starting point | Acceptance check |
| --- | --- | --- |
| Test an invalid text action in an episode report | `python/faultline_sim/benchmark.py` and `tests/python/test_benchmark.py` | Verify the invalid-action count and the single idle tick batch consumed by the fallback. |
| Add a no-extra CLI smoke check | `.github/workflows/framework.yml` and `python/faultline_sim/__main__.py` | A wheel installed without `gym` can run `simulate` and provides an installation hint for `benchmark`. |
| Exercise Unicode and spaces in trace paths | `tests/python/test_replay.py` | Write and replay a trace from an external subprocess using a path with spaces and non-ASCII characters on Windows and Linux. |
| Document one installed-wheel policy type-check | `python/faultline_sim/_native.pyi` and `examples/navigation_agent.py` | Provide a minimal verification command and verify the public policy signature with a tool used by the contributor. |
