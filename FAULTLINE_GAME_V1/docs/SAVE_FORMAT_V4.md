# Save format v4

SaveFormatVersion is 4; GameVersion remains 0.1.0 and GeneratorVersion remains 3.
Readers accept save versions 1..4 and generator versions 1..3. Prior v1/v2/v3
format documents remain historical references. Magic, primitive/string encoding,
explicit little-endian byte order, limits and typed error policy are unchanged.

The header uses the same 16-byte FAULTLINE_SAVE magic (two trailing zero bytes),
u32 save version, three u32 game version components, u32 generator version,
u64 seed, and u32 section count. V4 section count is 13. Section framing is
u32 tag plus u32 byte length, exact fixed order, no implicit padding or pointers.

Sections 1..12 retain their prior field encodings. Section 5 still contains the
legacy TerritoryState district/controller-mirror/residentMembers record. The
controller mirror is derived and verified against section 13; it is not a second
control authority. In v4, a contested/uncontrolled territory may have resident
members whose faction is explicitly stored in the new runtime section.

Section 13, TerritorySimulation:

1. Ten u32 rules in order: maximumPresence, minimumControlPresence,
   contestThreshold, gainMargin, holdMargin, garrisonUnit, initialEntrenchment,
   homeEntrenchment, attackPressureDivisor, defenseSurvivorUnit.
2. Four u64 counters: transitions, controlChanges, contestedTransitions,
   uncontrolledTransitions.
3. u32 territory count, exactly covering the current TerritoryStore; ascending
   TerritoryId u64, garrisonFaction u64, incumbent u64; u32 entrenchment count;
   ascending FactionId u64 plus positive u32 entrenchment per sparse entry.
4. u32 consumed-operation count; ascending OperationId u64 for every already
   applied Resolved/Failed operation, including historical results acknowledged
   during migration. Preparing/Active/Cancelled operations cannot appear here.

Authority is causal entrenchment plus the existing concrete garrison membership
and count, configured rules, and a minimal incumbent latch needed for temporal
hysteresis. Effective presence and Controlled/Contested/Uncontrolled are derived.
The incumbent exists only while control is held; it clears on loss. No separate
control enum is serialized. Derived control/deadline/member/spatial caches and
transient event buffers are excluded and rebuilt after decoding.

V1 -> v2 and v2 -> v3 keep their documented deterministic defaults. Migration
v3 -> v4 (also after loading older versions) imports current saved owner as the
initial garrison affiliation and incumbent, adds configured initial entrenchment
and a home anchoring increment, and derives identical initial control. Existing
resident manpower, reserves, money, IDs, clocks, NPC state and RNG words are not
changed. No world regeneration or RNG consumption occurs. All old Resolved/Failed
operations are marked consumed without replaying their outcomes: old garrison
losses were already applied. Active operations remain unconsumed and their future
resolution uses TerritorySystem. Counters start at zero. Initialization verifies
that the complete imported ownership map is preserved.

Validation checks map coverage, IDs/districts/factions, bounded positive sparse
entries, garrison membership, hysteresis latch, derived control and legacy mirror,
legal consumed operation references and complete consumption of terminal results.
Null/duplicate/unordered keys, invalid counts/rules, out-of-range values or
contradictory references fail explicitly, with no silent control repair.
Untrusted inputs retain 64 MiB file, 100000 per-collection and 300000 aggregate
collection-element budgets; lengths/offsets are checked before reads/allocations.

StateHash FAULTLINE.state.v6 adds every rule, territory input, incumbent, derived
control enum/controller, consumed-operation ID and counter in stable ID order.
The control cache itself is excluded. Same semantic state gives identical v4
bytes. Saving while contested preserves the hysteresis latch, presence and future
results; day-30 save/resume must match uninterrupted year-end hashes and bytes.
