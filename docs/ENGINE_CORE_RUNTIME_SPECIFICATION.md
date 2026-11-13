\# ENGINE CORE RUNTIME SPECIFICATION — v0.1



\*\*Project:\*\* Procedural Crime Sandbox  

\*\*Parent specification:\*\* `ENGINE\_ARCHITECTURE\_SPEC v0.1`  

\*\*Purpose:\*\* Define ownership, runtime execution and deterministic simulation flow.



\---



\# 1. CORE RUNTIME MODEL



The runtime is divided into four primary layers:



```text

Application / GameLoop

&#x20;       │

&#x20;       ↓

&#x20;      Game

&#x20;       │

&#x20;       ├──────────────┐

&#x20;       ↓              ↓

&#x20;   GameState      Simulation

&#x20;                      │

&#x20;                      ↓

&#x20;                   Systems

```



Their responsibilities must remain separate.



\---



\# 2. APPLICATION / GAME LOOP



`Application` is outside the authoritative simulation.



It owns platform-facing systems:



```text

Application

├── Platform

├── InputAdapter

├── Renderer

├── Game

└── FixedStepRunner

```



Responsibilities:



\- poll operating-system events;

\- receive keyboard/controller input;

\- measure real elapsed time;

\- maintain the fixed-step accumulator;

\- call simulation ticks;

\- request render snapshots;

\- present frames;

\- handle window/application shutdown.



It must NOT contain gameplay logic.



Example:



```cpp

while (app.running())

{

&#x20;   const auto realDelta = platform.pollTime();



&#x20;   input.poll();



&#x20;   runner.addTime(realDelta);



&#x20;   while (runner.shouldTick())

&#x20;   {

&#x20;       game.submitInput(input.makeFrame());

&#x20;       game.tick();



&#x20;       runner.consumeTick();

&#x20;   }



&#x20;   renderer.render(game.createRenderSnapshot());

}

```



The simulation therefore does not know how quickly the renderer is running.



\---



\# 3. GAME



`Game` is the public façade of one active game session.



Conceptually:



```cpp

class Game

{

private:

&#x20;   GameState state\_;

&#x20;   Simulation simulation\_;

&#x20;   CommandQueue commands\_;



public:

&#x20;   void tick();



&#x20;   void submit(Command command);



&#x20;   const GameState\& state() const;



&#x20;   RenderSnapshot createRenderSnapshot() const;

};

```



`Game` owns:



```text

GameState

Simulation

incoming command queue

```



`Game` does NOT own:



```text

Renderer

Keyboard

Controller

SDL window

Terminal

Filesystem UI

Operating-system state

```



Its primary job is:



```text

receive external intent

↓

prepare tick input

↓

run simulation

↓

expose resulting state

```



\---



\# 4. GAME MUST NOT BECOME A GOD OBJECT



Forbidden:



```cpp

game.shootWeapon();

game.updateFactionWar();

game.spawnPolice();

game.calculateDrugPrice();

game.moveCar();

game.completeMission();

```



Those belong to systems.



Correct:



```cpp

game.tick();

```



Internally:



```text

Simulation

├── PlayerControlSystem

├── AISystem

├── MovementSystem

├── VehicleSystem

├── CombatSystem

├── WitnessSystem

├── PoliceSystem

├── MissionSystem

├── MarketSystem

├── TerritorySystem

└── FactionSystem

```



`Game` coordinates.



Systems implement rules.



`GameState` stores reality.



\---



\# 5. GAMESTATE



`GameState` is the \*\*single authoritative state of the current world\*\*.



Conceptually:



```cpp

struct GameState

{

&#x20;   WorldMetadata metadata;



&#x20;   SimulationClock clock;

&#x20;   RuntimeRandomState randomness;



&#x20;   WorldState world;



&#x20;   PlayerState player;



&#x20;   MissionState missions;

&#x20;   StoryState story;



&#x20;   WorldAchievementState achievements;

};

```



If the game is stopped after a completed tick, the information necessary to reproduce the persistent world must originate from `GameState`.



\---



\# 6. GAMESTATE CONTAINS DATA, NOT ENGINE SERVICES



`GameState` may contain:



```text

numbers

IDs

enums

vectors

maps

component data

domain state

simulation state

```



It must NOT contain:



```text

Renderer\*

SDL\_Window\*

FileHandle

Thread

Socket

ControllerHandle

terminal handle

OS timer

```



It should also avoid arbitrary service pointers.



Bad:



```cpp

struct Faction

{

&#x20;   MissionSystem\* missions;

&#x20;   Renderer\* renderer;

&#x20;   AudioEngine\* audio;

};

```



Correct:



```cpp

struct Faction

{

&#x20;   FactionId id;

&#x20;   TerritoryId homeTerritory;

&#x20;   std::vector<NpcId> members;

&#x20;   float cohesion;

&#x20;   Money treasury;

};

```



\---



\# 7. NO POINTER GRAPH AS WORLD STATE



Persistent relationships must primarily use IDs.



Bad:



```cpp

Npc\* boss;

Faction\* faction;

Territory\* home;

Npc\* rival;

```



Preferred:



```cpp

NpcId boss;

FactionId faction;

TerritoryId home;

NpcId rival;

```



This gives us:



\- safe serialization;

\- stable references;

\- easier debugging;

\- easier world validation;

\- no pointer invalidation after container changes;

\- simple save/load reconstruction.



Transient caches may resolve IDs to objects for performance.



Those caches are never authoritative.



\---



\# 8. WORLDSTATE



`WorldState` contains the persistent semantic city.



Initial architecture:



```text

WorldState

│

├── EntityRegistry

├── SpatialState

│

├── DistrictStore

├── TerritoryStore

├── BuildingStore

│

├── NpcStore

├── VehicleStore

│

├── FactionStore

├── RelationshipState

│

├── MarketState

├── DealerState

│

└── PoliceWorldState

```



Exact component layouts are defined later by the Entity/NPC specification.



\---



\# 9. PLAYERSTATE



The player is partly a spatial entity and partly a special persistent game concept.



Therefore:



```text

PlayerState

├── EntityId controlledEntity

├── Respect

├── HistoricalMaxRespect

├── Cash

├── FactionMembership

├── OwnedVehicles

├── Home

├── progression

└── player-specific state

```



The player's physical body remains an entity.



`PlayerState` stores information belonging specifically to the player's game progression.



This avoids making every NPC contain player-only fields.



\---



\# 10. SIMULATION



`Simulation` contains the rules that transform one valid `GameState` into the next.



Conceptually:



```cpp

class Simulation

{

public:

&#x20;   void tick(

&#x20;       GameState\& state,

&#x20;       const TickInput\& input

&#x20;   );

};

```



Important distinction:



```text

GameState = WHAT EXISTS



Simulation = WHAT HAPPENS

```



`Simulation` owns behavior.



It does not own the authoritative world.



\---



\# 11. SYSTEM OWNERSHIP



`Simulation` owns or coordinates systems:



```cpp

class Simulation

{

private:

&#x20;   PlayerControlSystem playerControl\_;

&#x20;   AiSystem ai\_;



&#x20;   MovementSystem movement\_;

&#x20;   VehicleSystem vehicles\_;



&#x20;   InteractionSystem interaction\_;

&#x20;   CombatSystem combat\_;



&#x20;   WitnessSystem witnesses\_;

&#x20;   PoliceSystem police\_;



&#x20;   MissionSystem missions\_;

&#x20;   StorySystem story\_;



&#x20;   MarketSystem market\_;

&#x20;   TerritorySystem territory\_;

&#x20;   FactionSystem factions\_;

};

```



Systems should ideally be mostly:



```text

rules

algorithms

configuration

temporary working memory

```



rather than owners of persistent world data.



\---



\# 12. GAME RULES VS GAME STATE



Static balancing/configuration must not be mixed with runtime state.



Example:



```text

GameRules

├── weapon definitions

├── vehicle definitions

├── Respect formulas

├── market coefficients

├── police parameters

└── faction simulation parameters

```



versus:



```text

GameState

├── pistol currently has 7 rounds

├── player Respect = 31

├── district demand = 0.67

├── police pressure = 0.42

└── faction #4 controls territory #17

```



Rule:



> Definitions describe what \*can happen\*.  

> State describes what \*has happened / currently exists\*.



\---



\# 13. SIMULATION CLOCK



The authoritative clock belongs to `GameState`.



Example:



```cpp

struct SimulationClock

{

&#x20;   uint64\_t tick;

&#x20;   GameTime gameTime;

};

```



For the initial 20 Hz simulation:



```text

1 tick = 50 ms simulation time

```



The clock does NOT store wall-clock time.



Forbidden:



```text

18:42:31 Windows local time

```



Gameplay must never depend on system clock.



\---



\# 14. REAL TIME VS GAME TIME



These are completely different concepts.



```text

REAL TIME

```



belongs to `Application`.



It determines when another simulation tick should execute.



```text

GAME TIME

```



belongs to `GameState`.



It determines:



\- day/night;

\- market progression;

\- missions;

\- faction decisions;

\- police processes;

\- sleep;

\- death time advancement;

\- arrest;

\- strategic simulation.



Therefore:



```text

Application time ≠ World time

```



\---



\# 15. PAUSE



Pause is not a world event.



When paused:



```text

Application

→ stops requesting simulation ticks

```



The world does not need:



```cpp

if (paused)

```



scattered through every gameplay system.



This keeps pause outside authoritative world logic.



\---



\# 16. TIME SKIPS



Sleep, arrest, death and scripted time advancement are different.



They are \*\*game-time operations\*\*.



Example:



```text

PLAYER ARRESTED

↓

Arrest resolution

↓

advance world by N game hours/days

↓

coarse simulation

↓

resume active gameplay

```



A time skip must not simply change:



```cpp

clock += 3\_days;

```



without simulating consequences.



Instead:



```text

advanceWorld(duration)

```



must run the relevant abstract systems.



\---



\# 17. TICK INPUT



Every simulation tick receives a deterministic input package.



Conceptually:



```cpp

struct TickInput

{

&#x20;   uint64\_t tick;



&#x20;   PlayerInputFrame playerInput;



&#x20;   std::vector<GameCommand> commands;

};

```



`TickInput` represents external intent for exactly one tick.



\---



\# 18. PLAYER INPUT FRAME



Continuous controls are sampled into a simulation-friendly structure.



Example:



```cpp

struct PlayerInputFrame

{

&#x20;   Vec2 movement;

&#x20;   Vec2 aim;



&#x20;   bool shootHeld;

&#x20;   bool shootPressed;



&#x20;   bool interactPressed;

&#x20;   bool enterVehiclePressed;



&#x20;   bool openHudPressed;

};

```



The platform adapter is responsible for transforming:



```text

keyboard

controller

future input device

```



into this abstract structure.



\---



\# 19. COMMANDS



Discrete higher-level actions may use explicit commands.



Examples:



```text

DialogueChoiceCommand

FactionOrderCommand

AcceptMissionCommand

RejectMissionCommand

PurchaseCommand

SleepCommand

```



A command must express intent.



It should not directly express internal mutation.



Bad:



```text

SetFactionMoney(-500)

SetNpcHealth(0)

```



Good:



```text

BuyWeapon

AttackNpc

OrderFactionAttack

```



The simulation decides consequences.



\---



\# 20. DETERMINISTIC COMMAND ORDERING



Commands processed in a tick require stable ordering.



Each accepted command receives:



```text

TargetTick

SequenceNumber

Source

```



For V1 there is only one local player, so this remains simple.



Still, deterministic ordering is mandatory for:



\- testing;

\- replays;

\- debugging;

\- reproducible simulations.



\---



\# 21. ONE SIMULATION TICK



Canonical V1 tick pipeline:



```text

┌──────────────────────────┐

│  1. BEGIN TICK           │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  2. INGEST COMMANDS      │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  3. DECISIONS / INTENTS  │

│  player + nearby NPC AI  │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  4. MOVEMENT / VEHICLES  │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  5. INTERACTION / COMBAT │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  6. CONSEQUENCES         │

│  damage/death/etc.       │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  7. PERCEPTION/WITNESS   │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  8. POLICE / WANTED      │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│  9. MISSIONS / STORY     │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│ 10. STRATEGIC SYSTEMS    │

│ when scheduled           │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│ 11. COMMIT / CLEANUP     │

└────────────┬─────────────┘

&#x20;            ↓

┌──────────────────────────┐

│ 12. END TICK             │

└──────────────────────────┘

```



The order is explicit.



It must not accidentally depend on C++ container iteration order.



\---



\# 22. DECISION VS EXECUTION



NPC decisions and physical actions should be separated.



Example:



```text

AI decides:

ATTACK NPC #54

```



then:



```text

CombatSystem determines:

can attack?

weapon available?

range valid?

hit?

damage?

```



AI must not directly modify health.



Bad:



```cpp

if (npc.hates(target))

&#x20;   target.health -= 20;

```



Correct:



```text

AI

↓

AttackIntent

↓

CombatSystem

↓

DamageResult

```



This will become extremely important when missions, NPCs and factions interact.



\---



\# 23. TICK CONTEXT



Systems receive a common short-lived context.



Conceptually:



```cpp

struct SimulationContext

{

&#x20;   uint64\_t tick;



&#x20;   const GameRules\& rules;



&#x20;   DomainEventBuffer\& events;

&#x20;   StructuralCommandBuffer\& structuralChanges;



&#x20;   RuntimeRandomAccess\& random;

};

```



The context exists only while executing the tick.



It is not part of persistent state.



\---



\# 24. RUNTIME RANDOMNESS



World generation randomness and runtime randomness are separate.



\## Generation randomness



Derived from:



```text

WorldSeed

\+

GeneratorVersion

\+

Generator stream

```



It reconstructs the base world.



\## Runtime randomness



Examples:



```text

AI variation

combat spread

procedural mission details

faction strategic choices

market variation

```



Runtime deterministic RNG state must be persistent.



Conceptually:



```cpp

struct RuntimeRandomState

{

&#x20;   RngState combat;

&#x20;   RngState npc;

&#x20;   RngState missions;

&#x20;   RngState factions;

&#x20;   RngState market;

};

```



This is crucial.



Otherwise:



```text

save

↓

reload

```



could produce a completely different future despite identical player actions.



\---



\# 25. RNG OWNERSHIP



Systems do not create their own arbitrary RNG.



Bad:



```cpp

std::mt19937 rng(std::random\_device{}());

```



inside `FactionSystem`.



Correct:



```text

SimulationContext

↓

Faction RNG stream

↓

FactionSystem

```



Every random decision must belong to an identifiable deterministic stream.



\---



\# 26. DOMAIN EVENTS



Systems may produce explicit domain events.



Example:



```cpp

NpcKilled {

&#x20;   NpcId victim;

&#x20;   EntityId killer;

}



TerritoryControlChanged {

&#x20;   TerritoryId territory;

&#x20;   FactionId previous;

&#x20;   FactionId current;

}

```



Events describe something that \*\*already happened\*\*.



They must not replace normal control flow.



\---



\# 27. EVENT ORDER



Events produced during the tick are stored in deterministic insertion order.



Example:



```text

Tick 9128



\#0 WeaponFired

\#1 NpcDamaged

\#2 NpcKilled

\#3 WitnessObservedCrime

\#4 MissionObjectiveCompleted

```



This sequence is extremely useful for diagnostics.



\---



\# 28. NO MAGIC GLOBAL EVENT BUS



There will not be one omnipotent object such as:



```cpp

GlobalEventBus::emit(...)

```



accessible from everywhere.



That architecture makes dependencies invisible.



Instead, events are scoped through the active simulation context.



\---



\# 29. STRUCTURAL CHANGES



Some mutations should not occur immediately while iterating world containers.



Examples:



```text

spawn entity

destroy entity

add component

remove component

remove NPC

create vehicle

```



These use a deferred structural command buffer.



Example:



```text

Combat detects NPC death

↓

mark death result

↓

StructuralCommandBuffer

↓

Commit phase

↓

remove/update required runtime structures

```



This prevents iterator invalidation and inconsistent half-updated states.



\---



\# 30. NORMAL STATE CHANGES VS STRUCTURAL CHANGES



Normal mutation:



```text

health: 100 → 74

cash: 400 → 500

respect: 22 → 23

```



may happen directly through the owning system.



Structural mutation:



```text

create NPC

destroy vehicle

transfer/remove registry object

```



should normally be deferred to the commit phase.



\---



\# 31. SYSTEM OWNERSHIP RULE



Each piece of mutable state should have a clear system responsible for its rules.



Examples:



```text

Health              → Combat / Health logic

Territory control   → TerritorySystem

Faction strategy    → FactionSystem

Wanted level        → WantedSystem

Mission status      → MissionSystem

Market pricing      → MarketSystem

```



Other systems may request changes.



They should not silently rewrite foreign state whenever possible.



\---



\# 32. CROSS-SYSTEM EXAMPLE



Player kills an important dealer.



Wrong implementation:



```text

CombatSystem

├── kills NPC

├── changes territory

├── changes market price

├── fails mission

├── modifies faction strength

└── updates story graph

```



Correct architecture:



```text

CombatSystem

↓

NpcKilled

↓

Dealer system recognizes lost dealer

↓

Market updates dealer coverage

↓

Faction system updates operational strength

↓

Territory system evaluates presence

↓

Mission system evaluates consequences

↓

Story system reacts if relevant

```



The causal chain remains visible.



\---



\# 33. MULTI-RATE SYSTEM SCHEDULER



Not every system executes every tick.



`Simulation` owns a deterministic scheduler.



Example:



```text

20 Hz:

player control

movement

vehicles

combat



10 Hz:

nearby NPC decisions



5 Hz:

traffic decisions



1 Hz:

local environmental simulation



game-time scheduled:

market

territory

factions

long-distance simulation

```



The scheduler uses:



```text

SimulationClock

```



not real wall time.



\---



\# 34. NO FLOAT-TIME SCHEDULING DRIFT



Do not implement:



```cpp

timer += dt;



if (timer > 0.1f)

```



for authoritative strategic scheduling.



Prefer integer ticks or integral game-time units.



Example:



```cpp

if (tick % 4 == 0)

&#x20;   runNpcDecision();

```



or explicit scheduled game-time values.



This avoids accumulating floating-point timing drift.



\---



\# 35. FLOATING POINT POLICY



Floating point is acceptable for:



```text

position

velocity

rotation

aim

physics-like calculations

```



but deterministic architecture must not rely on unstable unordered operations.



Avoid assuming:



```text

floating point simulation

=

bit-identical on every future CPU/platform

```



V1 determinism target is initially:



> same build + same platform/runtime conditions + same seed/input → same simulation result.



True cross-platform bit-perfect determinism is NOT a V1 requirement unless later proven necessary.



\---



\# 36. ITERATION ORDER



Authoritative simulation must never depend accidentally on:



```text

unordered\_map iteration order

pointer addresses

allocation order

thread timing

```



When processing multiple semantic objects where order can affect results, use:



```text

stable ordered collection

```



or:



```text

explicit sorting by stable ID

```



Example:



```cpp

sort(npcs.begin(), npcs.end(), byNpcId);

```



when ordering affects outcomes.



\---



\# 37. CACHE POLICY



Derived data may be cached.



Examples:



```text

NPC lookup cache

spatial query cache

pathfinding cache

render visibility cache

relationship lookup cache

```



But:



```text

CACHE ≠ AUTHORITY

```



Caches must be rebuildable from authoritative state.



Therefore caches should generally not be serialized.



\---



\# 38. LOADING



Loading occurs conceptually as:



```text

deserialize persistent state

↓

validate

↓

migrate if required

↓

rebuild transient caches

↓

initialize Simulation runtime

↓

resume ticks

```



A saved memory address or runtime pointer must never be necessary.



\---



\# 39. NEW WORLD



New game flow:



```text

Game::createNewWorld(seed)

↓

WorldGenerator generates base state

↓

GameState created

↓

runtime RNG initialized

↓

state validated

↓

Simulation initialized

↓

Tick 0 ready

```



No renderer is required.



\---



\# 40. HEADLESS EXECUTION



The exact same `Game` and `Simulation` used by the playable executable must be usable like:



```cpp

Game game = Game::create(seed);



for (uint64\_t i = 0; i < 1'000'000; ++i)

{

&#x20;   game.tick();

}

```



No mock renderer is required.



No fake SDL window is required.



\---



\# 41. STATE HASHING



Development builds should support computing a deterministic state fingerprint.



Example:



```text

Tick: 40000

StateHash: 3B017D...



Tick: 40001

StateHash: 91AF82...

```



This allows tests such as:



```text

run A

seed = 42

inputs = X

→ hash ABC



run B

seed = 42

inputs = X

→ hash ABC

```



If:



```text

ABC != ABC

```



we immediately know determinism broke.



\---



\# 42. REPLAY-READY ARCHITECTURE



A full replay feature is not required for V1.



However, the runtime architecture should make future replay possible using:



```text

WorldSeed

\+

initial save/state

\+

ordered player inputs/commands

```



This is another reason not to allow uncontrolled randomness or wall-clock dependencies.



\---



\# 43. SIMULATION VALIDATION



At the end of selected ticks in debug/test builds, the engine may run invariants.



Examples:



```text

every NPC references an existing faction or NONE



every territory owner exists



every faction home territory exists



dead NPC cannot be active dealer



player controlledEntity exists



mission references valid NPCs



no duplicate persistent ID

```



Release builds may disable expensive checks.



\---



\# 44. ERROR PHILOSOPHY



Invalid core state should fail loudly during development.



Prefer:



```text

assert / validation error

```



over silently inventing replacement state.



Example:



Bad:



```text

Mission references missing NPC

→ ignore it

```



Better during development:



```text

Mission references missing NPC #182

Seed: 91821

Tick: 72918

Mission: 412

FATAL VALIDATION ERROR

```



Procedural corruption must be observable.



\---



\# 45. CORE RUNTIME OWNERSHIP SUMMARY



Final ownership:



```text

Application

│

├── Platform

├── InputAdapter

├── Renderer

├── FixedStepRunner

│

└── Game

&#x20;   │

&#x20;   ├── GameState       ← authoritative persistent reality

&#x20;   │

&#x20;   ├── Simulation      ← rules

&#x20;   │   └── Systems

&#x20;   │

&#x20;   └── CommandQueue    ← pending external intent

```



Additional immutable configuration:



```text

GameRules

```



is provided to simulation systems but is not mutable world state.



\---



\# 46. THE MOST IMPORTANT SEPARATION



The entire engine can be reduced to:



```text

GameState(t)

\+

Input(t)

\+

Deterministic Rules

\+

Deterministic Random State

&#x20;           ↓

GameState(t+1)

```



Conceptually:



```text

S(t+1) = F(S(t), I(t), R(t))

```



where:



```text

S = authoritative state

I = external intent

R = deterministic randomness

F = simulation rules

```



Everything visible on screen is downstream of this transformation.



\---



\# 47. MINIMUM CORE API



The first implementation should aim toward an interface approximately this small:



```cpp

class Game

{

public:

&#x20;   static Game createNew(

&#x20;       WorldSeed seed,

&#x20;       const GameRules\& rules

&#x20;   );



&#x20;   void submit(PlayerInputFrame input);



&#x20;   void submit(GameCommand command);



&#x20;   void tick();



&#x20;   const GameState\& state() const;



&#x20;   RenderSnapshot createRenderSnapshot() const;

};

```



Persistence later adds something conceptually similar to:



```cpp

SaveData save(const GameState\&);

GameState load(const SaveData\&);

```



The exact C++ signatures are not yet canonical.



The architectural boundaries are.



\---



\# 48. FIRST CORE RUNTIME TEST



Before factions, cars, weapons or ASCII graphics, this must work:



```text

create Game(seed = 12345)



tick 1000 times



save StateHash

```



Then:



```text

create Game(seed = 12345)



tick 1000 times



save StateHash

```



Requirement:



```text

HASH A == HASH B

```



Then:



```text

create Game(seed = 54321)



tick 1000 times

```



Requirement:



```text

HASH C != HASH A

```



This becomes the first proof that the engine foundation behaves correctly.



\---



\# 49. CORE RUNTIME INVARIANTS



\*\*R1\*\* — `GameState` is the authoritative world.



\*\*R2\*\* — `Simulation` contains rules, not persistent ownership.



\*\*R3\*\* — `Game` orchestrates but does not implement domain gameplay.



\*\*R4\*\* — real time exists outside simulation.



\*\*R5\*\* — game time exists inside simulation.



\*\*R6\*\* — runtime randomness is deterministic and saveable.



\*\*R7\*\* — persistent relationships use typed stable IDs.



\*\*R8\*\* — system execution order is explicit.



\*\*R9\*\* — structural changes are committed safely.



\*\*R10\*\* — iteration order must never create accidental nondeterminism.



\*\*R11\*\* — transient caches are rebuildable.



\*\*R12\*\* — the complete simulation can execute headlessly.



\*\*R13\*\* — identical seed + state + commands must produce identical results within the supported determinism environment.



\*\*R14\*\* — renderer output cannot affect `GameState`.



\---



\# 50. STATUS



With this specification, the fundamental responsibilities of:



```text

Application

Game

GameState

Simulation

System

```



are considered architecturally defined.



The next required architecture specification is:



```text

ENTITY + DOMAIN DATA MODEL

```



because before implementing `GameState` we must define exactly how physical entities, NPCs, factions, territories and semantic domain objects are stored and referenced.

