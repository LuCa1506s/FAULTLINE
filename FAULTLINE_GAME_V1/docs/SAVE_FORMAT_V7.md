# Save format v7

SaveFormatVersion is 7; GeneratorVersion remains 3; GameVersion is 0.1.0.
Readers accept v1..v7. Sections 1..14 preserve their complete v6 wire encoding.
The file contains fifteen mandatory, ordered sections. Existing bounded binary
framing, collection/global resource limits and typed diagnostics remain in force.

Section 15 (StrategicHistory), little endian, in order:

1. u32 HistoryVersion (0 = no history, 1 = current algorithm), u32 completed
   months, u64 startTick.
2. PrologueEvidence: four u64 IDs (origin faction, rival faction, witness NPC,
   location territory), u8 DiscoveryIncident (0..5), u64 deliveryTick,
   u64 discoveryTick, one canonical bool firstDeliveryCompleted.
3. u32 informed count and ascending unique u64 FactionIds.
4. u32 event count, then chronological records: u8 HistoryEventKind (0..7),
   u32 month, u64 faction, u64 other faction, u64 NPC, u64 territory, i64 cost.
   Kinds are FIRST_DELIVERY, RIVAL_DISCOVERY, MARKET_ENTRY, DEALER_EXPANSION,
   TERRITORIAL_SETTLEMENT, MARKET_DISPUTE, COOPERATION, NETWORK_MAINTENANCE.

No-history saves require the exact empty/default history value. Version 1 stores
at most 48 months, five informed factions and 2048 compressed events. References,
ordered/unique informed IDs, event month/order/enums/costs, prologue ordering and
time ranges are validated. Partial history requires the exact monthly boundary;
completed history permits later runtime time. The loader restores saved economic,
NPC, faction and territory consequences; it never reapplies events.

All v1..v6 migrations default to empty history without inventing delivery,
discovery, revenue, NPCs or territorial changes. Old domains, RNG and allocators
are untouched. The real pre-Phase-13 v6 day-30 fixture verifies all fourteen old
section payloads byte for byte through migration and future deterministic resume.
Existing independent v1..v5 fixtures and migration assertions remain unchanged.

Runtime StateHash is now FAULTLINE.state.v9, covering every history field,
evidence, ordered informed list and compressed event. History-generated NPCs use
the existing persisted NpcId allocator; no new persistent ID namespace or global
RNG stream is added. Monthly checkpoints preserve custom market rules/fractional
credits and resume exactly. The immutable prehistory BaseWorldHash is unchanged.
