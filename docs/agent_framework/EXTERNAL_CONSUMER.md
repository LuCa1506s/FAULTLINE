# Independent developer qualification

For each host, copy the [consumer project](../../examples/consumer_project/README.md),
qualification constraints and the CPython 3.12 wheel into a new directory
after leaving the checkout. Create a clean venv and install the local wheel
with its `gym` extra. Run the consumer with `python -I review.py output`.
The exact Windows test directory name contained spaces.

Both Windows and Linux qualified the six consumer tasks. The installed CLI
completed three MEDIUM Adaptation benchmark episodes and exported JSON and CSV.
The custom policy finished the EASY Navigation route and reproduced
`2da37a8a3bd0e0fd` in a separate process. The idle policy reproduced failure
`budget` at tick 960, zero stops completed and StateHash `fc16f80b433ed0cb`.
The final position was `(0, 0)` and the decision budget was exhausted.
The results demonstrate a local recreation of an external workflow.

Evidence is under `build-scientific-consumer/{windows,linux}/output/` and is
copied into the review kit.
