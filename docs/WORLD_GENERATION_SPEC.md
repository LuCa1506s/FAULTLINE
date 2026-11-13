# WORLD GENERATION SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**File:** `05_WORLD_GENERATION_SPEC.md`  
**Parent specs:** `ENGINE_ARCHITECTURE_SPEC`, `ENGINE_CORE_RUNTIME_SPEC`, `ENTITY_DOMAIN_DATA_MODEL_SPEC`, `WORLD_SPATIAL_STREAMING_SPEC`  
**Purpose:** Define the deterministic pipeline that transforms a world seed into a complete, coherent base world.

---

# 1. PRIMARY REQUIREMENT

The world generator must satisfy:

```text
same WorldSeed
+
same WorldGeneratorVersion
=
same generated base world
```

The result must not depend on:

- system time;
- thread scheduling;
- memory addresses;
- unordered container iteration;
- random device state;
- renderer state;
- machine performance.

---

# 2. GENERATOR OUTPUT

World generation creates the **base world**.

It does NOT create runtime history caused by the player.

Output includes:

```text
WorldMetadata

Geography
Water

MacroZones
Districts
RoadNetwork
Buildings

Territories

Factions
ImportantNPCs
InitialRelationships

FourYearHistory

Markets

StoryGraph
InitialMissions

InitialSandboxState
```

---

# 3. GENERATION PIPELINE

Canonical high-level order:

```text
WORLD SEED
    ↓
WORLD PARAMETERS
    ↓
GEOGRAPHY
    ↓
WATER
    ↓
MACRO-ZONES
    ↓
ROAD NETWORK
    ↓
DISTRICTS
    ↓
BUILDING LAYOUT
    ↓
TERRITORIES
    ↓
FACTIONS
    ↓
IMPORTANT NPCs
    ↓
INITIAL RELATIONSHIPS
    ↓
PROLOGUE CONDITIONS
    ↓
FOUR-YEAR HISTORY
    ↓
MARKET
    ↓
STORY GRAPH
    ↓
INITIAL MISSIONS
    ↓
SANDBOX VALIDATION
    ↓
FINAL BASE WORLD
```

Each stage receives explicit inputs and produces explicit outputs.

---

# 4. STAGE ISOLATION

No generation stage may casually consume one global RNG stream.

Forbidden:

```cpp
WorldRng rng(seed);

generateWater(rng);
generateRoads(rng);
generateFactions(rng);
generateNPCs(rng);
```

Reason:

adding one random call inside water generation would change:

```text
roads
factions
NPCs
story
```

This would make the whole world unstable across generator revisions.

---

# 5. HIERARCHICAL SUB-SEEDS

Each subsystem derives its own seed.

Conceptually:

```text
WorldSeed = 12345

hash(12345, "world.parameters")
hash(12345, "geography")
hash(12345, "water")
hash(12345, "macro_zones")
hash(12345, "roads")
hash(12345, "districts")
hash(12345, "buildings")
hash(12345, "territories")
hash(12345, "factions")
hash(12345, "npc")
hash(12345, "history")
hash(12345, "market")
hash(12345, "story")
hash(12345, "missions")
```

Further sub-seeds should derive from persistent IDs.

Example:

```text
hash(WorldSeed, "faction", FactionId)
```

or:

```text
hash(WorldSeed, "building", BuildingId)
```

---

# 6. SEED DERIVATION

Seed derivation must use a stable algorithm.

Do NOT use:

```cpp
std::hash<std::string>
```

as a persistent procedural-generation contract.

Its behavior is not intended as a stable serialized procedural algorithm.

The project must implement or select an explicitly stable deterministic hash/mixer.

Candidate family:

```text
64-bit stable hash
+
64-bit RNG
```

Exact algorithm must be locked before worldgen code is considered stable.

---

# 7. RNG ALGORITHM

The generator requires a deterministic PRNG with:

- stable documented algorithm;
- good statistical quality;
- low implementation complexity;
- explicit internal state;
- fast generation.

Suitable choices include algorithms from families such as:

```text
PCG
xoshiro
xoroshiro
```

The exact algorithm becomes part of:

```text
WorldGeneratorVersion
```

Changing it invalidates deterministic compatibility with older generator versions unless migration support exists.

---

# 8. GENERATOR VERSIONING

Every generated world records:

```cpp
struct GeneratorMetadata
{
    uint32_t generatorVersion;
    uint64_t worldSeed;
};
```

Example:

```text
WorldSeed: 12345
WorldGeneratorVersion: 1
```

If generator behavior changes incompatibly:

```text
GeneratorVersion 1
→
GeneratorVersion 2
```

Old saves must know which generation rules created their base world.

---

# 9. WORLD PARAMETERS

The first generation phase derives top-level parameters.

Example:

```text
world width
world depth
target macro-zone count
water amount
terrain variation
urban density
population target
road density
district density
```

For V1:

```text
Target size:
8000 × 8000 m
```

but actual architecture remains configurable.

---

# 10. WORLD BOUNDS

World generation defines explicit playable bounds.

Conceptually:

```cpp
WorldBounds
{
    minX;
    maxX;
    minZ;
    maxZ;
}
```

Nothing should assume the world begins exactly at:

```text
0,0
```

although V1 may choose that representation.

---

# 11. GEOGRAPHY GENERATION

Geography defines the broad physical form of the city.

Possible features:

```text
landmass
coastline
river
lake
hills
flat areas
waterfront
major natural barriers
```

V1 does not require realistic geological simulation.

The objective is:

```text
coherent urban geography
```

not natural-science accuracy.

---

# 12. GEOGRAPHY REQUIREMENTS

Generated geography must:

- leave enough buildable land;
- permit a connected city;
- support meaningful road layout;
- avoid excessive unusable terrain;
- allow variety between seeds;
- influence district archetype selection.

Example:

```text
large coastline
→ harbor possible

no water
→ harbor impossible
```

---

# 13. WATER

Water is a world-generation constraint, not decorative geometry.

Water affects:

```text
roads
bridges
district archetypes
building placement
territory boundaries
future boats
industrial placement
tourist zones
```

Water regions must be represented semantically.

Example:

```cpp
WaterRegion
{
    WaterRegionId id;
    Polygon boundary;
    WaterType type;
}
```

---

# 14. MACRO-ZONES

Each world generates approximately:

```text
5–10 macro-zones
```

Macro-zone examples:

```text
Downtown
Industrial
Tourist
Park
Little Italy
Chinatown
Ghetto
Financial
Residential
Suburbs
Harbor
Warehouse
Entertainment
University
Waterfront
```

Not every seed contains every archetype.

---

# 15. MACRO-ZONE SELECTION

Selection depends on:

```text
geography
water
urban density
world size
population
adjacent zone compatibility
```

Example compatibility:

```text
Harbor
requires:
meaningful water adjacency
```

```text
Financial District
prefers:
central/high-access area
```

```text
Industrial
prefers:
edge areas
major roads
harbor/rail/logistics access
```

Exact scoring belongs to zone-generation rules.

---

# 16. MACRO-ZONE PLACEMENT

Macro-zones should form contiguous meaningful areas.

Avoid:

```text
tiny isolated zone fragments
checkerboard city
random scattered archetypes
```

Possible generation method:

```text
seed points
↓
weighted region growth
↓
geography-aware boundaries
↓
connectivity correction
```

The exact geometric algorithm remains an implementation choice.

---

# 17. ZONE ADJACENCY GRAPH

World generation produces:

```text
MacroZoneAdjacencyGraph
```

Example:

```text
Downtown
├── Financial
├── Entertainment
└── Chinatown
```

This graph becomes useful for:

- roads;
- population movement;
- faction boundaries;
- mission generation;
- market connectivity.

---

# 18. ROAD NETWORK

Road generation is one of the most important worldgen systems.

The network must be:

```text
connected
navigable
hierarchical
district-aware
```

Canonical structure:

```text
RoadGraph
├── RoadNodes
└── RoadEdges
```

---

# 19. ROAD HIERARCHY

Initial categories:

```text
ARTERIAL
MAJOR
LOCAL
SERVICE
```

Potential future categories:

```text
HIGHWAY
ALLEY
PRIVATE
```

Each category influences:

```text
width
speed
traffic
building access
intersection frequency
```

---

# 20. ROAD GENERATION ORDER

Recommended structure:

```text
1. major inter-zone connections
2. main arterials
3. district-level network
4. local roads
5. cleanup/connectivity
```

Do not generate every street independently.

The city needs large-scale structure first.

---

# 21. ROAD CONNECTIVITY INVARIANT

Core urban network must be traversable.

Validation must detect:

```text
isolated road islands
unreachable districts
dead network components
invalid bridge connections
roads ending inside water
```

Some intentionally isolated locations may exist.

They must be explicitly marked as such.

---

# 22. BRIDGES

When road topology crosses water:

```text
road route
+
valid crossing
→ bridge candidate
```

Bridge generation depends on:

```text
crossing length
road importance
terrain
connectivity importance
```

The generator must avoid creating excessive bridges solely to repair poor road layout.

---

# 23. DISTRICTS

Macro-zones are subdivided into districts.

Example:

```text
SOUTH SIDE
├── South Heights
├── Jefferson Blocks
└── Riverside
```

District generation depends on:

```text
macro-zone size
roads
population density
natural barriers
urban layout
```

---

# 24. DISTRICT GEOMETRY

Each district stores:

```text
DistrictId
MacroZoneId
Boundary
Archetype
PopulationBaseline
UrbanDensity
EconomicProfile
```

Districts must cover the intended urban space without invalid overlap.

---

# 25. DISTRICT IDENTITIES

Two districts of the same macro-zone archetype should still vary.

Example:

```text
Residential District A
→ wealthy
→ low density
→ low criminal presence

Residential District B
→ dense
→ economically weak
→ higher criminal pressure
```

Zone archetype is not the complete district identity.

---

# 26. BUILDING GENERATION

Buildings are generated only after:

```text
roads
districts
```

are known.

Pipeline:

```text
road blocks
↓
parcels
↓
building archetype selection
↓
footprint
↓
orientation
↓
building metadata
```

---

# 27. BUILDING PLACEMENT

Buildings should normally align with road access.

Avoid:

```text
doors facing nowhere
buildings blocking arterial roads
overlapping footprints
buildings inside water
```

Building placement rules depend on district archetype.

---

# 28. BUILDING ARCHETYPES

Examples:

```text
Apartment
House
Store
Warehouse
Office
Industrial
Restaurant
Garage
PoliceStation
FactionSafehouse
Hotel
Nightclub
```

Most buildings initially require only:

```text
exterior representation
semantic metadata
```

not detailed interiors.

---

# 29. INTERIORS

V1 does not generate full interiors for every building.

Buildings may have:

```text
NO_INTERIOR
SIMPLE_INTERIOR
MISSION_INTERIOR
SPECIAL_INTERIOR
```

Interior generation remains separate from city topology.

---

# 30. TERRITORIES

Territories are generated after districts and basic urban structure.

A territory is a criminal-control region.

It should make sense relative to:

```text
roads
district boundaries
neighborhood identity
faction structure
```

Territories should not be arbitrary geometric squares.

---

# 31. TERRITORY SUBDIVISION

Districts may contain:

```text
1
or
multiple territories
```

depending on:

```text
district size
population
criminal activity
future faction structure
cohesion
```

Initial territory geometry should create useful gameplay boundaries.

---

# 32. TERRITORY CONNECTIVITY

Territories should normally be contiguous.

A faction's initial territory may consist of multiple adjacent territories.

Avoid inexplicable disconnected initial territory unless generated history explicitly explains it.

---

# 33. FACTION GENERATION

Exactly:

```text
5 main factions
```

are generated for the initial sandbox.

They derive from four canonical macro-families.

One macro-family therefore appears twice.

The seed decides which one.

---

# 34. FACTION GENERATION DATA

Each faction receives:

```text
FactionId
name
macro-family
cultural/display identity
home area
specialty
hierarchy style
cohesion
initial wealth
initial strength
major NPC slots
dealer profile
rivalry tendencies
alliance tendencies
```

No faction should be an exact clone.

---

# 35. DUPLICATE MACRO-FAMILY DIFFERENTIATION

If two factions share the same macro-family:

```text
StreetGang A
StreetGang B
```

they still need meaningful differences.

Possible dimensions:

```text
wealth
territorial style
aggression
cohesion
hierarchy
specialty
geography
relationships
leadership
```

---

# 36. FACTION HOME TERRITORY

Each faction receives a valid home territory.

Requirements:

```text
exists
accessible
meaningful size
not initially owned by another faction
compatible with faction identity
```

Initial home territories should not force one faction into immediate guaranteed collapse.

---

# 37. INITIAL POWER BALANCE

Post-timeskip sandbox must begin in unstable equilibrium.

Therefore faction generation must avoid:

```text
Faction A = 80% city control
Faction B = nearly dead
```

unless future intentional game modes allow this.

Initial target:

```text
all 5 factions viable
none has already won
```

Variation is allowed.

Perfect equality is not required.

---

# 38. IMPORTANT NPC GENERATION

Target:

```text
approximately 15–25 major NPCs
```

across the world.

Possible roles:

```text
Boss
Second-in-command
Important Dealer
Trusted Member
Rival
Police Figure
Potential Traitor
```

---

# 39. NPC GENERATION PER FACTION

Each faction should have enough semantic leadership structure to support:

```text
succession
betrayal
missions
relationships
internal conflict
```

Minimum V1 concept:

```text
Leader
Second-level figure
Important dealer/member
```

Exact count may vary.

---

# 40. NPC IDENTITY

Major NPC generation includes:

```text
name
age band
role
personality traits
respect
loyalty
competence
ambition
relationships
faction role
home location
```

Avoid generating giant personality simulations with dozens of meaningless numbers.

Every generated trait should have gameplay utility.

---

# 41. NPC TRAIT PRINCIPLE

A trait should exist only if it influences:

```text
decisions
relationships
missions
faction behavior
dialogue
story consequences
```

If it has no gameplay effect, it probably does not belong in V1 worldgen.

---

# 42. INITIAL RELATIONSHIPS

After NPC and faction generation:

```text
Faction ↔ Faction relationships
NPC ↔ NPC relationships
```

are generated.

Faction relations may include:

```text
alliance
neutrality
tension
rivalry
war-risk
```

The initial world should already contain political pressure.

---

# 43. RELATIONSHIP COHERENCE

Relationships must reflect generated history.

Bad:

```text
Faction A hates Faction B
```

with no generated cause anywhere.

Better:

```text
past territory dispute
→ resentment
→ present tension
```

History and relationships should reinforce one another.

---

# 44. PROLOGUE BASE STATE

Before the four-year history:

```text
player-selected faction
```

is the first faction involved with the fictional new drug.

The canonical prologue requires:

```text
FIRST DELIVERY
```

and before the prologue ends:

```text
at least one rival discovers the drug
```

The exact discovery incident derives from the seed.

---

# 45. DISCOVERY INCIDENT GENERATION

Possible incident families:

```text
rival witness
betrayal
accidental exposure
police interference
wrong destination
information leak
```

World generation may preselect:

```text
incident type
relevant NPC candidates
location context
```

while exact runtime resolution still depends on gameplay.

---

# 46. FOUR-YEAR HISTORY

After the canonical first mission:

```text
4 YEARS LATER
```

The generator simulates a compressed four-year history.

This is NOT merely random assignment.

It should use simplified versions of actual world systems.

---

# 47. HISTORY PURPOSE

The four-year simulation explains:

```text
how all five factions entered the market
how territories stabilized
how dealers appeared
how rivalries formed
how alliances formed
why current tensions exist
```

The player should enter a world that feels like it already has history.

---

# 48. HISTORY SIMULATION LEVEL

Do NOT simulate four years at 20 Hz.

History uses strategic steps.

Example:

```text
weekly
monthly
event-driven
```

depending on system.

Conceptually:

```text
4 years
→ strategic simulation
→ resulting state
```

---

# 49. HISTORY EVENTS

Possible generated historical events:

```text
territorial dispute
leadership change
dealer expansion
alliance
alliance collapse
betrayal
police crackdown
financial problem
market expansion
internal faction conflict
```

Events should leave persistent consequences.

---

# 50. HISTORY RECORD

The generator may preserve a compressed world history.

Example:

```text
Year 1:
Faction #2 begins dealing product

Year 2:
Faction #4 takes Territory #18

Year 3:
Alliance #2/#5 collapses

Year 4:
Dealer conflict begins in District #7
```

This history can support:

```text
missions
dialogue
relationships
story graph
debugging
```

---

# 51. INITIAL INSTABILITY

After the four-year timeskip, at least one meaningful instability must exist.

Examples:

```text
boss illness
possible betrayal
broken alliance
import problem
dealer conflict
police pressure
financial weakness
territorial dispute
```

Prefer several low/medium tensions over one world-breaking crisis.

---

# 52. MARKET GENERATION

The fictional drug market exists city-wide after the timeskip.

Market generation depends on:

```text
population
district wealth
criminal presence
dealer network
faction presence
history
police pressure
```

---

# 53. MARKET BASELINE

Each relevant district receives:

```text
demand baseline
supply baseline
price baseline
dealer capacity
faction market shares
```

These values must form a stable playable starting state.

---

# 54. MARKET VALIDATION

Avoid:

```text
zero demand everywhere
infinite supply
negative prices
single faction owns 100%
no dealers in entire city
```

unless an explicit special world type later allows such states.

---

# 55. DEALER GENERATION

Dealers are generated from:

```text
persistent NPCs
generic faction population
territorial market needs
```

Important dealers receive persistent `NpcId`.

Not every minor dealer necessarily needs major narrative status.

---

# 56. STORY GRAPH GENERATION

The broad story graph is generated only after:

```text
world
factions
NPCs
history
market
relationships
```

exist.

Story must derive from world state.

Do not generate story first and force the city to fit it.

---

# 57. STORY GRAPH INPUTS

Inputs include:

```text
faction rivalries
important NPC relationships
historical conflicts
territorial tensions
market weaknesses
leadership instability
police pressure
player starting faction
```

---

# 58. STORY GRAPH NODES

Possible node families:

```text
conflict
alliance
betrayal
succession
territorial war
dealer conflict
boss operation
market change
police escalation
```

Nodes reference domain IDs.

---

# 59. STORY GRAPH MUST BE MUTABLE

World generation creates:

```text
possible future structure
```

not fixed destiny.

Runtime gameplay may:

```text
invalidate
replace
mutate
redirect
resolve
```

story nodes.

---

# 60. INITIAL MISSIONS

The generator creates or prepares:

```text
Primary 001
FIRST DELIVERY
```

plus post-timeskip opportunities.

Side missions should derive from current generated world conditions.

Example:

```text
territorial tension
→ protection mission

dealer conflict
→ intimidation mission

weak faction finances
→ robbery opportunity
```

---

# 61. GENERATED NAMES

Names for:

```text
factions
NPCs
districts
businesses
streets
```

must use deterministic name-generation streams.

Do not consume unrelated RNG streams.

---

# 62. NAME COLLISION VALIDATION

Generated names should avoid obvious duplicates.

Example:

```text
two major factions with identical names
```

must be rejected/regenerated deterministically.

Collision resolution must itself be deterministic.

---

# 63. PROCEDURAL IDS

Generated objects should receive stable IDs based on generation order within explicitly stable local domains.

Example:

```text
DistrictId #1
DistrictId #2
```

is acceptable if district ordering is deterministic.

More robust derivation may use:

```text
stable generation key
→ deterministic ID
```

Exact allocation method will be fixed during implementation.

---

# 64. ID ORDERING REQUIREMENT

If IDs depend on generation order, that order must be explicit.

Do not derive persistent IDs from iteration over:

```text
unordered_map
unordered_set
pointer collections
```

unless sorted first.

---

# 65. VALIDATION PHASE

World generation is not complete until validation passes.

Pipeline:

```text
generate
↓
validate
↓
repair if safe
↓
validate again
↓
accept / reject
```

---

# 66. REPAIR VS REGENERATION

Minor geometric problems may be repaired.

Example:

```text
building slightly overlaps road
→ adjust placement
```

Major structural failures should cause deterministic stage regeneration.

Example:

```text
road graph disconnected
```

may regenerate road network using:

```text
same stage seed
+
attempt index
```

---

# 67. DETERMINISTIC RETRY

Retry randomness:

```text
hash(stageSeed, attemptNumber)
```

Example:

```text
roads attempt 0
roads attempt 1
roads attempt 2
```

This makes failure recovery deterministic.

---

# 68. RETRY LIMITS

Every stage must have finite retry limits.

Forbidden:

```cpp
while (!valid)
{
    regenerate();
}
```

with no upper bound.

Use:

```text
MAX_ATTEMPTS
```

then fail world generation with diagnostics.

---

# 69. WORLD VALIDATION — GEOGRAPHY

Check:

```text
enough buildable land
valid world bounds
valid water geometry
no impossible region topology
```

---

# 70. WORLD VALIDATION — ROADS

Check:

```text
core road graph connected
districts reachable
valid intersections
roads not unintentionally inside water
bridges valid
```

---

# 71. WORLD VALIDATION — DISTRICTS

Check:

```text
valid boundaries
no invalid overlaps
every district belongs to macro-zone
reasonable size
road access
```

---

# 72. WORLD VALIDATION — BUILDINGS

Check:

```text
no invalid overlap
valid parcel
valid road access where required
not placed in forbidden water/road areas
```

---

# 73. WORLD VALIDATION — TERRITORIES

Check:

```text
valid geometry
district association valid
no impossible initial ownership
sufficient playable territory
```

---

# 74. WORLD VALIDATION — FACTIONS

Check:

```text
exactly 5 main factions
valid home territory
valid leader
valid family
no duplicate IDs
all factions viable
```

---

# 75. WORLD VALIDATION — NPCs

Check:

```text
all major NPC IDs valid
faction references valid
leadership references valid
no impossible lifecycle state
relationships reference existing actors
```

---

# 76. WORLD VALIDATION — MARKET

Check:

```text
market exists
dealers exist
all five factions have plausible market access
valid prices
valid shares
```

---

# 77. WORLD VALIDATION — STORY

Check:

```text
all referenced NPCs exist
all referenced factions exist
all referenced territories exist
no impossible mandatory branch
prologue conditions valid
```

---

# 78. INITIAL PLAYABILITY VALIDATION

Final world must satisfy:

```text
player can spawn
starting faction exists
first mission can execute
first delivery location reachable
required rival discovery can occur
post-timeskip sandbox is viable
```

---

# 79. GENERATION REPORT

Development builds should produce a report.

Example:

```text
WORLD GENERATION REPORT

Seed: 12345
Generator: v1

World:
8000 × 8000 m

Macro-zones: 7
Districts: 19
Territories: 31
Buildings: 14,821

Road nodes: 4,291
Road edges: 5,848

Main factions: 5
Major NPCs: 21
Dealers: 47

Generated history events: 64
Story nodes: 23

Validation:
PASS

BaseWorldHash:
71D2AA9B...
```

---

# 80. GENERATION HASH

After generation, compute:

```text
BaseWorldHash
```

from canonical serialized base-world data.

This allows:

```text
seed 12345
run A
→ HASH X

seed 12345
run B
→ HASH X
```

Requirement:

```text
X == X
```

---

# 81. HASH MUST EXCLUDE TRANSIENT DATA

Do not hash:

```text
pointer addresses
cache order
render state
runtime timestamps
debug counters
```

Only canonical generated world state.

---

# 82. HEADLESS WORLDGEN TOOL

Required development executable:

```text
faultline_worldgen
```

Example commands:

```bash
faultline_worldgen --seed 12345
```

```bash
faultline_worldgen --seed 12345 --validate
```

```bash
faultline_worldgen --seed 12345 --dump world.json
```

```bash
faultline_worldgen --range 1 10000 --validate
```

---

# 83. MASS-SEED TESTING

World generation must be testable across thousands of seeds.

Example:

```text
Seeds tested: 100,000

Generation success:
99.999%

Invalid road networks:
0

Missing faction leaders:
0

Unreachable spawn:
0
```

Any failed seed must be reproducible exactly.

---

# 84. PROPERTY-BASED WORLD TESTING

Do not test only specific expected worlds.

Also test invariants.

Example:

```text
FOR random seed:

assert factionCount == 5
assert roadGraph.connected()
assert playerSpawn.valid()
assert majorNpcCount > 0
```

This will be more valuable than manually inspecting a handful of cities.

---

# 85. PERFORMANCE TARGET

Generation does not initially need to be instantaneous.

Correctness comes first.

However, architecture should allow later optimization via:

```text
parallel independent generation stages
cached generation
lazy geometry generation
```

without changing deterministic semantics.

---

# 86. PARALLEL GENERATION

Parallel generation may be introduced only where dependencies are explicit.

Example:

```text
Building generation in independent districts
```

could run in parallel.

But results must be merged using deterministic order.

Thread completion order must never change the world.

---

# 87. LAZY GENERATION

Large low-level detail may eventually be generated lazily.

Example:

```text
building decorative details
ephemeral local props
```

using deterministic sub-seeds.

But anything affecting strategic gameplay must already be represented semantically in the base world.

---

# 88. GAMEPLAY-RELEVANT VS DECORATIVE PROCEDURAL DATA

Gameplay-relevant:

```text
road connectivity
territories
building type
NPCs
factions
markets
story
```

must be part of canonical world generation.

Decorative:

```text
minor trash props
decorative signs
cosmetic variation
```

may be generated on demand.

---

# 89. WORLD GENERATOR MODULE STRUCTURE

Recommended:

```text
world/generation/
│
├── WorldGenerator
│
├── SeedDerivation
│
├── WorldParameterGenerator
│
├── GeographyGenerator
│
├── WaterGenerator
│
├── MacroZoneGenerator
│
├── RoadGenerator
│
├── DistrictGenerator
│
├── BuildingGenerator
│
├── TerritoryGenerator
│
├── FactionGenerator
│
├── NpcGenerator
│
├── HistoryGenerator
│
├── MarketGenerator
│
├── StoryGraphGenerator
│
├── MissionSeedGenerator
│
└── WorldValidator
```

---

# 90. WORLD GENERATOR API

Conceptually:

```cpp
class WorldGenerator
{
public:
    GeneratedWorld generate(
        WorldSeed seed,
        GeneratorVersion version,
        const WorldGenerationConfig& config
    );
};
```

Internally:

```text
generateParameters()
generateGeography()
generateWater()
generateMacroZones()
generateRoads()
generateDistricts()
generateBuildings()
generateTerritories()
generateFactions()
generateNPCs()
generateHistory()
generateMarkets()
generateStory()
validate()
```

---

# 91. STAGE RESULT API

Each generation stage should return explicit results.

Example:

```cpp
RoadGenerationResult
```

rather than mutating arbitrary global state everywhere.

This makes stages:

```text
testable
replaceable
debuggable
```

---

# 92. GENERATION CONTEXT

A controlled generation context may contain:

```cpp
struct GenerationContext
{
    WorldSeed seed;
    GeneratorVersion version;
    WorldGenerationConfig config;
};
```

It must not expose one mutable global RNG.

Each stage derives its own stream.

---

# 93. CONFIGURATION

Worldgen configuration may include:

```text
world size
macro-zone range
population scale
road density
water probability
building density
```

Canonical normal game uses standard config.

Debug tools may generate:

```text
tiny worlds
dense worlds
stress-test worlds
```

using the same generator architecture.

---

# 94. DEBUG MICRO-WORLDS

Development should support smaller maps.

Example:

```text
512 × 512 m
```

or:

```text
1024 × 1024 m
```

for fast testing.

This does NOT change the canonical V1 target.

It simply makes iteration faster.

---

# 95. NO GAMEPLAY LOGIC INSIDE GENERATOR

World generator creates initial state.

It does not become the runtime simulation.

Avoid:

```text
FactionGenerator::updateEveryDay()
```

Runtime behavior belongs to:

```text
FactionSystem
```

Likewise:

```text
MarketGenerator
```

creates initial market.

```text
MarketSystem
```

evolves it.

---

# 96. SHARED RULES

Generator and runtime simulation may share rule/configuration data.

Example:

```text
FactionRules
MarketRules
TerritoryRules
```

But they must not share hidden mutable state.

---

# 97. GENERATOR / RUNTIME CONSISTENCY

Four-year history should reuse simplified versions of actual rules wherever practical.

Avoid:

```text
worldgen faction logic
```

that creates states impossible for:

```text
runtime faction logic
```

The same semantic invariants must apply.

---

# 98. FIRST IMPLEMENTATION MILESTONE

Do NOT implement the whole generator at once.

Milestone 1:

```text
Seed
↓
World bounds
↓
Macro-zones
↓
Basic road graph
↓
Districts
↓
Validation
```

No buildings yet.

No factions yet.

No NPCs yet.

---

# 99. SECOND MILESTONE

Add:

```text
building parcels
basic buildings
territories
```

Then validate.

---

# 100. THIRD MILESTONE

Add:

```text
5 factions
major NPCs
relationships
```

---

# 101. FOURTH MILESTONE

Add:

```text
four-year strategic history
market
initial instability
```

---

# 102. FIFTH MILESTONE

Add:

```text
story graph
prologue setup
initial missions
```

At that point the complete base world exists.

---

# 103. FIRST REAL TEST WORLD

Target command:

```bash
faultline_worldgen --seed 12345 --validate
```

Expected conceptual result:

```text
Generating world...

[PASS] Geography
[PASS] MacroZones
[PASS] Roads
[PASS] Districts
[PASS] Buildings
[PASS] Territories
[PASS] Factions
[PASS] NPCs
[PASS] History
[PASS] Market
[PASS] Story

WORLD READY

Seed: 12345
Hash: 91A8D2F4...
```

Running it twice must produce the same hash.

---

# 104. WORLDGEN INVARIANTS

**G1** — Same seed + generator version produces same base world.

**G2** — Generation stages use independent deterministic RNG streams.

**G3** — One stage must not consume randomness belonging to another stage.

**G4** — Gameplay-relevant procedural objects have stable persistent IDs.

**G5** — World generation must terminate.

**G6** — Regeneration/retry behavior is deterministic.

**G7** — Roads and important locations are reachable.

**G8** — Exactly five main factions exist in standard V1 worlds.

**G9** — All factions begin viable.

**G10** — Major NPC references are valid.

**G11** — Initial sandbox state contains meaningful instability.

**G12** — Four-year history explains the post-timeskip world.

**G13** — Story derives from world state rather than forcing arbitrary world state.

**G14** — Runtime systems, not generators, evolve the world after creation.

**G15** — Procedural failures are reproducible from seed + generator version.

---

# 105. STATUS

The deterministic world-generation architecture is now defined.

The next required specification is:

```text
FACTION SIMULATION SPECIFICATION
```

because faction behavior is the first major runtime system required to make the generated city actually evolve.

It must define:

```text
faction power
resources
hierarchy
strategy
alliances
rivalries
war
collapse
dormancy
reformation
territorial pressure
dealer protection
operations
```

without turning the game into a pure strategy simulator.