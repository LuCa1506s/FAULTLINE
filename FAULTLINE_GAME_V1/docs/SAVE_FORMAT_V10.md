# Save Format v10 — Prologue State

State introduced: Phase 16 (PRIMARY 001 / FIRST DELIVERY).

## Versions

- **SaveFormatVersion**: 9 → 10
  - v9: Phase 15 Mission Core (MissionSimulation section added)
  - v10: Phase 16 Prologue (Prologue section added)
- **StateHash**: `FAULTLINE.state.v11` → `FAULTLINE.state.v12`
  - v12 adds `appendPrologueStateHash` covering version, phase, firstDelivery,
    origin, giver, recipient, and all PrologueEvidence fields.
- **GameVersion**: unchanged at 0.1.0
- **GeneratorVersion**: unchanged at 3
- **HistoryVersion**: unchanged at 1
- **StoryGraphVersion**: unchanged at 1

## Sections

v10 adds one new section, appended as section 18 (final section):

| Section | Index | Version added | Content |
|---------|-------|---------------|---------|
| Prologue | 18 | v10 | PrologueState: version, phase, firstDelivery MissionId, origin FactionId, giver/recipient NpcId, PrologueEvidence |

All prior sections (1–17) are unchanged from v9.

## Schema

PrologueState serializes as:
```
uint32 version
uint8  phase            // ProloguePhase: 0=PreMission, 1=MissionAvailable, 2=MissionActive, 3=DeliveryComplete, 4=Complete
uint64 firstDelivery    // MissionId
uint64 origin          // FactionId
uint64 giver           // NpcId
uint64 recipient       // NpcId
-- PrologueEvidence --
uint64 origin          // FactionId
uint64 rival           // FactionId
uint64 witness         // NpcId
uint64 location        // TerritoryId
uint8  incident        // DiscoveryIncident: 0..5
uint64 deliveryTick
uint64 discoveryTick
bool   firstDeliveryCompleted
```

## Migration (v9 → v10)

When loading a v9 save:
- The Prologue section is absent.
- `simulation::initializePrologueState(state)` is called, setting `state.prologue`
  to `PrologueState{}` (version=1, phase=PreMission, all IDs null, default evidence).
- No other section is modified; all prior bytes survive migration unchanged.
- Re-saving produces a canonical v10 save with the default Prologue section.

## Fixtures

| Fixture | Version | Source | SHA-256 |
|---------|---------|--------|---------|
| `save_v8_phase14.flsave` | v8 | Phase 14 `faultline_story.exe --seed 12345 --fixture-prologue` | `df1574a61f4005a182a7bf8dec87e7fbca4993a8076dcae9df65a0c947376f08` |
| `save_v9_phase15.flsave` | v9 | Phase 15 `faultline_story.exe --seed 12345 --fixture-prologue` | `d376e7f56e4787685cacff4c64402f143ea2e5aff455868ec85f6a875c355c7c` |

All fixtures remain binary (`*.flsave -text binary` in `.gitattributes`). Do not
regenerate with the current writer to mask compatibility regressions.
