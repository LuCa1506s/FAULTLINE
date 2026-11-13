# Save format v13

Phase 20 changes the persistent schema because pending reports, police knowledge,
Wanted deadlines/last-known information, and semantic response state affect future
simulation. `SaveFormatVersion` therefore advances from 12 to 13 and StateHash from
`FAULTLINE.state.v14` to `FAULTLINE.state.v15`. GameVersion remains 0.1.0 and
GeneratorVersion remains 3.

V13 appends section 21, Police. It serializes PoliceStateVersion 1 rules and ID
allocators; CrimeEvents; observations; pending/delivered/interrupted reports;
knowledge; processed Combat source facts; Wanted context; semantic responders;
district pressure; and pending arrest. Diagnostic counters are excluded. Wire fields are
declared in `src/persistence/schema_v13.hpp` and remain explicit little-endian data.
No renderer, Map2D, spatial-cache, candidate-array, transient event, or wall-clock
state is serialized.

V1-v12 migration initializes neutral PoliceState v1: no crimes, reports, knowledge,
Wanted state, or responders, with zero district pressure and deterministic allocators.
It consumes no gameplay RNG, does not alter GeneratorVersion, and preserves every
historical v12 section payload. The authentic Phase 19 v12 fixture and its provenance
are recorded in `tests/fixtures/README.md`.

The reader rejects unsupported PoliceState versions, invalid enum/identity/reference
data, stale pending deadlines, unordered or duplicate records, invalid positions,
dirty `CLEAR` state, duplicate responders, and invalid pressure/rules.

