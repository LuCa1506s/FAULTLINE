# Save format v3

Phase 10 writes SaveFormatVersion 3. GameVersion stays 0.1.0 and GeneratorVersion
stays 3. All three generator versions and save formats 1 and 2 remain readable.
[Version 1](SAVE_FORMAT_V1.md) and [version 2](SAVE_FORMAT_V2.md) retain their
historical contracts. There is no compression, checksum or atomic replacement yet.

Magic is the same 16 bytes: ASCII FAULTLINE_SAVE followed by two zero bytes.
Every integer/IEEE-754 binary32 is explicitly little-endian; typed IDs are u64,
zero means null. Booleans are u8 0/1, enums use their documented u8 type.
Strings remain u32 byte length plus strict UTF-8. Collections use u32 counts.
Header: magic, u32 save version, three u32 game version components, u32 generator
version, u64 world seed, u32 section count (12 for v3). Section headers are u32 tag
and u32 payload length. Ordering is fixed and extra/missing fields are rejected.

Sections 1..11 retain exactly the v2 wire order: Runtime, Generated, Entities,
Districts, Territories, Factions, NPCs, Vehicles, Missions, Relationships,
FactionSimulation. The historical NpcState encoding in Generated and NPCs does
**not** grow; generated baselines and their hashes remain frozen. Personal
relationship context additionally accepts MeaningfulInteraction=2 at runtime.
Generated relationships still accept only their original causal contexts.

Section 12, NpcSimulation:

1. Config: u64 update interval ticks, u64 minimum travel ticks, u32 estimated speed
   in meters per game minute (defaults 800, 80, 120).
2. Seven u64 bounded statistics: updates, travels started, completed, cancelled,
   operation participations, relationship changes, promotions.
3. u32 actor count; ascending NpcId u64 followed by its runtime record, covering
   exactly the semantic NPC store (including dead records).

NPC runtime record: u8 activity, home WorldLocation (district/territory/building,
each u64), u8 optional travel presence followed, if present, by origin and
 destination WorldLocations plus u64 departure and arrival ticks; finally u64
next semantic update tick and u64 assigned OperationId. Activity values are Idle=0,
Working=1, Travelling=2, Meeting=3, Hiding=4, FactionOperation=5, Arrested=6,
Recovering=7. Tick-to-GameTime conversion is unchanged (2250 game ms/tick).
UINT64_MAX is the unscheduled sentinel; all other deadlines fit SimulationClock.
Per-actor deadlines and travel are authority; the cached earliest deadline is
rebuilt after load. Event buffers and ephemeral actors are transient and excluded.
The existing NPC allocator/high-water mark remains in section 7.

Migration is incremental: v1 first receives the documented Phase 9 defaults;
v1/v2 then receive the Phase 10 defaults: home=current semantic location, Idle
for Alive, Recovering for Injured, Arrested for Arrested, Hiding for Missing,
Idle for Dead; no travel or operation assignment; next update=saved tick+800
for Alive, unscheduled otherwise. Overflow yields unscheduled, never wraparound.
Statistics start at zero. Migration consumes no RNG, reallocates no identities,
and changes no old section data. Foundation worlds without generated/domain
geography retain normalized WorldPosition as their semantic location; no district
is invented and semantic travel remains unavailable without generated geography.
Normal generated-world actors always require valid district/domain references.

StateHash schema FAULTLINE.state.v5 extends the prior semantic snapshot with the
new config, statistics and per-actor runtime fields in NpcId order. All personal
relationship values already belong to the semantic world hash. Derived indexes,
body links, event buffers and transient actors do not enter this extension.
Active NPCs still save as synchronized abstract semantic snapshots.

Error policy and resource budgets are unchanged: 64 MiB maximum file, 100000
records per collection, 300000 total collection objects, 65536 UTF-8 bytes per
string; bounds checked before allocation/offset arithmetic. Invalid enums, actor
keys, references, lifecycle/activity combinations, travel/deadlines or commitments
fail explicitly with section/offset/version diagnostics, without silent repair.
Travel counters must reconcile exactly with completed/cancelled/live journeys.
Same semantic state produces identical bytes within v3; migration changes header
and adds section 12, so cross-version whole-file equality is not expected.
