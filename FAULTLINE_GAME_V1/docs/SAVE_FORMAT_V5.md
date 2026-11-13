# Save format v5

SaveFormatVersion is 5; GameVersion remains 0.1.0 and GeneratorVersion remains 3.
Readers accept save versions 1..5 and generator versions 1..3. All header and
section framing, primitive encodings, bounds and typed diagnostics are unchanged.
There are still 13 sections. SAVE_FORMAT_V4.md documents the historical prefix.

Sections 1..12 and the complete v4 section-13 payload retain their encodings.
Section 13 appends, after the consumed-operation ledger:

1. u32 resistanceCasualtyUnit (default 5), then u32 resistanceReliefUnit (default 2).
2. One u32 localResistance per territory, in the same ascending TerritoryId order
   as the section's territory records. No extra IDs or count are encoded.

Both rule units allow 0..1000 (zero disables that contribution). Resistance must
be 0..maximumPresence. Rules, resistance and the existing semantic data all enter
runtime StateHash schema `FAULTLINE.state.v7`. Derived control caches stay excluded.
The existing schema_v4.hpp describes the unchanged v4 prefix; save.cpp explicitly
writes/reads the v5 extension after that prefix.

V4 migration initializes resistance to zero and both new rules to their defaults.
It preserves entrenchment, incumbent, garrison affiliation/members, consumption
ledger, statistics, operation records, schedules, RNG and old control exactly.
Past casualties are not replayed; only future unconsumed results affect resistance.
V1/v2/v3 retain their previous migrations and also start with zero resistance.
No migration consumes RNG or regenerates the saved base. BaseWorldHash fixtures
and GeneratorVersion remain unchanged. V4 readers reject new v5 saves as usual.

The fixed save_v4_phase11.flsave fixture was generated with the pre-repair v4
executable. Tests compare sections 1..12 byte for byte and the complete old
section-13 payload prefix, then continue pending operations across another save.
Behavioral tests also round-trip nonzero resistance and non-default resistance
rules before further actual paid attack/defense outcomes.
