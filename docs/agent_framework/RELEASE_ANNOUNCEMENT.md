# Release announcement draft for 0.1.0b2

FAULTLINE helps developers build, evaluate and reproduce agents in persistent
simulated worlds. This candidate adds four route challenge levels, a bounded
model adapter and a source distribution. Every policy decision uses the same
authoritative C++ controller and can be checked from a versioned trace.

The local qualification ran 4,800 held-out episodes with 100 seeds per
scenario, difficulty and policy. The graph planner and one-exclusion planner
showed different results on HARD and STRESS. The standalone consumer creates
an agent, exports results and replays a failed episode from its wheel install.

The publication kit includes Windows x64 and `manylinux_2_39_x86_64` wheels
for CPython 3.12 and 3.14. A CPU-only SmolLM2 run generated 120 valid actions
with a response schema, exhausted its route budget and replayed offline to
the same terminal hash. See the [distribution matrix](DISTRIBUTION.md),
[model methodology](MODEL_INTEGRATION.md) and [consumer tutorial](EXTERNAL_CONSUMER.md).
This announcement is a maintainer draft.
