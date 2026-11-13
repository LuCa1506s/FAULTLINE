# Save format v2

Current SaveFormatVersion is **2**. GameVersion remains **0.1.0** (the existing
pre-release project version); no policy couples it to save-format increments.
GeneratorVersion remains **3**, with generators 1 and 2 still supported.
The original [v1 format document](SAVE_FORMAT_V1.md) remains historical reference.

## Wire format

Magic, header offsets, little-endian primitives, UTF-8, length limits and error
policy are unchanged from v1. Header section count is now 11. Sections 1..10
retain their exact v1 field lists, including generated baselines and the original
faction fields. New faction strategic fields are runtime-only and do not change
BaseWorldHash. Production always writes v2; readers explicitly dispatch v1/v2.

New tag **11**, `FactionSimulation`, follows Relationships. Its payload is:

1. Config: five u64 tick intervals (evaluation, preparation, active duration,
   defense preparation, attack cooldown); three u32 bounds (minimum force,
   maximum force, concurrent operations); four i64 values (attack base cost,
   defense base cost, per-member cost, minimum attack utility).
2. u64 next strategic evaluation tick.
3. Operation store: u64 next OperationId, u32 record count, ascending OperationId
   plus records. Keys are u64 typed IDs.
4. u32 faction-strategy count, then ascending FactionId plus strategic records,
   covering exactly the current runtime faction store.

An operation record (92 bytes excluding its 8-byte key) contains:

* FactionId, opponent FactionId;
* u8 type: AttackTerritory=0, DefendTerritory=1;
* u8 status: Preparing=0, Active=1, Resolved=2, Failed=3, Cancelled=4;
* TerritoryId target, OperationId threat (defense-only; zero for attack);
* u32 committed manpower, i64 mobilization cost already paid;
* four u64 times: created, activation, resolution, finished tick;
* u32 own losses, opposing losses, garrison losses; u16 strategic pressure.

A faction-strategy record is 42 bytes excluding its 8-byte FactionId: u16
confidence; u64 evaluation count, last evaluation tick, next attack tick;
TerritoryId last chosen target; i64 last utility. All times are integer ticks;
the exact clock conversion to GameTime remains persisted in Runtime.

Pending operation-ID sets, deadline indexes and territory adjacency are derived,
excluded from bytes/hash and rebuilt after decoding. Existing geometry/runtime
validation plus operation/commitment/scheduler/reference validation must pass.
Supported topology bounds are checked before building the adjacency cache.
Validation failures retain version, section/offset and strategic ID context.

## Real v1 migration

After decoding the original ten sections, migration initializes:

* an empty operation store, next OperationId=1 and zero derived commitments;
* default runtime config and confidence=5000;
* evaluation count=0, last evaluation/target/utility=0 and attack cooldown=0;
* next evaluation = saved tick + 6400, or `neverTick` if it cannot fit safely.

No world regeneration, RNG consumption, ID reallocation or resource grant occurs.
Existing clock, all seven RNG states, all old allocator marks, semantic stores,
names, geometry and references are preserved. Saving the migrated state writes
v2; byte identity across migration is not expected, but unchanged generated
identity/resources/RNG are required. V2 save/load/save remains byte-identical.

Tests cover generated v1/v2/v3 worlds encoded using the frozen Phase 8 test
encoder, plus an independent fixed historical-wire binary fixture:
`tests/fixtures/save_v1_reference.flsave`. It is constructed without the current
C++ encoder by the explicit little-endian fixture script next to it; normal tests
only read it. SHA-256:
`db23ea06ef1008bbe82674d1f3a6e37abea991d2d60087c596488d6a364d4a3c`.
The test also verifies that the frozen v1 encoder reproduces this independent
fixture byte for byte. A temporary old-engine fixture executable was blocked by
Application Control; no policy bypass or security setting change was used.

## Hash and continuation

Runtime StateHash schema is now `FAULTLINE.state.v4`, extending the Phase 8
semantic snapshot with every faction strategic field, config, operation field,
operation allocator and next evaluation tick. Resource totals/capacity are
derived from those authorities rather than duplicated. Generator base hashes
and the generator version are unchanged.

Save during active linked attack/defense, restore, then continue the same
scheduled simulation: final hash, canonical bytes, future RNG and next OperationId
must match uninterrupted execution. Event buffers are transient and are emitted
again only for future transitions, not replayed as old side effects on load.

The v1 hardening limits still apply. No checksum, compression or atomic replace
was added; no migration beyond the real v1-to-v2 path is claimed.
