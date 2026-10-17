# Save format v8

SaveFormatVersion 8 appends mandatory ordered section 16 (StoryGraph). Sections
1..15 retain their complete v7 encoding. GeneratorVersion 3, HistoryVersion 1 and
GameVersion 0.1.0 are unchanged. The header is still 16-byte FAULTLINE_SAVE magic,
u32 save version, three u32 game-version components, u32 generator, u64 seed and
u32 section count (16). Every section has u32 tag and u32 payload byte length.
All primitives are explicitly little endian; no native C++ struct layout is used.

Section 16 payload, in order:

1. u32 StoryGraphVersion (0 = uninitialized, 1 = implemented).
2. u64 next StoryNodeId, u32 node count.
3. Ascending node records: u64 StoryNodeId; u8 StoryKind; six u64 references
   (faction, other faction, original contact NPC, current contact NPC, territory,
   district); u8 StoryOutcome; u32 prerequisite count; each prerequisite is u64
   StoryNodeId and u8 required StoryOutcome; finally u32 historyRecord.

StoryKind: Contact=0, BossOperation=1, Succession=2, Betrayal=3,
DealerConflict=4, MarketChange=5, TerritorialConflict=6, Alliance=7.
StoryOutcome: Unresolved=0, Succeeded=1, Failed=2. Prerequisites accept only 1/2.
historyRecord is a one-based index into section 15's compressed events, zero if
there is no history source. No strings, floats, pointers, EntityIds or caches occur
in this section. Existing global count/byte/UTF-8 limits remain unchanged; graph
validation further limits nodes to 4096. Enum/reference/history-context/edge/order
errors fail cleanly. The section's offset and name appear in read errors.

Version 0 requires zero nodes and allocator 1. Version 1 requires completed
HistoryVersion 1, at least one node and valid semantic references. Availability is
derived from saved world truth plus saved node outcomes, never a redundant field.
Dead original contacts remain historical references. Semantic validation rejects
malformed data rather than silently repairing it. Explicit runtime repair is a
separate operation. Save/load preserves graph bytes and deterministic future
continuation, including unresolved, resolved and invalidated opportunities.

Readers accept v1..v8. v1..v7 migrate to the exact empty/default graph, preserving
all existing runtime domains, RNG and allocators. They do not invent completed
history or story outcomes. An already completed history can explicitly initialize
its graph. The real frozen v7 fixture is retained unchanged; it is a day-one save
without completed history, not a fabricated four-year snapshot. A separate test
projects the unchanged fifteen-section v7 wire prefix of a completed history and
verifies migration, without claiming that projection is a historical fixture.

StateHash is now `FAULTLINE.state.v10`. Historical StateHash values intentionally
change with this schema; historical BaseWorldHash values do not.
