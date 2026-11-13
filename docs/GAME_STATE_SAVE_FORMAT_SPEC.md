# GAME STATE + SAVE FORMAT SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**File:** `12_GAME_STATE_SAVE_FORMAT_SPEC.md`  
**Parent specs:** `01` through `11`  
**Purpose:** Define persistence boundaries, versioning, save structure, runtime deltas, validation and migration.

---

# 1. CORE PRINCIPLE

Canonical model:

```text
BASE WORLD
=
WorldSeed
+
WorldGeneratorVersion

CURRENT WORLD
=
Base World
+
Persistent Runtime State / Deltas
```

---

# 2. SAVE MUST NOT SERIALIZE THE PROCESS

Persist game reality.

Do not persist:

```text
memory addresses
threads
renderer handles
OS handles
AI paths
physics contacts
cache ordering
```

---

# 3. SAVE HEADER

Every save contains:

```cpp
struct SaveHeader
{
    SaveFormatVersion saveVersion;
    GameVersion gameVersion;
    GeneratorVersion generatorVersion;
    WorldSeed worldSeed;
};
```

Also recommended:

```text
created game-time
last saved game-time
checksum/hash
```

---

# 4. PROFILE VS WORLD SAVE

Separate:

```text
PLAYER PROFILE
```

from:

```text
WORLD SAVE
```

Profile contains global achievements/settings if appropriate.

World save contains seed-specific progression.

---

# 5. WORLD SAVE

Persistent categories include:

```text
player state
NPC state
faction state
territory runtime state
market state
missions/story
police/wanted
vehicles/items
world changes
runtime RNG state
achievements for this seed
```

---

# 6. GENERATED BASE DATA

Data reconstructible exactly from:

```text
WorldSeed + GeneratorVersion
```

does not necessarily need complete duplication.

However implementation may temporarily serialize more for development simplicity.

Correctness first.

---

# 7. RUNTIME DELTAS

Examples:

```text
NPC #42 DEAD
Vehicle #18 moved
Building #281 door destroyed
Territory #9 contested
Faction #3 treasury changed
Market #7 supply changed
```

---

# 8. DELTA GRANULARITY

Do not prematurely optimize to byte-level deltas.

Initial save may serialize current persistent semantic state.

Later optimize toward:

```text
generated baseline
+
changed objects
```

---

# 9. AUTHORITATIVE SAVE SNAPSHOT

Save occurs at a safe synchronization point.

Pipeline:

```text
finish current simulation tick
↓
commit structural changes
↓
synchronize active persistent actors
↓
validate state
↓
serialize snapshot
```

---

# 10. ACTIVE NPC SYNC

Before save:

```text
active Entity
↓
persistent NpcState updated
```

so save does not depend on region remaining loaded.

---

# 11. ACTIVE VEHICLE SYNC

Same principle for persistent vehicles.

---

# 12. RNG STATE

Persist runtime deterministic RNG streams:

```text
combat
npc
missions
factions
market
police
story
```

This preserves future determinism across save/load.

---

# 13. SIMULATION CLOCK

Persist:

```text
tick
gameTime
scheduled strategic events
```

Wall clock is irrelevant.

---

# 14. ID ALLOCATORS

Persist runtime ID allocation state where IDs are not derivable.

Example:

```text
nextNpcId
nextVehicleId
nextMissionId
```

Never risk ID reuse after load.

---

# 15. SAVE FORMAT VERSION

Independent from game version.

Example:

```text
SaveFormatVersion = 3
GameVersion = 0.7.2
```

---

# 16. GENERATOR VERSION

Required to reconstruct exact base world.

Changing worldgen algorithm without changing generator version is prohibited once compatibility matters.

---

# 17. MIGRATION

Loading older save:

```text
read header
↓
identify save version
↓
migrate version-by-version
↓
validate
↓
construct current GameState
```

---

# 18. NO GIANT ONE-SHOT MIGRATOR

Prefer:

```text
v1 → v2
v2 → v3
v3 → v4
```

rather than one fragile:

```text
v1 → v14
```

function.

---

# 19. SAVE CONTAINER

Exact physical encoding is implementation choice.

Recommended early options:

```text
binary container for production
JSON/debug dump for developer inspection
```

The semantic schema is more important than file encoding.

---

# 20. DEBUG JSON

Development tools should support export:

```text
save_dump.json
```

for inspection.

Do not require production saves to remain human-readable.

---

# 21. SECTIONING

Logical save sections:

```text
HEADER
WORLD
PLAYER
NPCS
FACTIONS
TERRITORIES
MARKETS
MISSIONS
STORY
POLICE
VEHICLES
ITEMS
RNG
ACHIEVEMENTS
WORLD_DELTAS
```

---

# 22. CHECKSUM

Save should include integrity verification.

Corrupt save should fail clearly rather than silently create broken world state.

---

# 23. ATOMIC WRITE

Use:

```text
write temporary file
↓
flush
↓
validate
↓
atomic rename/replace
```

to reduce save corruption risk.

---

# 24. SAVE SLOTS

Architecture should support multiple save files/slots even if UI starts simple.

---

# 25. AUTOSAVE

Autosave may occur at controlled moments.

Do not save in middle of structural mutation.

---

# 26. LOAD PIPELINE

```text
read
↓
verify header/checksum
↓
migrate
↓
reconstruct/generated base
↓
apply persistent state
↓
rebuild caches
↓
validate references
↓
initialize Simulation
↓
resume
```

---

# 27. CACHE REBUILD

Do not serialize derived caches such as:

```text
spatial grid
navigation cache
faction member lookup cache
render visibility
```

unless later profiling proves useful.

---

# 28. STORY/MISSION REFERENCES

Dead or historical NPC records remain available so save references remain valid.

---

# 29. MISSING REFERENCE POLICY

Development build:

```text
fatal validation error
```

for unexpected missing persistent IDs.

Do not silently invent replacements.

---

# 30. PARTIAL RECOVERY

Future release builds may attempt safe recovery for non-critical corruptions.

Not a V1 requirement.

---

# 31. BASE WORLD HASH

Save may store:

```text
BaseWorldHash
```

to verify reconstructed generated world matches expected version/seed.

---

# 32. STATE HASH

Debug builds may store/compute:

```text
StateHash
```

for determinism testing.

---

# 33. SAVE SIZE

Do not optimize save size before correctness.

Semantic world size should remain manageable because generic population remains aggregated/ephemeral.

---

# 34. WORLD HISTORY

Persist only meaningful historical events, not every simulation tick.

---

# 35. DEAD NPCS

Dead persistent NPCs remain in save as historical records.

---

# 36. EPHEMERAL NPCS

Normally not saved individually.

If meaningful at save time, promote before persistence if necessary.

---

# 37. ACTIVE GENERIC FACTION MEMBERS

Must reconcile into semantic faction/resource state.

Do not duplicate aggregate and physical representations in save.

---

# 38. VERSIONED DEFINITIONS

Saved instances reference stable definition IDs.

Example:

```text
WeaponDefId
VehicleArchetypeId
BuildingArchetypeId
```

Definitions themselves belong to game data version.

---

# 39. REMOVED DEFINITIONS

Migration strategy must handle removed/renamed definition IDs explicitly.

No silent fallback to random item.

---

# 40. TESTS

Required:

```text
new world → save → load
save twice → equivalent state
save active NPC → load abstract
save abstract NPC → load active later
RNG future preserved
mission state preserved
dead NPC preserved
```

---

# 41. DETERMINISM TEST

```text
run 1000 ticks
save
run 1000 more → hash A

load save
run same 1000 ticks → hash B

require A == B
```

under supported determinism environment.

---

# 42. FAILURE TEST

Simulate:

```text
truncated save
bad checksum
unknown version
missing section
invalid ID
```

Error must be explicit.

---

# 43. FIRST MILESTONE

Serialize full persistent `GameState` simply.

Do not implement optimized deltas first.

---

# 44. SECOND MILESTONE

Add:

```text
versioning
migration
checksum
atomic write
debug dump
```

---

# 45. THIRD MILESTONE

Optimize generated baseline vs runtime delta storage if useful.

---

# 46. INVARIANTS

**S1** — Save represents semantic state, not process memory.

**S2** — Runtime RNG state is persisted.

**S3** — IDs remain stable after load.

**S4** — Active streaming state does not alter logical persistence.

**S5** — Save occurs at synchronization boundary.

**S6** — Generator version is recorded.

**S7** — Save format is independently versioned.

**S8** — Caches are rebuildable.

**S9** — Corrupt saves fail explicitly.

**S10** — Save/load can preserve deterministic future simulation.

---

# 47. STATUS

Next specification:

```text
13_RENDERER_INTERFACE_SPEC.md
```
