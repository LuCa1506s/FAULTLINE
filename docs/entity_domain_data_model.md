\# ENTITY + DOMAIN DATA MODEL SPECIFICATION — v0.1



\*\*Project:\*\* Procedural Crime Sandbox  

\*\*Parent specs:\*\* `ENGINE\_ARCHITECTURE\_SPEC v0.1`, `ENGINE\_CORE\_RUNTIME\_SPEC v0.1`  

\*\*Purpose:\*\* Define how physical entities, NPCs, vehicles, factions, territories and other persistent game concepts are represented and referenced.



\---



\# 1. CORE PRINCIPLE



The engine distinguishes two major categories:



```text

SPATIAL ENTITIES

```



and:



```text

DOMAIN OBJECTS

```



A concept becomes an `Entity` only if it needs an identity inside the physical/spatial world.



Examples:



```text

NPC       → Entity

Vehicle   → Entity

Dropped weapon → Entity

Door      → potentially Entity

Projectile → potentially Entity

```



But:



```text

Faction      → Domain Object

Territory    → Domain Object

Mission      → Domain Object

Market       → Domain Object

Relationship → Domain State

District     → Domain Object

```



Not everything belongs in the entity registry.



\---



\# 2. ENTITY DEFINITION



An Entity is fundamentally:



```cpp

EntityId

\+

components

```



An Entity has no required inheritance hierarchy.



Conceptually:



```cpp

struct Entity

{

&#x20;   EntityId id;

};

```



Its actual capabilities come from components stored elsewhere.



Example NPC:



```text

Entity #421



Transform

CharacterBody

Health

NpcIdentity

FactionMembership

Inventory

CombatState

AIState

```



Example civilian car:



```text

Entity #913



Transform

VehicleBody

VehiclePhysics

Health

Ownership

```



\---



\# 3. ENTITY IDs



Base type:



```cpp

struct EntityId

{

&#x20;   uint64\_t value;

};

```



`0` should be reserved as invalid/null.



Conceptually:



```text

EntityId::Invalid = 0

```



Entity IDs must never be recycled within an active world session if this could create stale-reference ambiguity.



Example:



```text

Entity #500 destroyed

```



The next entity should not casually become:



```text

Entity #500

```



again.



Preferred runtime allocation:



```text

1

2

3

4

...

```



or equivalent deterministic stable allocation.



\---



\# 4. TYPED DOMAIN IDs



Semantic objects receive their own ID types:



```cpp

NpcId

FactionId

TerritoryId

DistrictId

BuildingId

VehiclePersistentId

MissionId

MarketId

RelationshipId

```



These IDs are not interchangeable.



Example:



```text

NpcId(52)

FactionId(52)

```



may share the same raw integer but represent completely different namespaces.



\---



\# 5. ENTITY ID VS NPC ID



An NPC has two identities:



```text

NpcId

```



persistent semantic identity,



and:



```text

EntityId

```



current physical representation.



Example:



```text

NpcId #16

│

└── currently represented by EntityId #482

```



This separation is important.



An NPC may exist semantically even when not physically instantiated.



Example:



```text

Boss Moretti

```



may be:



```text

NPC ID: 16

Current physical entity: NONE

Location: District 4

Activity: meeting

```



When the player approaches:



```text

NpcId #16

↓

instantiate spatial entity

↓

EntityId #482

```



When the player leaves:



```text

EntityId #482

↓

despawn physical representation

↓

NpcId #16 remains alive

```



This is fundamental for large-world simulation.



\---



\# 6. NPC SEMANTIC STATE



Persistent NPC information belongs in `NpcState`.



Conceptually:



```cpp

struct NpcState

{

&#x20;   NpcId id;



&#x20;   NameId name;



&#x20;   NpcRole role;



&#x20;   FactionId faction;



&#x20;   DistrictId homeDistrict;



&#x20;   EntityId activeEntity;



&#x20;   NpcStatus status;



&#x20;   RespectValue respect;

&#x20;   LoyaltyValue loyalty;



&#x20;   RelationshipProfile relationships;



&#x20;   InventoryState inventory;



&#x20;   NpcHistoryState history;



&#x20;   bool important;

};

```



Exact fields may evolve.



The important architectural split is:



```text

NpcState

=

semantic persistent NPC

```



versus:



```text

Entity

=

current spatial manifestation

```



\---



\# 7. NPC STATUS



Canonical high-level NPC lifecycle:



```text

ALIVE

INJURED

ARRESTED

MISSING

DEAD

```



Potential additional runtime states:



```text

UNCONSCIOUS

FLEEING

IN\_COMBAT

```



should normally belong to physical/AI components, not the persistent high-level lifecycle.



\---



\# 8. IMPORTANT NPCs



Important NPCs remain instantiated semantically even when outside active simulation.



Example:



```text

NpcId #17



Role: Faction Boss

Status: ALIVE

District: Little Italy

CurrentEntity: NONE

```



The NPC still participates in:



```text

faction hierarchy

relationships

missions

story graph

strategic simulation

```



without requiring a physical body to exist constantly.



\---



\# 9. GENERIC BACKGROUND NPCs



Not every pedestrian needs permanent world identity.



We distinguish:



```text

PERSISTENT NPC

```



and:



```text

EPHEMERAL NPC

```



Persistent examples:



```text

boss

dealer

trusted member

mission NPC

named rival

important police NPC

owned employee

```



Ephemeral examples:



```text

random pedestrian

temporary crowd member

generic driver

background worker

```



Ephemeral NPCs may disappear when unloaded.



Persistent NPCs must survive world streaming.



\---



\# 10. PROMOTION



An ephemeral NPC may become persistent if gameplay makes them important.



Example:



```text

random civilian witnesses major crime

↓

becomes recurring witness

↓

player threatens them

↓

mission generated around them

```



Then:



```text

Ephemeral NPC

→ promoted

→ assigned NpcId

→ persistent NpcState created

```



This allows emergent gameplay without permanently tracking every pedestrian ever generated.



\---



\# 11. ENTITY REGISTRY



The spatial world uses a central `EntityRegistry`.



Conceptually:



```cpp

class EntityRegistry

{

public:

&#x20;   EntityId create();

&#x20;   void destroy(EntityId);



&#x20;   bool exists(EntityId) const;

};

```



Component storage remains separate.



Example:



```text

EntityRegistry

│

├── TransformStore

├── HealthStore

├── CharacterStore

├── VehicleStore

├── InventoryStore

├── AIStore

└── InteractionStore

```



\---



\# 12. COMPONENT STORAGE



V1 does not require archetype ECS complexity.



A practical architecture:



```cpp

ComponentStore<Transform>

ComponentStore<Health>

ComponentStore<CharacterBody>

ComponentStore<VehicleBody>

```



Conceptually:



```text

EntityId → Component

```



Possible initial implementation:



```cpp

std::unordered\_map<EntityId, Transform>

```



or dense sparse-set storage later.



Correctness comes first.



Performance layout can be optimized after profiling.



\---



\# 13. COMPONENT RULE



Components contain \*\*state\*\*.



Systems contain \*\*behavior\*\*.



Bad:



```cpp

struct Health

{

&#x20;   float hp;



&#x20;   void die();

&#x20;   void notifyMissionSystem();

&#x20;   void spawnBlood();

&#x20;   void reduceFactionPower();

};

```



Preferred:



```cpp

struct Health

{

&#x20;   float current;

&#x20;   float maximum;

};

```



Then:



```text

CombatSystem

HealthSystem

DeathSystem

```



handle behavior.



\---



\# 14. INITIAL CORE COMPONENTS



The first useful component set should remain small.



\### Transform



```cpp

struct Transform

{

&#x20;   Vec3 position;

&#x20;   float yaw;

};

```



\### Velocity



```cpp

struct Velocity

{

&#x20;   Vec3 linear;

};

```



\### Health



```cpp

struct Health

{

&#x20;   float current;

&#x20;   float maximum;

};

```



\### CharacterBody



```cpp

struct CharacterBody

{

&#x20;   CharacterStance stance;

&#x20;   float movementSpeed;

};

```



\### NpcLink



```cpp

struct NpcLink

{

&#x20;   NpcId npc;

};

```



\### VehicleLink



```cpp

struct VehicleLink

{

&#x20;   VehiclePersistentId vehicle;

};

```



\### InventoryLink



References semantic inventory state where appropriate.



\---



\# 15. TRANSFORM



Every spatial entity that exists in the physical world generally has:



```text

Transform

```



Position units:



```text

meters

```



Coordinate system must be globally standardized.



Recommended:



```text

X = East/West

Y = Up

Z = North/South

```



Exact handedness must remain consistent throughout the project.



\---



\# 16. WORLD COORDINATES



Use floating point for local positions.



Initial:



```cpp

float

```



is acceptable for an 8×8 km map.



If future map sizes or precision prove problematic, coordinate architecture must allow migration toward:



```text

double world coordinates

```



or:



```text

chunk + local position

```



without changing gameplay semantics.



\---



\# 17. SPATIAL PARTITION



Entities are indexed by a spatial structure.



Conceptually:



```text

World

└── SpatialGrid

&#x20;   └── Cell / Chunk

&#x20;       └── EntityIds

```



The spatial index must answer queries like:



```text

entities within 20m

NPCs near player

vehicles inside district

witness candidates around crime

```



The index is derived/cache state.



The authoritative position remains the Transform component.



\---



\# 18. BUILDINGS



Buildings have two identities.



Persistent semantic building:



```cpp

BuildingState

```



Physical representation:



```text

zero or more Entities

```



Example:



```text

BuildingId #204

Type: ApartmentBlock

District: #4

OwnerFaction: NONE

```



The renderer may draw the structure from procedural geometry without every wall becoming an Entity.



\---



\# 19. BUILDING STATE



Conceptually:



```cpp

struct BuildingState

{

&#x20;   BuildingId id;



&#x20;   DistrictId district;



&#x20;   BuildingArchetype archetype;



&#x20;   TransformData transform;



&#x20;   OwnershipState ownership;



&#x20;   InteriorReference interior;



&#x20;   BuildingFlags flags;

};

```



Most buildings should NOT consist of thousands of entities.



Only interactive parts need entity representation.



Examples:



```text

door

garage gate

loot object

mission object

interactive terminal

```



\---



\# 20. DISTRICTS



A district is a domain object.



Conceptually:



```cpp

struct DistrictState

{

&#x20;   DistrictId id;



&#x20;   DistrictArchetype archetype;



&#x20;   Polygon boundary;



&#x20;   PopulationStats population;



&#x20;   CivilianOpinion opinion;



&#x20;   PolicePressure police;



&#x20;   MarketId market;

};

```



A district represents geographical and simulation context.



It is not an Entity.



\---



\# 21. TERRITORIES



A territory is also a domain object.



Conceptually:



```cpp

struct TerritoryState

{

&#x20;   TerritoryId id;



&#x20;   DistrictId district;



&#x20;   Polygon boundary;



&#x20;   TerritoryControlState control;



&#x20;   FactionPresenceMap presence;



&#x20;   MarketInfluence marketInfluence;



&#x20;   ResistanceLevel resistance;

};

```



Control must be derived from actual simulation state rather than just mission flags.



\---



\# 22. TERRITORY CONTROL



Possible high-level state:



```text

CONTROLLED

CONTESTED

UNCONTROLLED

```



Potential data:



```text

dominant faction

presence by faction

dealer presence

local resistance

police pressure

```



The `TerritorySystem` computes control.



Other systems must not arbitrarily assign it.



\---



\# 23. FACTIONS



Faction is a domain object.



Conceptually:



```cpp

struct FactionState

{

&#x20;   FactionId id;



&#x20;   FactionName name;



&#x20;   FactionFamily family;



&#x20;   FactionLifecycle lifecycle;



&#x20;   TerritoryId homeTerritory;



&#x20;   Money treasury;



&#x20;   CohesionValue cohesion;



&#x20;   NpcId leader;



&#x20;   std::vector<NpcId> importantMembers;



&#x20;   FactionRelationshipMap relations;



&#x20;   FactionStrategicState strategy;

};

```



Faction membership of individual NPCs stays in NPC state.



Faction state should not redundantly own every piece of member data.



\---



\# 24. FACTION MEMBER INDEX



For performance, a faction may maintain:



```text

member index

```



but this should be considered derived/rebuildable if possible.



Avoid having:



```text

NpcState.faction = Faction #3

```



and simultaneously a separately authoritative list that can disagree.



Single source of truth is preferable.



\---



\# 25. FACTION HIERARCHY



Hierarchy should use explicit semantic roles.



Example:



```text

BOSS

UNDERBOSS

CAPTAIN

MEMBER

DEALER

ASSOCIATE

```



Exact names may differ by faction archetype.



Internally, roles should map to generalized authority levels rather than hardcoding Mafia-specific structures everywhere.



Example:



```text

AuthorityTier

0–5

```



plus cultural display role.



\---



\# 26. RELATIONSHIPS



Relationships are persistent semantic state.



Two categories:



```text

NPC ↔ NPC

NPC ↔ PLAYER

FACTION ↔ FACTION

```



Faction relationships should be separate from personal NPC relationships.



Example:



```text

Faction A hates Faction B

```



does not imply:



```text

every NPC A personally hates every NPC B

```



\---



\# 27. NPC RELATIONSHIP STATE



Conceptually:



```cpp

struct NpcRelationship

{

&#x20;   NpcId other;



&#x20;   float trust;

&#x20;   float loyalty;

&#x20;   float fear;

&#x20;   float hostility;

&#x20;   float debt;

};

```



Exact dimensions will be refined later.



Avoid one generic:



```text

relationship = 73

```



because we need qualitatively different social effects.



An NPC may:



```text

fear player

but hate player

but still cooperate

```



which one scalar cannot express.



\---



\# 28. SPARSE RELATIONSHIPS



Do not create an explicit relationship record for every pair of NPCs.



With:



```text

20,000 NPCs

```



a full matrix would be absurd.



Use sparse relationships.



Store records only when meaningful.



Default relationships are inferred from:



```text

faction

role

district

history

world conditions

```



Explicit records override or modify the baseline.



\---



\# 29. VEHICLES



Persistent vehicle identity is separate from current physical entity.



```text

VehiclePersistentId

```



and:



```text

EntityId

```



Example:



```text

Player's car #14

```



exists persistently even when parked outside loaded area.



Its semantic state may include:



```cpp

struct VehicleState

{

&#x20;   VehiclePersistentId id;



&#x20;   VehicleArchetype archetype;



&#x20;   VehicleOwnership ownership;



&#x20;   VehicleCondition condition;



&#x20;   WorldLocation location;



&#x20;   EntityId activeEntity;

};

```



\---



\# 30. VEHICLE PHYSICAL COMPONENTS



When instantiated:



```text

Transform

VehicleBody

VehiclePhysics

Health

VehicleLink

```



The detailed simulation state may be active only while near the player.



Far away, the vehicle may exist only as:



```text

VehicleState.location

```



\---



\# 31. ITEMS



Items need distinction between:



```text

ITEM DEFINITION

ITEM INSTANCE

WORLD ENTITY

```



Example:



```text

WeaponDefinition: PISTOL\_BASIC

```



describes rules.



```text

ItemInstance #845

```



represents a specific owned object.



If dropped:



```text

ItemInstance #845

→ represented by Entity #901

```



The item remains the same item.



\---



\# 32. ITEM DEFINITIONS



Immutable definitions:



```cpp

struct WeaponDefinition

{

&#x20;   WeaponDefId id;



&#x20;   float damage;

&#x20;   float fireRate;

&#x20;   float range;

&#x20;   AmmoType ammo;

};

```



These belong to `GameRules`.



Not `GameState`.



\---



\# 33. ITEM INSTANCES



Runtime data:



```cpp

struct WeaponInstance

{

&#x20;   ItemInstanceId id;



&#x20;   WeaponDefId definition;



&#x20;   uint16\_t ammoInMagazine;



&#x20;   float condition;

};

```



This belongs to persistent game state.



\---



\# 34. INVENTORY



Inventory stores IDs of item instances.



Example:



```cpp

struct InventoryState

{

&#x20;   std::vector<ItemInstanceId> items;

};

```



Avoid embedding giant item structures repeatedly.



Ownership remains explicit.



\---



\# 35. MISSIONS



Mission is a domain object.



Conceptually:



```cpp

struct MissionState

{

&#x20;   MissionId id;



&#x20;   MissionType type;

&#x20;   MissionResolutionState status;



&#x20;   NpcId issuer;



&#x20;   MissionObjectiveGraph objectives;



&#x20;   MissionContext context;



&#x20;   MissionReward reward;



&#x20;   MissionConsequences consequences;

};

```



Mission objects refer to world concepts using stable IDs.



\---



\# 36. MISSION REFERENCES



Example mission:



```text

Protect Dealer

```



references:



```text

NpcId dealer

TerritoryId location

FactionId issuerFaction

```



not raw pointers.



If the dealer dies:



```text

MissionSystem

```



determines how the mission mutates or resolves.



The reference itself remains valid enough to know which NPC died.



\---



\# 37. STORY GRAPH



The story graph is domain state.



Nodes reference:



```text

NPCs

factions

territories

missions

world facts

```



using IDs.



Story graph nodes must never require active physical entities.



This allows the story to evolve around distant or unloaded actors.



\---



\# 38. MARKET



The drug market is domain simulation state.



Recommended hierarchy:



```text

Global Market

&#x20;  ↓

District Market

&#x20;  ↓

Territory / Dealer influence

```



Example:



```cpp

struct MarketState

{

&#x20;   MarketId id;



&#x20;   float demand;

&#x20;   float supply;

&#x20;   Money price;



&#x20;   FactionMarketShares shares;

};

```



Exact formulas belong to the dedicated Market Specification.



\---



\# 39. DEALERS



Dealer is a role, not a class hierarchy.



A dealer is:



```text

NpcState

\+

DealerRoleState

```



Conceptually:



```cpp

struct DealerRoleState

{

&#x20;   NpcId npc;



&#x20;   TerritoryId territory;



&#x20;   FactionId faction;



&#x20;   DealerStatus status;



&#x20;   float capacity;

};

```



This allows the same NPC to simultaneously have other properties.



No:



```text

class Dealer : public GangMember

```



required.



\---



\# 40. ROLE COMPONENTS / DOMAIN ROLES



NPCs may accumulate semantic roles:



```text

Dealer

FactionLeader

MissionIssuer

Informant

PoliceOfficer

Witness

TraitorCandidate

```



Do not force them into one exclusive enum if roles can overlap.



Prefer:



```text

NpcRoles

```



or dedicated role states.



\---



\# 41. POLICE



Generic police officers may be ephemeral NPC entities.



Important police figures may be persistent `NpcState`.



Police organization itself can be modeled as domain state:



```text

PoliceWorldState

```



with:



```text

district pressure

active searches

wanted investigations

resources

```



Exact structure belongs to Police/Wanted spec.



\---



\# 42. PLAYER



The player's physical body uses the same physical mechanisms as NPCs where reasonable.



Example:



```text

Transform

Health

CharacterBody

Inventory

CombatState

```



Do not build an entirely separate physics/combat implementation for the player.



Differences belong in:



```text

PlayerController

PlayerState

```



not duplicate mechanics.



\---



\# 43. PLAYER VS NPC CONTROL



Physical character:



```text

Character Entity

```



Control source:



```text

PlayerControl

```



or:



```text

AIControl

```



Same movement/combat system.



This makes mechanics consistent.



\---



\# 44. ENTITY LIFETIME



Spatial entity states:



```text

CREATED

ACTIVE

PENDING\_DESTROY

DESTROYED

```



Destroy requests are deferred until commit.



After destruction:



```text

EntityId

```



must no longer resolve.



Persistent semantic object may survive.



Example:



```text

NPC leaves active zone

```



means:



```text

physical entity destroyed

NPC semantic state survives

```



not:



```text

NPC dies

```



\---



\# 45. DESPAWN VS DEATH



These are completely different operations.



```text

DESPAWN

```



means:



> physical representation no longer required.



```text

DEATH

```



means:



> semantic NPC lifecycle changed permanently.



Never conflate them.



\---



\# 46. WORLD LOCATION



Semantic objects outside active physical simulation still need location.



Use a persistent representation:



```cpp

struct WorldLocation

{

&#x20;   DistrictId district;

&#x20;   TerritoryId territory;



&#x20;   Vec3 approximatePosition;

};

```



Potentially:



```text

BuildingId

```



when indoors.



When physically instantiated:



```text

Transform

```



becomes authoritative for exact position.



Before despawn, semantic location is synchronized.



\---



\# 47. ACTIVE ENTITY MAPPING



Persistent objects needing spatial representation maintain mappings:



```text

NpcId → EntityId

VehiclePersistentId → EntityId

```



These mappings are runtime state.



Example:



```text

Npc #22 active → Entity #809

Npc #23 inactive → NONE

```



\---



\# 48. LOADING AN AREA



When a region activates:



```text

Spatial streaming detects region

↓

query persistent semantic objects

↓

instantiate required entities

↓

attach components

↓

initialize transient AI/physics

```



\---



\# 49. UNLOADING AN AREA



When region deactivates:



```text

capture relevant semantic result

↓

update persistent location/state

↓

destroy spatial entity

↓

retain persistent domain object

```



This is the bridge between physical and abstract simulation.



\---



\# 50. ACTIVE / ABSTRACT TRANSITION INVARIANT



Switching simulation level must not fundamentally change game reality.



Example:



```text

Dealer alive before unload

```



must not spontaneously become:



```text

dead after reload

```



unless the abstract simulation generated that outcome.



The transition must be explicit.



\---



\# 51. DATA NORMALIZATION



Avoid unnecessary duplicated authoritative information.



Bad:



```text

NpcState.faction = 3



FactionState.members contains NPC

```



if both are treated as primary truth.



Better:



```text

NpcState.faction = 3

```



authoritative,



and:



```text

Faction member cache

```



derived.



\---



\# 52. EXCEPTIONS TO NORMALIZATION



Some duplication may be justified for performance.



But whenever duplicated state exists, one side must be explicitly authoritative.



Example:



```text

FactionPresenceByTerritory

```



may cache values derived from many NPC/dealer states.



It is not independent truth.



\---



\# 53. OWNERSHIP



Domain stores own objects.



Example:



```text

NpcStore owns NpcState

FactionStore owns FactionState

TerritoryStore owns TerritoryState

MissionStore owns MissionState

VehicleStore owns VehicleState

```



References between stores use IDs.



No store should own objects belonging to another store through raw pointers.



\---



\# 54. STORE API



Conceptual pattern:



```cpp

class NpcStore

{

public:

&#x20;   NpcState\& get(NpcId);

&#x20;   const NpcState\& get(NpcId) const;



&#x20;   bool exists(NpcId) const;



&#x20;   NpcId create(...);

};

```



Failure to resolve a supposedly valid persistent ID should be treated as a validation error.



\---



\# 55. NULL REFERENCES



Use explicit invalid/optional IDs.



Example:



```cpp

std::optional<FactionId>

```



or typed null value.



Avoid magic values like:



```text

\-999

```



\---



\# 56. DATA-DRIVEN DEFINITIONS



Definitions should be externalizable.



Examples:



```text

weapon types

vehicle archetypes

building archetypes

district archetypes

mission templates

faction archetypes

```



The engine should load these into `GameRules`.



Exact external format can initially be:



```text

JSON

```



or another human-readable format.



Runtime save format is a separate concern.



\---



\# 57. ENUMS VS STRINGS



Runtime hot paths should not compare freeform strings like:



```cpp

if (npc.role == "dealer")

```



Prefer strongly typed enum/IDs:



```cpp

NpcRole::Dealer

```



Names displayed to the player remain data.



\---



\# 58. SERIALIZATION



All persistent domain objects must be serializable without relying on:



```text

memory address

platform handle

renderer reference

thread state

```



Components that contain transient engine resources must be excluded or rebuilt.



\---



\# 59. PERSISTENCE CLASSIFICATION



Every data field should conceptually be one of:



```text

GENERATED

PERSISTENT\_RUNTIME

TRANSIENT

DERIVED

```



Example:



```text

Npc name             → GENERATED

Npc alive/dead       → PERSISTENT\_RUNTIME

AI path              → TRANSIENT

Faction member cache → DERIVED

```



This classification should guide serialization.



\---



\# 60. EXAMPLE NPC



```text

NpcId: 18



NpcState

├── Name: Carlo Moretti

├── Faction: #2

├── Role: Boss

├── Status: Alive

├── Respect: 91

├── Loyalty: 84

├── HomeDistrict: #4

├── Important: true

└── ActiveEntity: #882

```



Physical entity:



```text

Entity #882



Transform

CharacterBody

Health

InventoryLink

NpcLink → NpcId #18

AIState

CombatState

```



If unloaded:



```text

Entity #882 destroyed

```



but:



```text

NpcId #18 continues existing.

```



\---



\# 61. EXAMPLE DEALER



```text

NpcId #114



NpcState

├── faction: #3

├── status: alive

└── ...



DealerRoleState

├── npc: #114

├── territory: #8

├── capacity: 0.63

└── status: active

```



No subclass is required.



\---



\# 62. EXAMPLE FACTION



```text

FactionId #3



Name: Eastside Kings

Family: StreetGang

Lifecycle: Active

HomeTerritory: #7

Leader: Npc #42

Treasury: 128300

Cohesion: 0.71

```



Territorial control is NOT simply stored as:



```text

Faction owns \[1,2,3,4]

```



without territorial simulation verification.



Territory state remains authoritative for local control.



\---



\# 63. EXAMPLE PLAYER CAR



Persistent:



```text

Vehicle #16



Type: Sedan\_A

Owner: Player

Condition: 0.84

Location: Garage #2

ActiveEntity: NONE

```



Player enters garage:



```text

Vehicle #16

↓

Entity #944 spawned

```



Player drives away:



```text

Transform updated

```



Area unloads:



```text

VehicleState.location updated

Entity #944 removed

```



\---



\# 64. VALIDATION RULES



Initial validation invariants:



\### E1



Every active EntityId exists in `EntityRegistry`.



\### E2



Every `NpcLink` points to an existing `NpcState`.



\### E3



At most one active entity represents one persistent NPC.



\### E4



At most one active entity represents one persistent vehicle.



\### E5



Dead NPCs cannot own active physical character entities except during controlled death-resolution windows.



\### E6



Every faction leader references an existing living/relevant NPC or an explicit succession state.



\### E7



Every territory references a valid district.



\### E8



Every persistent vehicle has a valid archetype.



\### E9



Every item instance references a valid item definition.



\### E10



No persistent domain relationship depends on raw memory addresses.



\---



\# 65. ENTITY SYSTEM SUCCESS TEST



Before implementing gameplay, the engine must support:



```text

create persistent NPC

↓

instantiate NPC entity

↓

move entity

↓

sync location

↓

despawn entity

↓

NPC state survives

↓

instantiate again

↓

same NpcId

```



And:



```text

create vehicle

↓

spawn vehicle entity

↓

despawn

↓

save

↓

load

↓

respawn

```



without identity loss.



\---



\# 66. ARCHITECTURAL INVARIANTS



\*\*D1\*\* — Physical existence and semantic existence are separate concepts.



\*\*D2\*\* — Persistent NPC identity uses `NpcId`, not `EntityId`.



\*\*D3\*\* — Persistent vehicles have semantic IDs separate from physical entities.



\*\*D4\*\* — Factions, territories, missions and markets are not ECS entities.



\*\*D5\*\* — Components store state; systems implement behavior.



\*\*D6\*\* — Persistent references use typed IDs.



\*\*D7\*\* — Raw pointer graphs are not authoritative world state.



\*\*D8\*\* — Despawn does not mean death.



\*\*D9\*\* — Background NPCs do not automatically require permanent persistence.



\*\*D10\*\* — Ephemeral NPCs may be promoted to persistent NPCs.



\*\*D11\*\* — Duplicated data must have one explicitly authoritative source.



\*\*D12\*\* — Caches and spatial indexes are rebuildable.



\*\*D13\*\* — Entity/component architecture remains composition-first.



\*\*D14\*\* — Streaming must preserve semantic continuity.



\---



\# 67. STATUS



The fundamental data architecture is now defined for:



```text

Entity

NPC

Vehicle

Faction

Territory

District

Building

Mission

Market

Item

Relationship

Player

```



Exact field-level definitions will be refined by their dedicated subsystem specifications.



The next architectural block is:



```text

WORLD STATE + SPATIAL / STREAMING MODEL

```



which defines:



```text

chunks

world coordinates

loaded regions

active simulation radius

abstract simulation

entity activation/deactivation

spatial queries

```



before the procedural world generator is implemented.

