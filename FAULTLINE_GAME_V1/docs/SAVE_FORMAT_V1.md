# Save format v1

`SaveFormatVersion = 1` is independent of `GeneratorVersion = 1/2/3` and
`GameVersion = 0.1.0`. No canonical design documents are modified. The user scoped
checksum/atomic replacement to later hardening; neither is claimed here.

## Header and framing

All integers are fixed-width little-endian, with signed values encoded in two's
complement. No structure layout, padding, pointer or native container layout is
written. Floats are finite IEEE-754 binary32, with either signed zero written as
positive zero. Booleans are one byte, exactly 0 or 1. Typed IDs are u64 (0 is null).
Enums have their explicitly declared u8/u16 representation. Strings are u32 byte
length followed by strict UTF-8, without terminator. Vectors are u32 count then
elements. Fixed arrays have no count. Optional values start with a boolean.

| Offset | Size | Meaning |
|---|---|---|
| 0 | 16 | Magic: ASCII `FAULTLINE_SAVE`, then two zero bytes |
| 16 | 4 | SaveFormatVersion (1) |
| 20 | 12 | GameVersion: u32 major, minor, patch (0, 1, 0) |
| 32 | 4 | GeneratorVersion (1, 2 or 3) |
| 36 | 8 | WorldSeed |
| 44 | 4 | Number of sections (10) |
| 48 | variable | Sections below |

Each section begins with u32 tag and u32 payload byte length. Exactly the ten
sections below must appear once in this order. Unknown, missing, repeated,
out-of-order sections, excess payload and trailing file data are errors. Header
versions are checked before decoding the body. Game versions other than 0.1.0 are
explicitly rejected until a real compatibility/migration policy exists.

## Sections and field order

The complete nested wire schema is the explicit member list in
[`schema_v1.hpp`](../src/persistence/schema_v1.hpp). Member declaration order or
C++ ABI changes do not change that list. Changing the wire schema requires a new
save format version and an implemented migration or explicit rejection.

| Tag | Section | Payload |
|---|---|---|
| 1 | Runtime | u64 tick; u64 game milliseconds; f32 cell size; u32 RNG count (7); 7 x 4 u64 RNG words |
| 2 | Generated | optional complete GeneratedWorld, including metadata/configuration, geometry and actor baselines |
| 3 | Entities | u64 nextEntityId; u32 live count and ascending EntityIds; transform, health and vehicle-link component stores |
| 4 | Districts | DistrictStore |
| 5 | Territories | TerritoryStore, including current owner and resident manpower |
| 6 | Factions | FactionStore, including current leader, resources and personality |
| 7 | NPCs | NpcStore, including identity, lifecycle, affiliation, location, health, roles and traits |
| 8 | Vehicles | Existing minimal VehicleStore (active EntityId link) |
| 9 | Missions | Existing minimal MissionStore (status and issuer); no mission generator/runtime added |
| 10 | Relationships | faction relationship vector, then NPC relationship vector |

RNG stream order is Combat, Npc, Missions, Factions, Market, Police, Story.
Each stream stores xoshiro256** words s[0], s[1], s[2], s[3] in that order; load
uses `DeterministicRng::restore`, not reseeding. Changing stream order/algorithm
requires an explicit compatible save-format transition. Clock persistence checks
`gameMilliseconds == tick * 2250` and the existing maximum tick bound.

DomainStore encoding: u64 allocator next value; u32 count; ascending pairs of
typed ID and explicit record fields. ComponentStore encoding: u32 count;
ascending pairs of EntityId and component fields. ID duplicates, null keys,
nonascending keys and allocator next values that could reuse an existing ID are
rejected. The next value is a high-water mark, not `largest surviving ID + 1`.
Zero next means permanently exhausted; allocating then fails. Entity destruction
and NPC abstraction never rewind this mark.

Generated vectors retain generation order, already ascending by object ID; a
different order is rejected rather than changing the historical BaseWorldHash
contract. Runtime relationship sets are sorted by directed endpoints and fields
in a snapshot, so insertion order does not affect save bytes or StateHash.

## Authority and snapshot boundary

The API is synchronous and must be called on the single authoritative thread at a
completed-tick/structural-commit boundary. `Game::state()` exposes completed state;
Simulation currently has no external callbacks that can save during a tick.
Neither concurrent mutation nor serialization from another thread is supported.

Before writing, validate the original state, copy it, synchronize active NPC
position/health from their authoritative components, clear their active links and
discard their physical bodies in the copy. Preserve identity and all allocator
high-water marks. The running state is untouched. NPCs load abstract and can
materialize again, with new EntityIds and the same semantic identity/condition.
Historical dead records remain available to mission/relationship references.

Other existing data-only entities/components are retained as explicit IDs and
values. The minimal vehicle store has no separate semantic transform yet; its
linked transform/health remain the authority and are preserved. This does not
introduce vehicle gameplay or serialize engine handles. There is no ephemeral
crowd or aggregate-member materialization system yet; its future reconciliation
policy is outside this schema's current modeled data.

StateHash schema `FAULTLINE.state.v3` hashes this same semantic snapshot, including
clock, every runtime RNG word, all allocator marks and existing domain/component
data. Physical NPC representation and relationship insertion order cannot change
the comparison. BaseWorldHash and generator algorithms/versions are unchanged.
Creating/destroying an Entity still changes StateHash through the allocator mark.

## Persistence classification

* GENERATED: complete immutable base world and generated identities. Included in
  full saves initially; no regeneration or delta optimization during load.
* PERSISTENT_RUNTIME: metadata, clock, seven RNG states, allocator marks, domain
  records and relationships; surviving data-only Entity IDs/components.
* TRANSIENT: NPC physical representation; pending application input/commands,
  event/structural buffers, Simulation services and FixedStepRunner accumulator.
  Queued external intent is not part of GameState and must be supplied again on
  resume. No player/market/story/police stores exist to serialize yet.
* DERIVED: SpatialGrid membership, lookup/navigation/render caches. Cell size is
  coordinate configuration, so it is persisted; memberships are rebuilt from
  authoritative surviving transforms after semantic validation.

## Error and resource policy

Public memory/file APIs throw typed `SaveError`, with category, section, byte
offset, save/generator versions and message. Categories distinguish invalid
magic, unsupported save/generator/game version, truncation, invalid encoding,
semantic validation failure and I/O failure. A failed load returns no partial
state, never repairs references and never generates missing actors.

Limits apply on both encoding and decoding: 64 MiB file/payload total, 65,536
bytes per string, 100,000 elements per collection, 300,000 aggregate collection
elements. These initial save resource limits can reject large custom worlds that
worldgen itself permits; they cover the standard V1 preset. Readers verify
remaining section bytes before reading and decode elements incrementally rather
than allocating a claimed count up front. Size additions are bounded by remaining
input before forming an end offset. Invalid UTF-8, nonfinite floats, clock
overflow, inconsistent tick/game time, zero PRNG state and broken references fail.
All existing geometry/actor/runtime validators run after decoding. Validated data
is used to rebuild caches and initialize a fresh Simulation via `Game(restored)`.

No checksum, compression, delta encoding, migrations, autosave or atomic replace
is implemented. Semantically valid bit changes are not detected as corruption;
that is an explicit limit until integrity hardening. File writing validates and
encodes before opening/truncating the destination, and reports write/flush/close
errors. The format is byte-explicit; cross-platform gameplay floating-point
determinism remains limited to the supported environment described in DETERMINISM.

## Verification tools

`faultline_persistence_tests` covers round-trip hashes/bytes for generator v1/v2/v3,
all RNG/allocator futures, 1000+1000 headless ticks with deterministic test-driven
random consumption/object allocation, NPC abstraction/rematerialization, dead
records, existing mission/vehicle data, cache rebuild and malformed inputs.
File tests create/clean `build/save-test-artifacts` using the configured binary
directory, including when invoked from another working directory. Cleanup removes
only the test's file and its newly created directory, with no recursive deletion.

`faultline_savecheck --seed 12345` measures in-memory encode/load with validation,
compares hashes and canonical bytes without creating any files.
`faultline_savecheck --inspect PATH` loads/validates an existing `.flsave` and
prints its seed/version/tick/hash. Magic, rather than extension, identifies files.
