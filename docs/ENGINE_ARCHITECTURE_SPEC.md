# ENGINE ARCHITECTURE SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**Status:** Technical baseline draft  
**Purpose:** Define the structural architecture of the game engine before implementation.

---

# 1. ARCHITECTURAL GOALS

The engine shall prioritize:

1. deterministic simulation;
2. reproducible procedural generation;
3. complete separation between simulation and presentation;
4. long-term maintainability;
5. testability without launching the game renderer;
6. persistent worlds based on seed + runtime changes;
7. support for large simulated worlds without fully simulating every entity;
8. future renderer replacement without rewriting gameplay;
9. future extension without requiring multiplayer support in V1.

The engine is not intended to be a general-purpose Unreal/Unity competitor.

It is a purpose-built engine for this game.

---

# 2. LANGUAGE AND BUILD SYSTEM

## 2.1 Core language

**C++20**

Reasons:

- native performance;
- deterministic control over data structures and memory;
- mature tooling;
- suitable for custom simulation and rendering systems;
- easy integration with native libraries;
- no garbage collector affecting simulation timing;
- good portability.

C++23 features may be introduced later if they provide a concrete advantage, but C++20 is the compatibility baseline.

## 2.2 Build system

**CMake**

The source tree must be composed of separate targets/libraries rather than one monolithic executable.

Example:

```text
faultline_core
faultline_world
faultline_simulation
faultline_gameplay
faultline_persistence
faultline_platform
faultline_renderer_ascii
faultline_app
faultline_tests
faultline_tools
```

---

# 3. FUNDAMENTAL DEPENDENCY RULE

Dependencies flow inward.

```text
                        ┌──────────────┐
                        │     App      │
                        └──────┬───────┘
                               │
             ┌─────────────────┼─────────────────┐
             ↓                 ↓                 ↓
         Platform          Renderer          Gameplay
                                                 │
                                                 ↓
                                           Simulation
                                                 │
                         ┌───────────────────────┼───────────┐
                         ↓                       ↓           ↓
                       World                Persistence    Core
                         │                       │
                         └───────────→ Core ←────┘
```

The critical rule is:

```text
CORE KNOWS NOTHING ABOUT SDL
CORE KNOWS NOTHING ABOUT ANSI
CORE KNOWS NOTHING ABOUT WINDOWS
CORE KNOWS NOTHING ABOUT CONTROLLERS
CORE KNOWS NOTHING ABOUT THE TERMINAL
```

Renderer and platform code may depend on the core.

The core must never depend on renderer or platform implementations.

---

# 4. TOP-LEVEL MODULES

```text
src/
│
├── core/
│
├── world/
│
├── simulation/
│
├── gameplay/
│
├── persistence/
│
├── platform/
│
├── renderer/
│   └── ascii/
│
└── app/
```

Additional non-runtime components:

```text
tests/
tools/
assets/
data/
docs/
```

---

# 5. CORE MODULE

`core/` contains infrastructure that is not specific to factions, weapons, missions or rendering.

Initial components:

```text
Core
├── IDs
├── RNG
├── Time
├── Math
├── Events
├── Serialization primitives
├── Diagnostics
└── Assertions
```

The core should remain small.

Anything that can exist only because this specific game exists probably does not belong in `core`.

---

# 6. ID SYSTEM

Every persistent game object receives a stable typed identifier.

Examples:

```cpp
EntityId
NpcId
FactionId
TerritoryId
DistrictId
BuildingId
VehicleId
MissionId
ItemId
```

Raw integers must not be casually exchanged between unrelated ID domains.

Conceptually:

```text
NpcId(42) != VehicleId(42)
```

Persistent IDs must survive:

```text
save
→ shutdown
→ load
```

Procedurally generated persistent objects must receive reproducible IDs whenever possible.

Temporary runtime objects may use transient IDs.

---

# 7. DETERMINISTIC RANDOMNESS

There shall be **no unrestricted global random generator**.

Forbidden architecture:

```cpp
random();
rand();
Random::global();
```

Instead:

```text
WorldSeed
│
├── GeographySeed
├── RoadsSeed
├── DistrictSeed
├── BuildingsSeed
├── FactionsSeed
├── NPCSeed
├── HistorySeed
├── MarketSeed
└── StorySeed
```

Each procedural subsystem receives its own deterministic RNG stream.

Example:

```text
Seed = 4815162342

hash(seed, "geography")
→ Geography RNG

hash(seed, "factions")
→ Faction RNG

hash(seed, "npc", factionID)
→ NPC RNG
```

This prevents an unrelated change in NPC generation from changing the entire road network.

The same:

```text
WorldSeed
+
GeneratorVersion
```

must always generate the same base world.

---

# 8. SIMULATION MODEL

The authoritative game state exists only inside the simulation.

```text
INPUT
   ↓
COMMANDS
   ↓
SIMULATION
   ↓
WORLD STATE
   ↓
SNAPSHOT / VIEW
   ↓
RENDERER
```

The renderer may observe state.

The renderer may never change gameplay state directly.

Wrong:

```text
Renderer → NPC.health -= 10
```

Correct:

```text
Input
→ ShootCommand
→ WeaponSystem
→ CombatSystem
→ Damage
→ WorldState
→ Renderer observes result
```

---

# 9. FIXED SIMULATION CLOCK

Gameplay simulation shall use a fixed timestep.

Initial baseline:

```text
Simulation tick = 20 Hz
Delta = 50 ms
```

Rendering may run independently:

```text
simulation: 20 Hz
rendering: 30 / 60 / unlocked
```

The exact simulation frequency may be changed after profiling, but gameplay logic must never depend directly on variable frame duration.

Conceptually:

```cpp
while (accumulator >= fixedStep)
{
    simulation.tick(fixedStep);
    accumulator -= fixedStep;
}

renderer.draw(interpolation);
```

This provides:

- deterministic behavior;
- reproducible tests;
- easier debugging;
- stable gameplay;
- independence from rendering speed.

---

# 10. MULTI-RATE SIMULATION

Not every subsystem needs to run every simulation tick.

Example baseline:

```text
Movement / combat
20 Hz

Nearby NPC decisions
5–10 Hz

Traffic decisions
5 Hz

Territory evaluation
1 Hz

Faction strategy
game-time intervals

Market updates
game-time intervals

Long-distance world simulation
coarse strategic intervals
```

Scheduling belongs to the simulation layer.

Individual systems must not each invent their own wall-clock timers.

---

# 11. WORLD SIMULATION LEVELS

The entire 8×8 km city must not be physically simulated at full fidelity.

World regions may exist in different simulation states:

```text
ACTIVE
SIMULATED
ABSTRACT
DORMANT
```

### ACTIVE

Near the player.

Full gameplay representation:

- NPC movement;
- vehicles;
- combat;
- collisions;
- witnesses;
- police;
- interactive objects.

### SIMULATED

Near enough to matter but outside immediate gameplay.

Reduced simulation.

### ABSTRACT

Distant district.

Only semantic state is simulated:

```text
Faction A presence: 0.73
Faction B presence: 0.21
Dealer count: 5
Police pressure: 0.37
Market demand: 0.64
```

### DORMANT

Areas requiring no active calculation until something relevant occurs.

The world simulation must produce coherent transitions between these levels.

---

# 12. ENTITY ARCHITECTURE

V1 will use a **custom composition-based Entity/Component model**.

It will NOT begin with:

- deep inheritance hierarchies;
- a fully generic commercial-engine-style ECS;
- dozens of virtual base classes.

An entity is primarily:

```text
EntityId
+
set of components
```

Example NPC:

```text
Entity #124

Transform
Health
NpcIdentity
FactionMembership
RelationshipState
Inventory
CombatState
MissionRole
```

Example vehicle:

```text
Entity #831

Transform
VehicleBody
Health
Ownership
PhysicsState
FuelState
```

Systems operate on relevant data.

This avoids structures such as:

```text
Entity
 └── Person
      └── Criminal
           └── GangMember
                └── Dealer
                     └── MafiaDealer
```

which would become fragile as procedural roles overlap.

---

# 13. SEMANTIC OBJECTS VS PHYSICAL ENTITIES

Not everything in the game should be an Entity.

Example:

```text
Faction
Territory
Mission
Relationship graph
Market
World history
District
```

are semantic simulation objects.

A faction is NOT an entity walking around the map.

A territory is NOT a physical entity.

The engine will distinguish between:

```text
Spatial Entities
```

and:

```text
Simulation Domain Objects
```

This prevents forcing every concept into one ECS.

---

# 14. SYSTEM ARCHITECTURE

Expected gameplay/simulation systems include:

```text
MovementSystem
CombatSystem
WeaponSystem
VehicleSystem
InteractionSystem

NPCSystem
RelationshipSystem

FactionSystem
TerritorySystem
MarketSystem
DealerSystem

MissionSystem
StoryGraphSystem

WitnessSystem
PoliceSystem
WantedSystem

RespectSystem
ProgressionSystem
```

A system should own rules, not arbitrary copies of shared state.

The authoritative state belongs to explicit domain stores/world state.

---

# 15. EVENT ARCHITECTURE

The engine may use domain events, but NOT a universal event bus for every interaction.

Good events:

```text
NpcKilled
TerritoryControlChanged
MissionCompleted
DealerRemoved
FactionCollapsed
PlayerArrested
WitnessReportedCrime
```

Bad architecture:

```text
everything
→ GlobalEventBus
→ unknown listeners everywhere
```

Critical gameplay flows should remain traceable.

Prefer:

```text
command
→ system
→ state change
→ optional domain event
```

instead of hidden event chains.

---

# 16. COMMAND MODEL

External intent enters the simulation as commands.

Examples:

```text
MoveCommand
AimCommand
ShootCommand
InteractCommand
EnterVehicleCommand
DialogueChoiceCommand
FactionOrderCommand
```

Input adapters transform devices into abstract actions.

Example:

```text
Keyboard W
           \
Controller LS → MoveForward
           /
future input
```

The gameplay layer receives `MoveForward`, not `SDL_SCANCODE_W`.

---

# 17. PLATFORM LAYER

Platform-specific functionality belongs in:

```text
platform/
```

Responsibilities may include:

- keyboard input;
- controller input;
- terminal configuration;
- filesystem access;
- high-resolution timing;
- process/platform utilities.

For V1, SDL3 may be used as a platform/input backend where useful, especially for standardized controller handling.

SDL types must not leak into game logic.

Bad:

```cpp
void Player::update(const SDL_Event&);
```

Good:

```cpp
void PlayerController::apply(const PlayerCommand&);
```

---

# 18. RENDERER INTERFACE

The renderer consumes a presentation-safe view of the world.

Conceptually:

```cpp
struct RenderSnapshot;
```

The simulation produces:

```text
RenderSnapshot
```

The ASCII renderer consumes:

```text
RenderSnapshot
```

Future renderer:

```text
RenderSnapshot
→ graphical renderer
```

No gameplay code should need modification.

---

# 19. ASCII RENDERER

Initial renderer:

```text
renderer/ascii/
```

Responsibilities:

- camera;
- projection;
- visibility;
- character selection;
- color;
- depth;
- HUD composition;
- terminal output.

It does NOT decide:

- damage;
- collision outcome;
- faction ownership;
- NPC behavior;
- mission resolution;
- police logic.

---

# 20. WORLD STATE

There shall be one authoritative simulation root.

Conceptually:

```cpp
GameState
{
    WorldState world;
    PlayerState player;
    SimulationState simulation;
    MissionState missions;
    StoryState story;
}
```

`WorldState` may contain or reference:

```text
EntityRegistry
Districts
Territories
Factions
NPC registry
Markets
Vehicles
Buildings
Relationships
```

Exact ownership will be refined in the data-model specifications.

---

# 21. PERSISTENCE BOUNDARY

Persistence is not gameplay.

Architecture:

```text
GameState
   ↓
Save Serializer
   ↓
Save Data
```

and:

```text
Save Data
   ↓
Loader / Migration
   ↓
GameState
```

Gameplay systems must not manually write files.

Forbidden:

```cpp
FactionSystem::saveToJson();
```

Preferred:

```text
FactionSystem
→ modifies FactionState

Persistence
→ serializes FactionState
```

---

# 22. SAVE MODEL

Canonical model:

```text
Base world
=
WorldSeed
+
WorldGeneratorVersion

Persistent game
=
Base world
+
Runtime state/deltas
```

The architecture must support save migration.

Every save therefore records at minimum:

```text
SaveFormatVersion
GameVersion
WorldGeneratorVersion
WorldSeed
```

---

# 23. THREADING POLICY — V1

Authoritative simulation will initially run on **one thread**.

Reason:

- deterministic execution;
- straightforward reproduction of bugs;
- no race conditions in gameplay state;
- easier testing;
- simpler saves;
- simpler event ordering.

This does NOT prohibit background work.

Future worker threads may perform tasks such as:

```text
procedural generation jobs
pathfinding calculations
asset loading
renderer preparation
```

but workers must not mutate authoritative simulation state directly.

Pattern:

```text
worker computes result
        ↓
result returned
        ↓
simulation accepts result at defined synchronization point
```

---

# 24. TESTABILITY REQUIREMENT

Every core simulation system must be runnable without:

- renderer;
- terminal;
- keyboard;
- controller;
- audio;
- graphical window.

Example:

```text
World world(seed);

simulate(world, 100000_ticks);

assert(world.valid());
```

The test executable must be able to simulate thousands of worlds automatically.

---

# 25. DIAGNOSTICS

Every runtime simulation should expose enough information to reproduce important failures.

Debug context:

```text
WorldSeed
GeneratorVersion
SimulationTick
GameTime
PlayerPosition
ActiveDistrict
LastDomainEvents
```

Example crash report:

```text
Seed: 91743381
Tick: 841029
District: SOUTH_SIDE_04
Last event: FACTION_COLLAPSED
```

This is mandatory for procedural bug reproduction.

---

# 26. SOURCE TREE — INITIAL BASELINE

```text
/
├── CMakeLists.txt
├── cmake/
├── docs/
│
├── src/
│   ├── core/
│   │   ├── id/
│   │   ├── rng/
│   │   ├── time/
│   │   ├── math/
│   │   ├── events/
│   │   └── diagnostics/
│   │
│   ├── world/
│   │   ├── state/
│   │   ├── generation/
│   │   └── spatial/
│   │
│   ├── simulation/
│   │   ├── factions/
│   │   ├── territory/
│   │   ├── market/
│   │   ├── npc/
│   │   ├── missions/
│   │   └── police/
│   │
│   ├── gameplay/
│   │   ├── player/
│   │   ├── combat/
│   │   ├── weapons/
│   │   ├── vehicles/
│   │   └── interaction/
│   │
│   ├── persistence/
│   │
│   ├── platform/
│   │
│   ├── renderer/
│   │   └── ascii/
│   │
│   └── app/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── determinism/
│   └── simulation/
│
└── tools/
    ├── worldgen/
    ├── validator/
    └── simulator/
```

---

# 27. INITIAL EXECUTABLES

The project should eventually produce at least:

```text
faultline
```

Main game.

```text
faultline_worldgen
```

Generate and inspect worlds.

```text
faultline_sim
```

Run headless simulations.

```text
faultline_tests
```

Automated test suite.

This means world generation and simulation are first-class programs, not functionality trapped inside the game executable.

---

# 28. FIRST IMPLEMENTATION LAYERS

Implementation order:

```text
PHASE A
Core primitives
│
├── typed IDs
├── deterministic RNG
├── simulation clock
├── diagnostics
└── basic tests

PHASE B
World state
│
├── entity registry
├── domain registries
└── spatial structures

PHASE C
Persistence foundation

PHASE D
World generator

PHASE E
Headless simulation

PHASE F
Gameplay systems

PHASE G
Renderer
```

The ASCII renderer intentionally does NOT come first.

The game should already be able to exist and evolve headlessly before the renderer becomes important.

---

# 29. PRIMARY ARCHITECTURAL INVARIANTS

These rules must not be violated casually.

### A1

**Simulation state is authoritative.**

### A2

**Rendering never determines gameplay.**

### A3

**Input hardware never enters gameplay logic directly.**

### A4

**Randomness must come from explicit deterministic RNG streams.**

### A5

**Persistent objects use stable identifiers.**

### A6

**Simulation uses fixed timestep.**

### A7

**Distant world state may be abstracted but must remain semantically coherent.**

### A8

**No deep gameplay inheritance hierarchy. Prefer composition.**

### A9

**Not every game concept is an ECS entity.**

### A10

**Persistence serializes state; gameplay systems do not write saves themselves.**

### A11

**Authoritative simulation is single-threaded in V1.**

### A12

**Core simulation must run headlessly.**

### A13

**A procedural failure must be reproducible from seed + state/tick whenever possible.**

---

# 30. ARCHITECTURAL SUCCESS TEST

The architecture is considered viable when the following program is possible without any renderer:

```text
create game
↓
seed = 12345
↓
generate world
↓
simulate world
↓
inspect factions/NPCs/territories
↓
save
↓
destroy process
↓
reload
↓
continue simulation
```

and:

```text
same seed
+
same generator version
=
same generated base world
```

while a completely different renderer could consume the resulting game state without changing simulation code.