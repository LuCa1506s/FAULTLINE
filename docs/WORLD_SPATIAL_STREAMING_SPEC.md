# WORLD STATE + SPATIAL / STREAMING SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**Parent specs:** `ENGINE_ARCHITECTURE_SPEC`, `ENGINE_CORE_RUNTIME_SPEC`, `ENTITY_DOMAIN_DATA_MODEL_SPEC`  
**Purpose:** Define world coordinates, spatial partitioning, streaming, simulation levels and transitions between physical and abstract world state.

---

# 1. CORE PRINCIPLE

The world is much larger than the physically active simulation.

Therefore:

```text
WORLD EXISTENCE
≠
PHYSICAL EXISTENCE
```

The entire city exists semantically.

Only a subset exists as fully instantiated physical entities at any moment.

---

# 2. THREE SEPARATE CONCEPTS

The engine must not confuse:

```text
WORLD COORDINATES
```

with:

```text
SPATIAL PARTITION
```

with:

```text
SIMULATION LEVEL
```

They solve different problems.

### World coordinates

Where something is.

### Spatial partition

How we efficiently find things.

### Simulation level

How much detail is currently being simulated.

---

# 3. WORLD SIZE

Initial target:

```text
8 km × 8 km
```

but the architecture must remain parameterized.

World metadata:

```cpp
struct WorldDimensions
{
    double widthMeters;
    double depthMeters;
};
```

The architecture must support future worlds larger than 8×8 km without redesigning gameplay systems.

---

# 4. WORLD COORDINATE MODEL

Do not rely permanently on one enormous global `Vec3<float>`.

Instead use:

```text
CELL COORDINATE
+
LOCAL POSITION
```

Conceptually:

```cpp
struct CellCoord
{
    int32_t x;
    int32_t z;
};

struct LocalPosition
{
    float x;
    float y;
    float z;
};

struct WorldPosition
{
    CellCoord cell;
    LocalPosition local;
};
```

This gives us stable precision regardless of future world size.

---

# 5. DEFAULT SPATIAL CELL SIZE

Initial baseline:

```text
128 m × 128 m
```

per spatial cell.

This value is configuration, not an architectural constant.

Example:

```cpp
constexpr float DEFAULT_CELL_SIZE = 128.0f;
```

should eventually come from world configuration rather than being scattered throughout gameplay code.

---

# 6. WHY 128 METERS

128 m is small enough for:

- local spatial queries;
- nearby entity indexing;
- streaming granularity;
- traffic activation;
- witness searches;
- combat-area activation.

But large enough that an 8×8 km city does not require an absurd number of cells.

Approximate cell count:

```text
8000 / 128 ≈ 63 cells per axis
```

or about:

```text
~4000 spatial cells
```

for the complete map.

That is perfectly manageable.

World boundaries do not need to align exactly with full cell dimensions.

Edge cells may be partial.

---

# 7. LOCAL POSITION NORMALIZATION

Normally:

```text
0 <= local.x < CellSize
0 <= local.z < CellSize
```

When movement crosses a cell boundary:

```text
local.x >= CellSize
```

becomes:

```text
cell.x += 1
local.x -= CellSize
```

Likewise for negative movement.

This normalization occurs inside spatial/world-coordinate utilities.

Gameplay systems should not manually manipulate cell boundaries.

---

# 8. WORLD POSITION CONVERSION

Utilities provide temporary global coordinates when required.

Conceptually:

```cpp
DoubleVec3 toGlobal(WorldPosition p);
WorldPosition fromGlobal(DoubleVec3 p);
```

Global conversion may use double precision.

The authoritative representation remains cell + local position.

---

# 9. RENDER COORDINATES

The renderer should use coordinates relative to the camera.

Example:

```text
Entity world position
-
Camera world position
=
Render-relative position
```

This prevents rendering precision from depending on absolute world coordinates.

The ASCII renderer therefore does not need to know anything about world-origin precision problems.

---

# 10. SPATIAL GRID

The world maintains a spatial index:

```text
SpatialGrid
│
├── Cell (-2, 4)
├── Cell (-1, 4)
├── Cell ( 0, 4)
├── ...
```

Each cell tracks active spatial entities.

Conceptually:

```cpp
struct SpatialCell
{
    std::vector<EntityId> entities;
};
```

This is derived runtime data.

It is not the authoritative source of entity positions.

---

# 11. AUTHORITATIVE POSITION

Authoritative physical position:

```text
Transform / SpatialTransform component
```

The spatial grid is only an index.

Therefore:

```text
Transform
→ truth

SpatialGrid
→ cache/index
```

If the grid is lost, it can be rebuilt.

---

# 12. SPATIAL QUERY API

Gameplay systems should query the world through a dedicated interface.

Examples:

```cpp
queryRadius(position, 20.0f);
queryAABB(bounds);
queryCell(cell);
queryNearbyCharacters(position, radius);
queryNearbyVehicles(position, radius);
```

Systems should not manually iterate through every entity in the world.

---

# 13. FILTERED QUERIES

Spatial queries may include filters.

Conceptually:

```cpp
SpatialQuery query;

query.center = position;
query.radius = 30.0f;
query.require(CharacterBody);
```

or equivalent APIs.

Example use cases:

```text
WitnessSystem
→ characters within hearing/vision range

CombatSystem
→ possible targets

InteractionSystem
→ interactable objects nearby

PoliceSystem
→ nearby responding police units
```

---

# 14. DOMAIN GEOGRAPHY HIERARCHY

Semantic geography follows:

```text
WORLD
│
├── MacroZone
│   │
│   ├── District
│   │   │
│   │   ├── Territory
│   │   │
│   │   └── Buildings
│   │
│   └── District
│
└── MacroZone
```

Spatial cells are NOT part of this semantic hierarchy.

A cell may overlap:

```text
one territory
multiple territories
roads
buildings
district borders
```

Cells exist for engine efficiency.

Districts and territories exist for gameplay meaning.

---

# 15. DO NOT MAKE CHUNKS GAMEPLAY OBJECTS

Forbidden design:

```text
Faction controls Chunk(21, 32)
```

unless chunk happens to be an implementation detail of a territory query.

Correct:

```text
Faction controls Territory #17
```

Chunk size may change in future.

Gameplay must not break because streaming granularity changes.

---

# 16. STREAMING CENTER

V1 uses the player as primary streaming focus.

Conceptually:

```text
StreamingFocus
=
player physical position
```

Future architecture may support several focuses:

```text
player
remote camera
important simulation area
future multiplayer player
```

but V1 only requires the player.

---

# 17. SIMULATION LEVELS

Each spatial region receives one of four simulation levels:

```text
ACTIVE
SIMULATED
ABSTRACT
DORMANT
```

These levels apply to regions and influence the representation of objects inside them.

---

# 18. ACTIVE REGION

`ACTIVE` means full local gameplay.

Includes:

```text
physical NPC entities
vehicles
movement
combat
collision
AI movement
weapons
witness perception
police behavior
doors
interactions
local traffic
```

This is the most expensive level.

---

# 19. ACTIVE RADIUS

Initial target:

```text
~512 m around player
```

Equivalent to roughly:

```text
4 cells
```

with 128 m cells.

This is not a permanent hardcoded gameplay value.

It is a performance baseline.

---

# 20. SPEED-ADAPTIVE PREFETCH

Vehicles can move quickly.

Therefore streaming must look ahead.

Example:

```text
player velocity
↓
predicted future position
↓
prefetch cells ahead
```

At high vehicle speed:

```text
active/prefetch area extends farther in direction of travel
```

This avoids entering an empty street before NPCs/traffic have activated.

---

# 21. SIMULATED REGION

`SIMULATED` exists outside full physical gameplay.

Initial target:

```text
approximately 512–1500 m
```

from the player.

Objects here may use simplified simulation.

Examples:

```text
NPC location movement between semantic points
traffic route progress
police movement
dealer activity
nearby faction activity
scheduled events
```

Full physics is unnecessary.

---

# 22. ABSTRACT REGION

Most of the city exists in `ABSTRACT`.

Instead of simulating individual footsteps:

```text
Dealer #16 working
Gang members present: 7
Faction pressure: 0.62
Police pressure: 0.21
```

Systems update semantic outcomes.

Example:

```text
Faction A sends 4 members to Territory #8

travel time calculated

later:

Faction presence Territory #8 += appropriate amount
```

No four physical NPCs need to walk through 3 km of streets.

---

# 23. DORMANT REGION

`DORMANT` means no periodic detailed updates are currently necessary.

The region still exists.

Its semantic state remains stored.

Updates occur when:

```text
scheduled event fires
strategic simulation affects area
player approaches
mission references area
```

Dormant does NOT mean deleted.

---

# 24. INITIAL DISTANCE BANDS

Development baseline:

```text
0–512 m
ACTIVE

512–1536 m
SIMULATED

1536 m+
ABSTRACT / DORMANT
```

These numbers are targets for testing, not immutable rules.

Profiling may change them substantially.

Architecture must not depend on their exact values.

---

# 25. HYSTERESIS

Streaming boundaries must not constantly oscillate.

Example problem:

```text
player at 511.9 m
→ ACTIVE

player at 512.1 m
→ SIMULATED

player at 511.9 m
→ ACTIVE
```

This is unacceptable.

Use hysteresis.

Example:

```text
activate below 500 m
deactivate above 600 m
```

Exact values are tunable.

---

# 26. REGION STATE TRANSITION

Allowed transitions:

```text
DORMANT
   ↕
ABSTRACT
   ↕
SIMULATED
   ↕
ACTIVE
```

Normally avoid jumping:

```text
DORMANT
→ ACTIVE
```

without preparing required state.

However teleport/load operations may explicitly perform fast transitions.

---

# 27. ACTIVATION PIPELINE

When a region becomes ACTIVE:

```text
1. identify persistent objects
2. resolve abstract simulation state
3. determine required physical entities
4. instantiate entities
5. attach components
6. initialize AI/runtime caches
7. populate spatial index
8. mark region ACTIVE
```

The player should never observe half-activated state.

---

# 28. DEACTIVATION PIPELINE

When an ACTIVE region becomes SIMULATED:

```text
1. stop creation of new local-only actions
2. finish/resolve required immediate interactions
3. capture persistent object states
4. convert physical state to semantic state
5. destroy unnecessary spatial entities
6. remove transient caches
7. preserve persistent references
8. mark region SIMULATED
```

---

# 29. ABSTRACTIFICATION

Moving from detailed simulation to abstract simulation is called:

```text
ABSTRACTIFICATION
```

Example:

Physical:

```text
7 gang members
specific positions
specific velocities
weapon animations
AI paths
```

becomes:

```text
FactionPresence:
members = 7
combatPower = X
location = Territory #12
currentOperation = DEFEND
```

The exact abstraction depends on object importance.

---

# 30. MATERIALIZATION

The reverse process:

```text
ABSTRACT STATE
→ PHYSICAL STATE
```

is called:

```text
MATERIALIZATION
```

Example:

```text
Territory #12
Faction A active members = 7
```

may materialize as seven appropriate NPC entities when the player arrives.

---

# 31. MATERIALIZATION MUST PRESERVE IMPORTANT IDENTITY

For generic members:

```text
abstract count
→ generated physical representatives
```

may be acceptable.

For persistent named NPCs:

```text
NpcId #17
```

must materialize as that exact NPC.

Important identities cannot be replaced by random equivalents.

---

# 32. IMPORTANT VS AGGREGATE ACTORS

Abstract simulation distinguishes:

```text
PERSISTENT ACTOR
```

and:

```text
AGGREGATE POPULATION
```

Example:

```text
Boss Moretti
→ persistent actor
```

versus:

```text
5 generic faction soldiers
→ aggregate possible
```

This dramatically reduces world-state cost.

---

# 33. GENERIC POPULATION

A district may store population aggregates such as:

```text
civilian population
traffic density
police availability
generic gang presence
```

rather than persistent individuals.

When activated:

```text
population model
+
seed
+
current context
→ local ephemeral NPCs
```

---

# 34. EPHEMERAL GENERATION

Ephemeral populations must still be deterministic enough for reproducibility.

Inputs may include:

```text
WorldSeed
CellCoord
GameTime bucket
PopulationState
ActivationCounter
```

This allows repeatable but evolving local populations.

Exact mechanism will be defined later.

---

# 35. DO NOT STORE EVERY PEDESTRIAN

We explicitly reject:

```text
every pedestrian ever seen
=
permanent NpcState
```

That would create unnecessary state growth.

Only meaningful individuals are promoted to persistence.

---

# 36. PROMOTION DURING STREAMING

If an ephemeral actor becomes important:

```text
witness
mission participant
player victim with consequences
recruited NPC
personal rival
```

then before the region unloads:

```text
ephemeral entity
↓
persistent NpcId allocated
↓
NpcState created
↓
semantic state preserved
```

---

# 37. PLAYER-CAUSED PERSISTENCE

Player actions may permanently alter otherwise procedural locations.

Examples:

```text
vehicle abandoned
important item dropped
door destroyed
building ownership changed
NPC killed
crime scene created
```

These become world deltas.

Streaming cannot regenerate the original state over them.

---

# 38. BASE WORLD VS WORLD DELTAS

Conceptually:

```text
BASE WORLD(seed)
+
RUNTIME DELTAS
=
CURRENT WORLD
```

Example:

```text
Seed-generated building #281
Door originally intact

RuntimeDelta:
Door #4 destroyed
```

On reload:

```text
generate building
↓
apply delta
↓
door remains destroyed
```

---

# 39. DIRTY STATE

Persistent objects changed from their generated baseline may be marked dirty.

Example:

```text
Building #281
dirty = true
```

or persistence may record changes at a more granular level.

Exact save representation comes later.

The spatial architecture only requires that runtime changes survive unloading.

---

# 40. WORLD REGION DATA

A streaming region should contain metadata, not gameplay authority.

Conceptually:

```cpp
struct RegionRuntime
{
    RegionCoord coord;

    SimulationLevel level;

    RegionRuntimeFlags flags;

    std::vector<EntityId> activeEntities;
};
```

Persistent semantic data remains in domain stores.

---

# 41. REGION VS CELL

Do not assume streaming region must equal spatial cell.

Initial implementation may use:

```text
1 region = 1 spatial cell
```

for simplicity.

But architecture allows:

```text
StreamingRegion
=
multiple spatial cells
```

later.

This is important for optimization.

---

# 42. WORLD STREAMING MANAGER

Dedicated system:

```text
WorldStreamingSystem
```

Responsibilities:

```text
determine desired simulation level
schedule activation
schedule deactivation
materialize persistent actors
abstract physical actors
manage prefetch
```

It does NOT decide faction strategy or mission consequences.

---

# 43. STREAMING BUDGET

Do not activate 500 entities in one frame if avoidable.

Streaming work should support budgets.

Example:

```text
max entity instantiations per tick
max region activation work per tick
max geometry preparation work
```

Critical player-visible regions receive priority.

---

# 44. PRIORITIES

Possible streaming priority:

```text
1. player's immediate cell
2. cells directly ahead
3. adjacent cells
4. mission-critical nearby areas
5. simulated ring
6. background prefetch
```

---

# 45. TELEPORTATION

Teleportation is a special operation.

Pipeline:

```text
freeze normal simulation transition
↓
resolve current region
↓
change player semantic position
↓
prepare destination
↓
materialize required regions
↓
place player
↓
resume simulation
```

The player should not appear in an uninitialized region.

---

# 46. FAST TRAVEL

If later introduced, fast travel should use the same world-time advancement architecture as sleep/arrest.

It must not simply move coordinates.

World time and distant simulation may advance during travel.

---

# 47. DEATH / ARREST RELOCATION

Death and arrest may move the player significantly.

They use the same relocation pipeline:

```text
advance world simulation
↓
determine destination
↓
prepare destination regions
↓
materialize
↓
resume
```

---

# 48. DISTRICT LOOKUP

The engine must efficiently answer:

```text
which district contains this position?
```

and:

```text
which territory contains this position?
```

These should use spatial acceleration structures.

Do not perform naive polygon tests against every district every tick.

---

# 49. TERRITORY BORDER PRECISION

Territories are semantic polygons/areas.

Spatial cells may cross territory borders.

Therefore an NPC inside one cell may belong spatially to Territory A while another NPC in the same cell belongs to Territory B.

Cell membership never determines territory ownership.

---

# 50. ROADS

Roads are persistent world-generation structures.

Conceptually represented as a graph:

```text
RoadGraph
│
├── RoadNode
└── RoadEdge
```

plus geometry.

Road graph supports:

```text
vehicle navigation
traffic
route planning
district connectivity
police routing
mission routes
```

It is not derived from physical car entities.

---

# 51. PEDESTRIAN NAVIGATION

Pedestrian navigation should also derive from generated world geometry.

Possible future representations:

```text
navigation graph
navigation mesh
street/sidewalk graph
```

Architecture does not require final choice yet.

However AI navigation data must be derived/rebuildable, not authoritative world state.

---

# 52. PATHFINDING

Pathfinding may occur in layers.

Nearby:

```text
detailed local path
```

Distant:

```text
road/district graph route
```

Example:

```text
NPC travels 4 km
```

should not require maintaining thousands of physical movement ticks.

Use:

```text
semantic route
+
travel duration
```

until physically relevant.

---

# 53. TRAVEL STATE

Persistent actor outside active simulation may use:

```cpp
struct TravelState
{
    WorldLocation origin;
    WorldLocation destination;

    GameTime departure;
    GameTime arrival;
};
```

When the player encounters the actor mid-route, the engine can materialize them at an appropriate derived position.

---

# 54. LONG-DISTANCE VEHICLES

The same applies to persistent vehicles.

Abstract:

```text
Vehicle #14
travelling District 2 → District 5
arrival 18:42 game time
```

Physical car simulation is unnecessary until relevant.

---

# 55. TRAFFIC MODEL

Traffic outside ACTIVE regions should primarily be statistical.

District/cell state may contain:

```text
traffic density
vehicle archetype distribution
congestion
police traffic
```

Activation creates appropriate ephemeral vehicles.

We must not simulate every civilian car across 64 km².

---

# 56. POLICE STREAMING

Police requires special handling because distant units may become gameplay-relevant.

Example:

```text
crime occurs
↓
PoliceSystem selects response resources
↓
response route begins abstractly
↓
unit approaches SIMULATED range
↓
vehicle/NPCs materialize
↓
physical chase begins
```

This creates the appearance of a continuous world without simulating the entire journey physically.

---

# 57. MISSION STREAMING

Mission-important actors/objects may force minimum simulation levels.

Example:

```text
critical ambush prepared 700m ahead
```

may request:

```text
SIMULATED/PREFETCH
```

before normal distance streaming would.

Mission system requests relevance.

Streaming system decides implementation.

---

# 58. RELEVANCE REQUESTS

Systems may submit:

```text
SimulationRelevanceRequest
```

Example:

```text
Mission #14
requests Region X >= SIMULATED
for next 30 game seconds
```

Streaming remains centralized.

Mission code must not directly spawn arbitrary distant cells.

---

# 59. PINNED REGIONS

Some regions may temporarily be:

```text
PINNED
```

Meaning they cannot drop below a required simulation level.

Examples:

```text
active mission
major police event
story transition
player-owned ongoing operation
```

Pinning must be used sparingly.

---

# 60. MEMORY BUDGET AWARENESS

Streaming exists primarily to control:

```text
CPU
memory
AI workload
physics workload
rendering workload
```

Therefore each region should eventually expose approximate costs.

Example:

```text
active NPC count
active vehicles
interactive objects
AI jobs
```

This enables adaptive streaming later.

---

# 61. DISTANCE IS NOT THE ONLY RELEVANCE FACTOR

Region simulation level may depend on:

```text
distance
player speed
camera direction
mission relevance
police relevance
story importance
recent interaction
performance budget
```

Distance remains the dominant V1 factor.

---

# 62. WORLD STATE UPDATE ORDER

Streaming changes occur at controlled tick boundaries.

Recommended:

```text
tick begins
↓
evaluate desired streaming state
↓
prepare activation/deactivation commands
↓
run gameplay simulation
↓
commit safe structural changes
↓
finalize streaming transitions
```

Exact ordering may be tuned.

But region transitions must not happen unpredictably in the middle of arbitrary system iteration.

---

# 63. SAVE DURING STREAMING

Save must represent semantic truth.

Before serialization:

```text
active physical persistent actors
↓
synchronize semantic state
```

The save system must not rely on an NPC remaining loaded.

---

# 64. SAVE INVARIANT

Saving in:

```text
ACTIVE
```

versus:

```text
ABSTRACT
```

should not change the logical persistent identity of an object.

Example:

```text
Npc #44 alive in active area
```

must save equivalently to the same NPC being abstractly simulated elsewhere.

---

# 65. DEBUG VISUALIZATION

Development builds should expose:

```text
cell boundaries
active radius
simulated radius
region state
entity counts
streaming transitions
```

Example:

```text
Cell (18,21)
Level: ACTIVE
Entities: 87
Persistent NPCs: 12
Ephemeral NPCs: 23
Vehicles: 31
```

This will be essential for optimization.

---

# 66. HEADLESS STREAMING TEST

Streaming must work without renderer.

Example test:

```text
spawn player at (0,0)
↓
activate regions
↓
move player 2000m
↓
deactivate old regions
↓
activate new regions
↓
validate persistent actors
```

Renderer is irrelevant.

---

# 67. CONTINUITY TEST

Example:

```text
Npc #42
position A
health 67
weapon X
```

player leaves area.

NPC abstractifies.

Player returns later.

Requirement:

```text
Npc #42 still exists
health consistent
weapon consistent
world history consistent
```

unless abstract simulation changed them.

---

# 68. ABSTRACT DEATH TEST

Example:

```text
Npc #81
exists in ABSTRACT territory
```

A valid faction operation kills them.

Then:

```text
NpcStatus = DEAD
```

When player later visits:

```text
Npc #81 must NOT materialize alive
```

Physical streaming obeys semantic reality.

---

# 69. PHYSICAL DEATH TEST

Likewise:

```text
player kills Npc #81 physically
↓
NpcState = DEAD
↓
area unloads
↓
area reloads
```

NPC must remain dead.

Procedural generation must never recreate them.

---

# 70. STREAMING FAILURE POLICY

If materialization finds impossible state:

```text
persistent NPC says ALIVE
but references invalid faction
```

development build must report:

```text
seed
tick
NpcId
region
validation failure
```

Do not silently replace the NPC.

---

# 71. INITIAL WORLD RUNTIME STRUCTURE

Conceptually:

```text
WorldState
│
├── Geography
│   ├── MacroZones
│   ├── Districts
│   ├── Territories
│   ├── Buildings
│   └── RoadGraph
│
├── SemanticActors
│   ├── NPCs
│   ├── Vehicles
│   └── Items
│
├── Organizations
│   └── Factions
│
├── Economy
│   └── Markets
│
└── RuntimeSpatial
    ├── EntityRegistry
    ├── SpatialGrid
    └── StreamingRegions
```

Some runtime spatial state may ultimately live outside serialized `WorldState`.

Exact ownership can be refined during implementation.

---

# 72. WORLD DATA CLASSIFICATION

### Generated immutable/base

```text
road graph
building baseline
district geometry
territory baseline geometry
macro-zones
```

### Persistent runtime

```text
destroyed objects
NPC life state
vehicle state
faction state
territorial changes
market changes
ownership
```

### Transient

```text
AI paths
physics contacts
render visibility
streaming queues
```

### Derived

```text
spatial index
cached faction member lookup
navigation caches
render batches
```

---

# 73. PERFORMANCE RULE

Never optimize by corrupting semantic continuity.

If performance is insufficient:

```text
reduce physical population
reduce active radius
lower AI frequency
increase abstraction
```

Do NOT:

```text
randomly delete persistent actors
skip important consequences
reset distant regions
```

The systemic world is the feature we are building.

---

# 74. FIRST IMPLEMENTATION VERSION

The first spatial implementation does NOT need:

```text
advanced adaptive budgets
multithreaded streaming
GPU culling
navigation mesh streaming
complex interior streaming
```

Initial version can use:

```text
128m spatial cells

player-centered distance rings

ACTIVE / ABSTRACT only initially

simple entity activation

basic radius queries
```

Then introduce `SIMULATED` as systems require it.

This avoids overengineering before world generation works.

---

# 75. IMPORTANT IMPLEMENTATION DISTINCTION

Architectural model:

```text
ACTIVE
SIMULATED
ABSTRACT
DORMANT
```

Implementation milestone 1 may support only:

```text
ACTIVE
ABSTRACT
```

The interfaces must still anticipate all four.

This reduces initial code without sacrificing the architecture.

---

# 76. V1 BASELINE VALUES

Initial development configuration:

```text
Spatial cell size:
128 m

Full ACTIVE radius:
~512 m

SIMULATED target radius:
~1536 m

Beyond:
ABSTRACT / DORMANT

Streaming focus:
Player

World units:
Meters

World position:
CellCoord + local float position
```

All distance thresholds are configuration values.

---

# 77. ARCHITECTURAL INVARIANTS

**W1** — Entire city exists semantically even when not physically loaded.

**W2** — Spatial cells are engine structures, not gameplay territories.

**W3** — Semantic geography is independent from streaming granularity.

**W4** — Transform is authoritative physical position.

**W5** — Spatial indexes are derived and rebuildable.

**W6** — Persistent actors preserve identity across streaming.

**W7** — Ephemeral population does not require permanent identity.

**W8** — Important actors may exist without active physical entities.

**W9** — Abstract simulation can cause real persistent consequences.

**W10** — Materialization must respect abstract state.

**W11** — Player-caused persistent changes survive region unloading.

**W12** — Region transitions occur at controlled synchronization points.

**W13** — Streaming behavior is not allowed to determine gameplay outcomes arbitrarily.

**W14** — Distance thresholds are configuration, not core assumptions.

**W15** — Large-scale simulation must prefer abstraction over brute-force physical simulation.

---

# 78. SUCCESS CRITERION

The spatial architecture is considered functional when this scenario works:

```text
create world
↓
spawn persistent NPC #42 3 km away
↓
NPC exists abstractly
↓
player drives toward area
↓
region becomes simulated
↓
NPC materializes
↓
player interacts with NPC
↓
NPC health/location changes
↓
player drives away
↓
NPC abstractifies
↓
world continues simulating
↓
player returns
↓
same NpcId materializes
↓
state remains coherent
```

Without renderer involvement.

---

# 79. STATUS

The architecture now defines:

```text
runtime ownership
entities/domain objects
persistent identity
world coordinates
spatial indexing
streaming
simulation fidelity
materialization
abstractification
```

The next major technical specification is:

```text
WORLD GENERATION SPECIFICATION
```

which must define the deterministic pipeline:

```text
Seed
↓
Geography
↓
Water
↓
Macro-zones
↓
Road network
↓
Districts
↓
Buildings
↓
Territories
↓
Factions
↓
Important NPCs
↓
Four-year history
↓
Market
↓
Story graph
↓
Initial sandbox state
```