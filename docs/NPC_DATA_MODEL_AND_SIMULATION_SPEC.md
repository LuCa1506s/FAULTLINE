# NPC DATA MODEL + SIMULATION SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**File:** `07_NPC_DATA_MODEL_AND_SIMULATION_SPEC.md`  
**Parent specs:** `01` through `06`  
**Purpose:** Define persistent NPC identity, social state, roles, decision-making, physical/abstract simulation and lifecycle.

---

# 1. CORE PRINCIPLE

NPCs are not disposable mission props.

An NPC may affect:

```text
factions
territories
market
missions
relationships
story
police
player progression
```

even while physically unloaded.

Therefore:

```text
NPC SEMANTIC EXISTENCE
≠
NPC PHYSICAL ENTITY
```

---

# 2. NPC CATEGORIES

The simulation distinguishes:

```text
MAJOR
PERSISTENT
EPHEMERAL
AGGREGATE
```

## MAJOR

Important named characters.

Examples:

```text
boss
underboss
important dealer
important rival
police figure
potential traitor
```

Always possess persistent `NpcId`.

## PERSISTENT

NPCs made relevant through gameplay.

Examples:

```text
recruited member
recurring witness
personal enemy
mission participant
dealer
player associate
```

## EPHEMERAL

Locally generated individuals.

Examples:

```text
pedestrian
generic gang member
generic police officer
driver
worker
```

Normally disappear when their region unloads.

## AGGREGATE

No individual physical identity currently exists.

Example:

```text
Faction #3
Territory #8
generic members: 12
```

---

# 3. NPC IDENTITY

Persistent NPCs use:

```cpp
NpcId
```

Physical representation uses:

```cpp
EntityId
```

Therefore:

```text
NpcId #21
```

may exist while:

```text
activeEntity = NONE
```

This is normal.

---

# 4. NPC STATE

Conceptual structure:

```cpp
struct NpcState
{
    NpcId id;

    NpcIdentity identity;

    NpcLifecycle lifecycle;

    WorldLocation location;

    std::optional<FactionId> faction;

    NpcRoles roles;

    NpcTraits traits;

    NpcSocialState social;

    NpcCapabilities capabilities;

    InventoryId inventory;

    NpcMemory memory;

    NpcActivityState activity;

    std::optional<EntityId> activeEntity;

    bool major;
};
```

Exact storage layout may change.

The semantic separation must not.

---

# 5. IDENTITY

`NpcIdentity` contains largely immutable/generated information.

Conceptually:

```cpp
struct NpcIdentity
{
    NameId name;

    AgeBand ageBand;

    BackgroundId background;

    AppearanceSeed appearanceSeed;
};
```

Do not store gameplay behavior inside identity data.

---

# 6. NPC LIFECYCLE

High-level persistent lifecycle:

```text
ALIVE
INJURED
ARRESTED
MISSING
DEAD
```

Possible specialized states can be added later.

These represent semantic reality.

---

# 7. PHYSICAL STATES

Temporary physical conditions such as:

```text
running
aiming
fleeing
unconscious
in combat
driving
```

belong to active Entity components.

They are NOT replacements for persistent lifecycle state.

---

# 8. DEATH

Death is persistent.

Pipeline:

```text
physical or abstract cause
↓
death validated
↓
NpcLifecycle = DEAD
↓
roles invalidated/updated
↓
FactionSystem reacts
↓
MissionSystem reacts
↓
StoryGraph reacts
↓
relationships/history update
```

Streaming must never recreate a dead NPC as alive.

---

# 9. INJURY

Injury must exist at two levels.

Physical combat uses detailed health.

Persistent abstraction stores meaningful consequences.

Example:

```text
minor injury
serious injury
critical injury
```

When an NPC unloads:

```text
physical health state
→ persistent injury state
```

where required.

---

# 10. RECOVERY

Injured NPCs may recover over game time.

Recovery depends on:

```text
injury severity
time
location
resources
special circumstances
```

V1 does not require medical simulation.

---

# 11. ARREST

Persistent NPCs may become:

```text
ARRESTED
```

Arrest stores enough information for future resolution:

```text
arrest time
expected release / unresolved status
reason/context if relevant
```

An arrested NPC cannot simultaneously perform ordinary faction operations.

---

# 12. MISSING

`MISSING` is useful when:

```text
location unknown
story intentionally hides actor
kidnapping
unresolved disappearance
```

It must not become a generic dumping ground for invalid NPC state.

---

# 13. ROLES

Roles are compositional.

An NPC may simultaneously be:

```text
FactionMember
Dealer
MissionIssuer
Informant
Witness
FactionLeader
```

Do NOT use one mutually exclusive:

```cpp
NpcType type;
```

for everything.

---

# 14. ROLE STATE

Roles that require state should own explicit data.

Example:

```cpp
struct DealerRole
{
    TerritoryId territory;
    float capacity;
    DealerStatus status;
};
```

Example:

```cpp
struct FactionLeadershipRole
{
    AuthorityTier authority;
};
```

---

# 15. FACTION MEMBERSHIP

Faction membership is persistent semantic state.

Conceptually:

```cpp
struct FactionMembership
{
    FactionId faction;

    AuthorityTier authority;

    float loyalty;
};
```

Exact loyalty ownership may later live in social state.

Only one field should be authoritative.

---

# 16. AUTHORITY

Faction hierarchy uses generalized authority.

Example:

```text
0 OUTSIDER
1 ASSOCIATE
2 MEMBER
3 SENIOR
4 COMMAND
5 LEADER
```

Display names may differ by faction culture.

This prevents Mafia terminology from becoming engine logic.

---

# 17. PERSONALITY MODEL

Do not build a 30-stat personality simulator.

Initial useful traits:

```text
aggression
ambition
riskTolerance
loyaltyBias
greed
courage
```

Potential range:

```text
0.0 – 1.0
```

Every trait must affect behavior.

---

# 18. AGGRESSION

Influences willingness to:

```text
fight
threaten
retaliate
escalate
```

It does not mean automatic violence.

Context still matters.

---

# 19. AMBITION

Influences:

```text
desire for promotion
leadership competition
betrayal opportunity
independent action
```

Ambitious NPCs become especially relevant during weak leadership.

---

# 20. RISK TOLERANCE

Influences willingness to act under:

```text
police pressure
strong opponents
low resources
uncertain information
```

---

# 21. LOYALTY BIAS

Represents baseline tendency toward organizational loyalty.

Actual loyalty also depends on:

```text
history
relationships
leadership
success/failure
```

Personality must not completely determine loyalty.

---

# 22. GREED

May influence:

```text
bribes
payment demands
betrayal
mission negotiation
financial crime choices
```

---

# 23. COURAGE

Influences:

```text
fleeing
surrender
combat persistence
intimidation outcomes
```

---

# 24. NO CHARISMA STAT

Canonical design has no generic player Charisma stat.

Social outcomes depend on actual context.

Relevant values include:

```text
player Respect
NPC Respect
personal relationship
fear
loyalty
faction power
recent actions
political situation
```

---

# 25. NPC RESPECT

NPCs have their own perceived status.

Conceptually:

```text
0–100
```

NPC Respect can affect:

```text
authority
social outcomes
mission importance
succession
faction influence
```

---

# 26. RESPECT ≠ LOYALTY

An NPC may:

```text
respect player greatly
but dislike them
```

or:

```text
love player personally
but consider them weak
```

These dimensions must remain separate.

---

# 27. RELATIONSHIP DIMENSIONS

Initial explicit relationship model:

```text
trust
fear
hostility
personalLoyalty
debt
```

Not every NPC pair receives explicit data.

---

# 28. SPARSE RELATIONSHIPS

Only meaningful relationships are stored.

Default behavior is inferred from:

```text
factions
roles
world history
district
current politics
```

Explicit records represent meaningful deviations/history.

---

# 29. PLAYER RELATIONSHIPS

Player relationships should always be eligible for persistence once interaction becomes meaningful.

Examples:

```text
player threatens NPC
player saves NPC
player betrays NPC
player owes NPC
NPC owes player
```

---

# 30. TRUST

Trust represents expectation that another actor behaves reliably.

Can increase through:

```text
fulfilled promises
successful cooperation
protection
honesty
```

Can fall through:

```text
betrayal
failed commitments
lies
abandonment
```

---

# 31. FEAR

Fear is not friendship.

High fear may produce obedience.

It may also produce:

```text
avoidance
secret betrayal
police cooperation
revenge when opportunity appears
```

---

# 32. HOSTILITY

Hostility measures active negative disposition.

It may arise from:

```text
personal attack
faction conflict
loss
humiliation
betrayal
```

---

# 33. PERSONAL LOYALTY

Important for:

```text
player-created faction
succession
betrayal
faction splits
```

Personal loyalty can differ from organizational loyalty.

Example:

```text
loyal to Boss A
not loyal to Faction A after Boss dies
```

---

# 34. DEBT / FAVOR

NPCs may owe or be owed favors.

This supports canonical cross-faction favors.

Examples:

```text
information
protection
safe passage
mission assistance
debt relief
```

---

# 35. NPC MEMORY

We do NOT store every event an NPC ever witnessed.

Use bounded meaningful memory.

Examples:

```text
PlayerSavedMe
PlayerThreatenedMe
PlayerKilledFriend
FactionWonWar
LeaderFailed
PlayerPaidWell
```

---

# 36. MEMORY ENTRY

Conceptually:

```cpp
struct NpcMemoryEntry
{
    MemoryType type;

    Entity/Npc/Faction references;

    Importance importance;

    GameTime age;
};
```

Exact representation may use variant/event IDs.

---

# 37. MEMORY DECAY

Some memories may decay.

Examples:

```text
minor insult
small payment dispute
```

Major memories may persist:

```text
murder
betrayal
life saved
leader death
```

---

# 38. HISTORY VS MEMORY

`WorldHistory` records objective important events.

`NpcMemory` records what matters to one NPC.

These are different.

NPCs do not automatically know all WorldHistory events.

---

# 39. KNOWLEDGE

NPC decisions should eventually distinguish:

```text
world truth
```

from:

```text
NPC knowledge
```

V1 may simplify local information propagation.

But mission secrets and hidden issuers require the architecture not to assume omniscience.

---

# 40. CAPABILITIES

Persistent NPC capabilities summarize useful competence.

Initial candidates:

```text
combat
driving
leadership
streetKnowledge
business
```

Do not turn this into a giant RPG skill tree.

---

# 41. CAPABILITIES PURPOSE

Capabilities affect:

```text
operation resolution
combat abstraction
succession
assigned jobs
mission outcomes
```

If a capability has no systemic use, remove it.

---

# 42. NPC DECISION MODEL

NPC behavior uses two levels:

```text
PHYSICAL AI
```

and:

```text
SEMANTIC AI
```

---

# 43. PHYSICAL AI

Used for active entities.

Handles:

```text
movement
cover
combat
fleeing
driving
local interaction
immediate perception
```

Runs frequently.

---

# 44. SEMANTIC AI

Used outside full physical simulation.

Handles decisions such as:

```text
travel
work
meet
hide
participate in operation
remain home
perform dealer activity
```

without physical movement simulation.

---

# 45. PHYSICAL AI MUST NOT OWN PERSONALITY

Physical AI consumes persistent NPC traits.

Example:

```text
NpcState.courage
↓
combat decision
```

Personality remains semantic persistent state.

---

# 46. ACTIVITY STATE

Persistent NPCs may have semantic activity:

```text
IDLE
WORKING
TRAVELLING
MEETING
HIDING
FACTION_OPERATION
ARRESTED
RECOVERING
```

This allows unloaded actors to keep functioning.

---

# 47. DAILY SCHEDULES

V1 explicitly does NOT require detailed citizen schedules.

Important NPC activity can use lightweight scheduling.

Example:

```text
dealer
→ operating in territory during relevant periods

boss
→ safehouse / meeting / operation
```

Avoid Sims-like daily simulation.

---

# 48. SEMANTIC LOCATION

Inactive persistent NPC:

```text
Npc #42

location:
Building #19
```

or:

```text
Territory #7
```

or:

```text
Travelling → Territory #11
```

Exact physical coordinates are unnecessary until materialization.

---

# 49. TRAVEL

Persistent travel uses:

```text
origin
destination
departure
arrival
route/context
```

When physically irrelevant, movement is abstract.

---

# 50. MATERIALIZATION

When an NPC becomes physically relevant:

```text
NpcState
+
current activity
+
semantic location
+
persistent condition
↓
Entity
```

Generated physical components must reflect semantic truth.

---

# 51. ABSTRACTIFICATION

When unloading:

```text
active Entity
↓
extract relevant persistent state
↓
update NpcState
↓
destroy physical Entity
```

Do not persist:

```text
animation frame
temporary path
physics contact
```

---

# 52. INVENTORY

Persistent important NPC inventory survives streaming.

Generic ephemeral NPC inventory may be generated contextually.

If an ephemeral item becomes gameplay-relevant:

```text
player steals it
mission references it
```

it becomes persistent.

---

# 53. NPC MONEY

Do not automatically simulate a bank account for every pedestrian.

Persistent financial state is useful primarily when needed for:

```text
dealers
important NPCs
transactions
debts
missions
```

Generic population remains abstract.

---

# 54. DEALER NPCs

Dealer is a semantic role.

Dealer behavior may include:

```text
work
hide
request protection
change territory
lose supply
generate faction income
```

Dealer output integrates with MarketSystem.

---

# 55. FACTION LEADERS

Leaders participate in:

```text
strategy
succession
diplomacy
mission issuance
internal cohesion
```

Leader personality may modify faction strategy.

Faction strategy must still remain a FactionSystem responsibility.

---

# 56. POTENTIAL TRAITORS

Do NOT generate:

```text
traitor = true
```

and guarantee betrayal.

Prefer:

```text
betrayal propensity
+
ambition
+
loyalty
+
future opportunities
```

The world may never trigger betrayal.

This preserves emergent outcomes.

---

# 57. SOCIAL DECISIONS

NPC social decisions use context.

Example intimidation:

```text
player Respect
vs
NPC Respect

player current power
NPC courage
fear
relationship
faction backing
recent history
```

Result:

```text
comply
refuse
stall
lie
retaliate
```

No single dice roll determines everything.

---

# 58. NEGOTIATION

Negotiation may consider:

```text
requested reward
NPC greed
relationship
mission importance
player Respect
NPC power
```

Dialogue remains short and intent-based.

---

# 59. DIALOGUE INTENTS

Canonical response styles include:

```text
ACCEPT
REFUSE
NEGOTIATE
THREATEN
LIE
ASK
```

Dialogue content is presentation.

Social consequence logic belongs to simulation.

---

# 60. WITNESSES

Witness is normally a temporary role.

When an NPC observes a meaningful crime:

```text
WitnessRole
```

may be created.

Possible outcomes:

```text
report
flee
hide
be threatened
become persistent
```

Detailed witness logic belongs to Police/Wanted spec.

---

# 61. EPHEMERAL PROMOTION

Promotion criteria may include:

```text
meaningful player interaction
crime witness
mission involvement
recurring encounter
recruitment
personal relationship
unique possession/state
```

Pipeline:

```text
Ephemeral Entity
↓
allocate NpcId
↓
create NpcState
↓
copy relevant state
↓
attach NpcLink
```

---

# 62. PROMOTION STABILITY

Promotion must not alter who the person is physically.

Example:

```text
Civilian Entity #918
```

gets promoted.

The existing physical entity remains.

It simply gains persistent semantic linkage.

---

# 63. GENERIC FACTION MEMBERS

Faction aggregate manpower may materialize as ephemeral NPCs.

Example:

```text
Faction #2 sends 6 generic members
```

Physical gameplay:

```text
6 generated faction NPC entities
```

If one becomes meaningful:

```text
promote
```

The others may return to aggregate representation later.

---

# 64. AGGREGATE CONSERVATION

Materializing generic members must not create free manpower.

Example:

```text
aggregate members = 20
```

6 are materialized.

Strategic availability must understand:

```text
14 abstract
6 physical
```

not:

```text
20 abstract
+
6 free physical
```

---

# 65. PHYSICAL LOSSES UPDATE AGGREGATE STATE

If three materialized generic members die:

```text
Faction manpower
```

must fall accordingly.

Physical and strategic representations refer to the same resources.

---

# 66. NPC DECISION PRIORITIES

Active NPC decision layers:

```text
1. immediate survival
2. direct command/operation
3. combat objective
4. role responsibility
5. personal goal
6. ambient behavior
```

Higher-priority needs interrupt lower ones.

---

# 67. SURVIVAL

NPCs should not behave suicidally unless context justifies it.

Inputs include:

```text
courage
loyalty
orders
enemy strength
injury
escape availability
```

---

# 68. FLEEING

Fleeing is valid behavior.

Faction members are not disposable combat robots.

A badly wounded low-courage member may retreat.

This gives personality and force preservation systemic meaning.

---

# 69. SURRENDER

Architecture may support:

```text
SURRENDER
```

for police/combat interactions.

Not required for first NPC implementation.

---

# 70. AI COMMANDS

Faction operations may give semantic commands:

```text
DEFEND location
ATTACK target
ESCORT actor
MOVE_TO territory
PROTECT actor
```

Physical AI translates them into local behavior.

---

# 71. AI DOES NOT KNOW MISSION OBJECTIVES

NPC AI should react to world reality.

Bad:

```text
NPC knows:
player mission says kill me
```

Correct:

```text
NPC knows:
player is attacking me
```

Mission system observes world events separately.

---

# 72. IMPORTANT NPC PROTECTION

Major NPCs receive canonical soft narrative immunity.

Low-level random simulation should not arbitrarily kill them.

This does NOT mean invincibility.

They may die through:

```text
player action
major faction operation
primary quest consequences
major narrative event
direct consequence of player decisions
```

---

# 73. PROTECTION IMPLEMENTATION

Do not use:

```text
important NPC HP = infinite
```

Instead, abstract simulation restricts invalid low-importance death outcomes.

If physically attacked by player:

```text
normal combat rules apply
```

unless a specific gameplay rule says otherwise.

---

# 74. SUCCESSION PARTICIPATION

Important NPC succession potential depends on:

```text
authority
Respect
ambition
loyalty
relationships
leadership ability
```

NPC system provides candidate state.

FactionSystem decides organizational succession.

---

# 75. BETRAYAL PARTICIPATION

NPC system owns personal factors:

```text
loyalty
ambition
relationship
memory
```

Faction/Story systems evaluate larger political opportunity.

No single system should independently invent betrayal.

---

# 76. NPC RESPECT CHANGE

Respect may rise from:

```text
successful operations
promotion
important victory
leadership
```

and fall from:

```text
failure
humiliation
loss
cowardice depending on context
```

Exact formula belongs with progression/social balancing.

---

# 77. RELATIONSHIP EVENTS

Systems should communicate meaningful events such as:

```text
NpcHelped
NpcThreatened
NpcBetrayed
NpcProtected
NpcAbandoned
```

RelationshipSystem converts them into social state changes.

Avoid unrelated systems directly editing five relationship values.

---

# 78. RELATIONSHIP AUTHORITY

Recommended:

```text
RelationshipSystem
```

owns rules for:

```text
trust
fear
hostility
loyalty
debt
```

NPC state stores the results.

---

# 79. ABSTRACT NPC OPERATIONS

Example:

```text
Npc #42 assigned to dealer protection
```

Outside active area:

```text
activity = FACTION_OPERATION
```

OperationSystem may resolve consequences.

NPC does not need a physical entity.

---

# 80. ABSTRACT COMBAT

Persistent NPC capabilities modify abstract combat.

Possible inputs:

```text
combat skill
equipment
injury
courage
operation strength
local advantage
```

Important NPC death restrictions still apply.

---

# 81. PLAYER MEETS ABSTRACT ACTOR

If player approaches an ongoing abstract operation:

```text
operation progress
+
NPC state
↓
materialize current situation
```

Do not restart the event from zero.

---

# 82. NPC CREATION

Persistent NPC creation requires:

```text
NpcId
identity
location
lifecycle
roles
traits
relationships defaults
```

Creation must be deterministic during world generation.

Runtime-created NPCs use deterministic runtime ID allocation.

---

# 83. NPC DELETION

Persistent NPC records should generally not be physically deleted merely because NPC died.

Why?

Their identity may remain referenced by:

```text
history
missions
relationships
story
succession
```

Instead:

```text
NpcLifecycle = DEAD
```

The semantic record remains.

---

# 84. TOMBSTONES

Dead historical NPCs effectively act as semantic tombstones.

They may retain only necessary data eventually.

V1 may retain complete `NpcState` for simplicity.

---

# 85. NPC STORE

Conceptually:

```cpp
class NpcStore
{
public:
    NpcId create(...);

    NpcState& get(NpcId);
    const NpcState& get(NpcId) const;

    bool exists(NpcId) const;
};
```

NPC IDs remain stable for world lifetime.

---

# 86. QUERY APIs

Useful semantic queries:

```text
membersOfFaction()
dealersInTerritory()
livingImportantNPCs()
successionCandidates()
NPCsWithRole()
NPCsAtLocation()
```

Do not duplicate authoritative membership lists unnecessarily.

---

# 87. NPC VALIDATION

Every persistent NPC must satisfy:

```text
valid NpcId
valid lifecycle
valid referenced faction or NONE
valid location or explicit unknown
valid roles
valid inventory reference if present
```

---

# 88. ACTIVE ENTITY VALIDATION

If:

```text
NpcState.activeEntity != NONE
```

then:

```text
entity exists
entity has NpcLink
NpcLink points back to same NpcId
```

---

# 89. DEAD NPC VALIDATION

Normally:

```text
DEAD NPC
```

cannot perform:

```text
operations
travel
dealer activity
leadership
```

Any transition window must be explicit.

---

# 90. ARRESTED NPC VALIDATION

Arrested actors cannot be simultaneously:

```text
physically operating dealer
actively attacking territory
```

unless escaped/released first.

---

# 91. AGGREGATE VALIDATION

Faction strategic manpower must reconcile:

```text
aggregate members
+
persistent generic members
+
physically instantiated members
```

without duplication.

---

# 92. DEBUG VIEW

Example:

```text
NPC #42
CARLO MORETTI

Lifecycle: ALIVE
Faction: #2
Authority: 5
Roles:
- Leader
- MissionIssuer

Respect: 91

Traits:
Aggression: 0.47
Ambition: 0.82
Risk: 0.31
Greed: 0.55
Courage: 0.73

Player relation:
Trust: 0.62
Fear: 0.14
Hostility: 0.08
Debt: 0.20

Activity:
MEETING

Location:
Building #817

Active Entity:
NONE
```

---

# 93. NPC HEADLESS TEST

Create:

```text
100 persistent NPCs
```

Simulate:

```text
30 game days
```

Validate:

```text
travel
activity
injury
arrest
relationships
faction membership
```

without renderer.

---

# 94. STREAMING TEST

Scenario:

```text
persistent NPC exists abstractly
↓
player approaches
↓
materialize
↓
interact
↓
player leaves
↓
abstractify
↓
save
↓
load
↓
materialize again
```

Identity and meaningful state must survive.

---

# 95. PROMOTION TEST

Scenario:

```text
spawn ephemeral civilian
↓
civilian witnesses crime
↓
promotion triggered
↓
NpcId allocated
↓
player leaves
↓
save/load
↓
same witness exists
```

---

# 96. DEATH TEST

Scenario:

```text
persistent NPC physically killed
↓
region unload
↓
world simulates
↓
region reload
```

Requirement:

```text
NPC remains DEAD
```

---

# 97. ABSTRACT CONSEQUENCE TEST

Scenario:

```text
important faction operation
↓
persistent member injured abstractly
```

When player encounters them:

```text
their physical state reflects injury
```

---

# 98. FIRST IMPLEMENTATION MILESTONE

Implement only:

```text
NpcId
NpcStore
NpcState
Lifecycle
Faction membership
World location
Persistent vs ephemeral
Materialization link
```

No personality AI yet.

---

# 99. SECOND MILESTONE

Add:

```text
roles
basic traits
relationships
activity
semantic travel
```

---

# 100. THIRD MILESTONE

Add:

```text
physical AI integration
promotion
generic faction members
aggregate conservation
```

---

# 101. FOURTH MILESTONE

Add:

```text
memory
social decisions
succession factors
betrayal factors
```

Only after basic systems prove useful.

---

# 102. DESIGN RULE

NPC simulation exists to make the world remember and react.

The target is NOT:

```text
simulate every human detail
```

The target is:

```text
make important people
have identities,
relationships,
goals,
consequences
and continuity.
```

---

# 103. NPC SYSTEM INVARIANTS

**N1** — Persistent NPC identity survives physical despawning.

**N2** — `NpcId` and `EntityId` remain separate.

**N3** — Death and despawn are never equivalent.

**N4** — Roles are compositional.

**N5** — Personality contains only gameplay-relevant traits.

**N6** — Respect, loyalty, fear and hostility are distinct concepts.

**N7** — Explicit relationships remain sparse.

**N8** — Ephemeral NPCs may be promoted.

**N9** — Promotion preserves current physical identity.

**N10** — Physical generic faction members consume real strategic manpower.

**N11** — NPC AI acts on world state, not mission-script knowledge.

**N12** — Important NPCs receive soft narrative immunity, not physical invulnerability.

**N13** — Dead persistent NPCs remain historically referenceable.

**N14** — NPC simulation works headlessly.

**N15** — Abstract and physical simulation must preserve semantic continuity.

---

# 104. STATUS

NPC persistent identity and simulation architecture are now defined.

The next specification is:

```text
08_MISSION_GRAPH_SPEC.md
```

It will define:

```text
Primary missions
Side missions
procedural opportunities
objective graphs
branching
expiration
NPC resolution
mutation
hidden issuers
failure
world consequences
story graph integration
```

without allowing missions to become the authority over world simulation.