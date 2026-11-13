# Save Format v11 — Player and logical streaming residency

Version 11 is the single Phase 17 schema increment from v10. GameVersion remains
0.1.0, GeneratorVersion 3. Encoding, magic, little-endian fixed primitives, bounded
length prefixes, canonical ordering and typed failure policy remain unchanged.
StateHash is `FAULTLINE.state.v13`; PlayerStateVersion starts at 1.

There are 19 ordered sections. Sections 1..18 retain v10 field encodings EXCEPT
Entities (3), which now writes a CharacterBody component collection after Health
and before VehicleLink. The v11 reader gates that extra collection on version >=11;
v1..v10 Entities payloads are never interpreted with the new field order.

Each CharacterBody entry contains EntityId u64 followed by yaw, movementSpeed,
stanceHeight (three f32). Its collection is count u32 then ascending EntityId entries.
The player body persists with Transform/Health. Ordinary NPC bodies remain transient
and are excluded, after synchronizing semantic position/health into their NpcState.

Section 19 (Player) payload is:

| Field | Wire type |
| --- | --- |
| version | u32 (=1) |
| controlledEntity | u64 (0 means absent) |
| spawnPosition.cell.x/z | two i32 |
| spawnPosition.local.x/y/z | three f32 |
| yaw synchronized body-facing cache | f32 |
| residency count | bounded u32 |
| each resident, ascending NpcId | NpcId u64, reserved EntityId u64 |

Logical streaming residency is necessary persistent scheduling/allocation state,
not a serialized NPC body. It preserves hysteresis and already issued handle
reservations. The loader leaves these NPCs abstract; the tick reconstructs their
physical components from NpcState before semantic rules run, without allocating
another ID. No cache, bucket order, path, contact or pointer is serialized.
See [PLAYER_CONTROLLER.md](PLAYER_CONTROLLER.md) for the exact boundary.

V1..v10 migration initializes default PlayerState: version 1, no controlled entity,
zero spawn/yaw, empty residency, no CharacterBodies. It does not replay FIRST
DELIVERY, create a player, change history, materialize NPCs or consume RNG.
The v10 Prologue payload is preserved. Earlier mission/story/history/market/NPC/
territory/operation migrations retain their existing rules.

The centralized validator checks player identity, body components, facing cache,
normalized positions and ordered valid leases. Invalid, duplicate, dangling,
nonfinite, truncated and oversized data fail through the existing SaveError
model. No silent repair of corrupted identity, allocator or facing is performed.

Historical migration tests compare every old payload byte; only the newly added,
required-empty CharacterBody collection is projected out of the v11 Entities
payload for that comparison. They do not compare shifted absolute offsets or
reinterpret a new save as an old fixture. The authentic v10 fixture is emitted
by a recovered Phase 16 v10 writer, independently reproduced byte-identically.
Provenance and checksums are in tests/fixtures/README.md.
