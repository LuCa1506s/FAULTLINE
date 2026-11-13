# FACTION SIMULATION SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**File:** `06_FACTION_SIMULATION_SPEC.md`  
**Parent specs:** `01_ENGINE_ARCHITECTURE_SPEC.md` through `05_WORLD_GENERATION_SPEC.md`  
**Purpose:** Define how factions think, act, compete, cooperate, weaken, collapse and reform during runtime simulation.

---

# 1. CORE PRINCIPLE

A faction is not a scripted mission chain.

A faction is a persistent strategic actor.

It has:

```text
resources
people
territory
dealers
relationships
leadership
goals
threats
operations
history
```

Faction behavior must continue even when the player does nothing.

---

# 2. MAIN REQUIREMENT

The faction system must create visible causal chains.

Example:

```text
dealer killed
↓
local distribution weakens
↓
market share falls
↓
territorial presence falls
↓
rival sees opportunity
↓
rival operation starts
↓
territory becomes contested
↓
new missions appear
```

The system must not jump directly from:

```text
dealer killed
```

to:

```text
territory magically changes owner
```

---

# 3. STANDARD WORLD

A standard world begins with:

```text
5 main factions
```

The architecture must also support later:

```text
local gangs
player-created faction
reformed factions
```

without assuming permanently that exactly five faction objects exist.

---

# 4. FACTION STATE

Conceptual persistent structure:

```cpp
struct FactionState
{
    FactionId id;

    FactionLifecycle lifecycle;

    FactionFamily family;

    NpcId leader;

    Money treasury;

    float cohesion;

    float confidence;

    float aggression;

    float riskTolerance;

    float operationalCapacity;

    FactionStrategyState strategy;

    FactionDiplomacyState diplomacy;

    FactionHistory history;
};
```

Exact numeric representation remains tunable.

---

# 5. FACTION LIFECYCLE

Canonical lifecycle:

```text
ACTIVE
COLLAPSED
DORMANT
REFORMED
```

These are strategic states.

They are not physical entity states.

---

# 6. ACTIVE

`ACTIVE` means the faction currently possesses a meaningful operational structure.

Typically requires some combination of:

```text
members
leadership
money
dealers
territorial presence
operational capability
```

An active faction may temporarily own no territory and still remain active if it retains enough structure to recover.

---

# 7. COLLAPSED

A faction becomes `COLLAPSED` when it has lost meaningful criminal operation.

Canonical conditions include:

```text
no active members
no meaningful territory
no operational criminal structure
```

This must not be represented as:

```text
territory == 0
→ instant faction deletion
```

Collapse is a strategic outcome.

---

# 8. DORMANT

A collapsed faction may later transition to:

```text
DORMANT
```

if remnants remain.

Possible remnants:

```text
loyal former members
surviving important NPC
hidden money
historic identity
old territory support
```

Dormant factions do not compete normally.

They may still influence:

```text
relationships
story
local unrest
future reformation
```

---

# 9. REFORMED

A dormant/collapsed organization may return.

Reformation requires meaningful cause.

Example:

```text
former captain survives
+
old members still loyal
+
rival control weakens
+
money available
↓
FACTION REFORMED
```

A reformed faction is not reset to its original strength.

It starts from its surviving assets.

---

# 10. FACTION POWER IS NOT ONE NUMBER

Do not model faction strength solely as:

```text
power = 73
```

Strategic strength has several dimensions.

Initial model:

```text
Manpower
FinancialPower
TerritorialPresence
MarketPower
OperationalCapacity
LeadershipQuality
Cohesion
PoliticalPosition
```

A faction may therefore be:

```text
rich but weak militarily
```

or:

```text
poor but strongly entrenched locally
```

---

# 11. DERIVED POWER SCORE

A derived global score may exist for:

```text
AI comparisons
debugging
HUD summaries
balancing
```

Example concept:

```text
PowerScore =
weighted(
    manpower,
    money,
    territory,
    market,
    operationalCapacity,
    cohesion
)
```

But it is not authoritative truth.

Gameplay consequences should rely on the relevant dimensions.

---

# 12. MANPOWER

Faction manpower derives from actual semantic membership.

Categories may include:

```text
leadership
trusted members
soldiers
dealers
associates
generic local presence
```

Persistent named NPCs and aggregate members may coexist.

---

# 13. OPERATIONAL CAPACITY

`OperationalCapacity` represents how much activity the faction can sustain simultaneously.

Influenced by:

```text
available members
money
vehicles
weapons
leadership
current injuries/losses
ongoing operations
```

This prevents a faction with 10 people from launching 30 attacks simultaneously.

---

# 14. RESOURCE COMMITMENT

Operations reserve resources.

Example:

```text
Faction members available: 24

Operation A:
6 committed

Operation B:
5 committed

Protection:
4 committed

Available:
9
```

Committed resources cannot simultaneously perform unrelated strategic actions.

---

# 15. TREASURY

Faction money is persistent.

Money enters from systems such as:

```text
dealer income
market operations
missions
territorial activities
other abstract criminal revenue
```

Money exits through:

```text
operations
replacement resources
payments
bribes
member support
strategic investments
```

V1 does not need a full accounting simulator.

---

# 16. ECONOMIC ABSTRACTION

Faction economics should remain intentionally aggregated.

Avoid tracking:

```text
every meal
every bullet purchase
every salary
```

unless it creates meaningful gameplay.

Track strategic-scale costs.

---

# 17. COHESION

Cohesion measures organizational stability.

Conceptually:

```text
0.0 → internally fractured

1.0 → extremely cohesive
```

Influences:

```text
betrayal probability
obedience
succession stability
defection
internal conflict
reformation potential
```

---

# 18. COHESION CHANGES

Possible increases:

```text
major victory
strong leadership
successful defense
stable profits
shared enemy
```

Possible decreases:

```text
repeated losses
leader death
financial crisis
betrayal
territorial collapse
failed leadership
player treason
```

---

# 19. CONFIDENCE

Separate from cohesion.

`Confidence` represents perceived momentum.

Example:

```text
high cohesion
low confidence
```

could describe:

> a loyal faction that knows it is losing.

Confidence affects willingness to:

```text
attack
take risks
challenge stronger rivals
expand
```

---

# 20. FACTION PERSONALITY

Generated factions receive strategic traits.

Example:

```text
aggression
riskTolerance
territorialFocus
marketFocus
diplomaticPreference
retaliationBias
```

These shape decisions.

They must not override reality.

An aggressive faction with no resources cannot magically attack endlessly.

---

# 21. STRATEGIC GOALS

At any time, a faction may prioritize goals such as:

```text
SURVIVE
RECOVER
DEFEND
EXPAND_TERRITORY
EXPAND_MARKET
WEAKEN_RIVAL
PROTECT_DEALERS
INCREASE_CASH
STABILIZE_INTERNAL
RETALIATE
PREPARE_WAR
```

Multiple goals may exist with weights.

---

# 22. UTILITY-BASED DECISION MODEL

Recommended strategy selection:

```text
candidate action
↓
score usefulness
↓
score cost
↓
score risk
↓
score personality compatibility
↓
score current strategic goal
↓
select among viable actions
```

Conceptually:

```text
Utility =
Benefit
- Cost
- Risk
+ StrategicAlignment
+ PersonalityBias
```

Exact formulas remain tunable.

---

# 23. NO PURE RANDOM AI

Randomness may break ties or produce variation.

It must not replace evaluation.

Bad:

```text
random(attack, defend, expand)
```

Better:

```text
Attack Territory #7 utility = 0.72
Defend Territory #4 utility = 0.91
Expand dealers utility = 0.53
```

then controlled deterministic variation.

---

# 24. DECISION FREQUENCY

Faction strategy does not run at 20 Hz.

Use strategic intervals.

Initial baseline:

```text
minor evaluation:
every few game minutes

major strategy:
every few game hours

long-term planning:
daily
```

Exact timings remain configurable.

---

# 25. EVENT-DRIVEN REEVALUATION

Important events may request immediate reevaluation.

Examples:

```text
leader killed
territory lost
major dealer killed
war declared
player betrayal
treasury collapse
```

Do not wait hours for an obviously obsolete strategy.

---

# 26. OPERATIONS

Strategic decisions become explicit operations.

Examples:

```text
DEFEND_TERRITORY
ATTACK_TERRITORY
PROTECT_DEALER
ATTACK_DEALER
RECRUIT
EXPAND_MARKET
MOVE_MEMBERS
RETALIATE
SUPPORT_ALLY
SURVEIL_RIVAL
```

---

# 27. FACTION OPERATION STATE

Conceptually:

```cpp
struct FactionOperation
{
    OperationId id;

    FactionId faction;

    OperationType type;

    OperationStatus status;

    OperationTarget target;

    ResourceCommitment commitment;

    GameTime startTime;

    GameTime expectedResolution;
};
```

---

# 28. OPERATION STATES

Suggested lifecycle:

```text
PLANNED
PREPARING
ACTIVE
RESOLVED
FAILED
CANCELLED
```

This is internal strategic state.

Not every operation becomes a player mission.

---

# 29. PLAYER MISSION EMERGENCE

Operations may generate opportunities.

Example:

```text
Faction plans dealer defense
↓
player is trusted enough
↓
MissionSystem may generate:
"Protect dealer"
```

If player ignores it:

```text
operation still happens
```

This is critical.

The world must not freeze because the player ignored a mission.

---

# 30. NPC RESOLUTION

If no player participates:

```text
FactionOperation
↓
abstract resolution
```

using actual committed resources and local conditions.

Possible result:

```text
success
partial success
failure
unexpected escalation
```

---

# 31. PLAYER PARTICIPATION

If the player enters a relevant active region:

```text
abstract operation
↓
materializes into physical gameplay
```

Player actions can alter the outcome.

The same operation should not be independently resolved abstractly while physical combat is occurring.

---

# 32. OPERATION OWNERSHIP

One system owns operation resolution:

```text
FactionSystem / OperationSystem
```

MissionSystem observes and hooks into it.

MissionSystem must not become the authority over faction reality.

---

# 33. TERRITORIAL PRESSURE

Factions influence territories through real presence.

Factors include:

```text
members
dealers
local operations
market share
resident support/fear
rival presence
police pressure
```

Territory ownership does not switch merely because an attack mission succeeds.

---

# 34. TERRITORY ATTACK

A territorial attack may:

```text
remove rival members
kill dealer
damage rival operational capacity
increase attacker presence
reduce defender confidence
```

Then:

```text
TerritorySystem
```

re-evaluates control.

---

# 35. TERRITORY DEFENSE

Defense aims to maintain or increase:

```text
local faction presence
dealer security
strategic positions
```

Successful defense may increase:

```text
confidence
cohesion
local control
```

---

# 36. CONTESTED TERRITORIES

A territory becomes contested when no single faction has sufficient control.

Example:

```text
Faction A presence: strong
Faction B presence: meaningful
```

Result:

```text
CONTESTED
```

This creates fertile conditions for:

```text
missions
violence
price instability
police pressure
```

---

# 37. FACTION PRESENCE

Faction presence should be derived from semantic actors and operations.

Conceptual inputs:

```text
active dealers
members
safehouses
operations
market control
historic entrenchment
```

Avoid arbitrary:

```text
presence += 10
```

without causal source.

---

# 38. MARKET STRATEGY

Factions may prioritize drug-market expansion separately from territory.

Possible actions:

```text
add dealer
protect dealer
undercut rival
increase supply
move supply
attack rival distribution
```

Exact market mechanics belong to the Market spec.

---

# 39. DEALER IMPORTANCE

Dealers should have strategic value.

Dealer loss may affect:

```text
income
market presence
territory pressure
confidence
mission generation
```

Important dealers may trigger stronger retaliation.

---

# 40. DIPLOMACY

Faction relationships require persistent state.

Conceptual dimensions:

```text
trust
hostility
fear
sharedInterest
historicalGrievance
```

Again, avoid one generic diplomacy number if multiple dimensions matter.

---

# 41. DIPLOMATIC STATES

Derived readable states may include:

```text
ALLIED
FRIENDLY
NEUTRAL
TENSE
HOSTILE
WAR
```

These are derived from underlying relations/history.

---

# 42. ALLIANCES

Alliance formation should require strategic reason.

Examples:

```text
shared rival
mutual weakness
market cooperation
historical relationship
temporary necessity
```

Alliances can be unstable.

---

# 43. ALLIANCE COST

Alliance should constrain behavior.

Example:

```text
Faction A allied with B
```

should reduce willingness to:

```text
attack B
undermine B
support B's enemies
```

unless betrayal logic overrides it.

---

# 44. BETRAYAL

Betrayal probability may depend on:

```text
trust
strategic benefit
cohesion
leadership traits
past grievances
current weakness
```

It should not be random narrative decoration.

---

# 45. WAR

War is a high-level diplomatic state.

War increases utility for:

```text
territorial attacks
dealer attacks
retaliation
resource commitment
```

and reduces normal cooperation.

War should consume resources.

---

# 46. WAR EXHAUSTION

Long conflicts should create costs.

Possible effects:

```text
money drain
member losses
cohesion loss
market disruption
police pressure
public suspicion
```

This prevents endless consequence-free wars.

---

# 47. WAR TERMINATION

War may end because of:

```text
ceasefire
leader change
resource exhaustion
major territorial shift
common threat
one faction collapse
story consequence
```

Not every war must continue until extermination.

---

# 48. RETALIATION MEMORY

Factions must remember meaningful attacks.

Examples:

```text
leader killed
dealer killed
territory attacked
player betrayal
```

Persistent grievance may increase future retaliation utility.

---

# 49. PLAYER REPUTATION WITH FACTIONS

Player Respect is global, but faction perception must also depend on:

```text
relationship
loyalty
history
recent actions
betrayals
favors
```

Global Respect alone must not determine whether a faction likes the player.

---

# 50. PLAYER AS FACTION MEMBER

When player belongs to a faction:

```text
player actions
```

may affect:

```text
faction confidence
relations
territories
market
leadership opinion
```

But player does not automatically control strategy at low rank.

---

# 51. LEADERSHIP AUTHORITY

Around the canonical leadership breakpoint:

```text
~LV65
```

the player may issue strategic orders.

Examples:

```text
send members
defend dealer
attack rival dealer
increase presence
defend territory
support operation
```

This adds commands into the same faction operation system.

---

# 52. PLAYER ORDERS ARE NOT GUARANTEED

An order must respect available resources.

Example:

```text
Player:
Attack Territory #12
```

Faction system checks:

```text
authority
available members
current operations
resources
target validity
```

A faction cannot execute impossible orders just because the player clicked them.

---

# 53. ORDER CONSEQUENCES

Poor leadership decisions may produce:

```text
member losses
money loss
territory loss
confidence loss
respect loss
internal dissatisfaction
```

Leadership has actual cost.

---

# 54. PLAYER-CREATED FACTION

Player-created organizations use the same faction system.

Do NOT create a separate simplified `PlayerFaction`.

A new player faction begins with:

```text
little/no territory
limited members
limited resources
high external threat
```

according to canonical design.

---

# 55. RECRUITMENT

Faction manpower recovery comes from recruitment.

Recruitment depends on:

```text
money
reputation
territorial influence
local population
faction identity
success
leadership
```

Recruitment takes game time.

It is not instantaneous regeneration.

---

# 56. LOSSES MATTER

Member deaths and arrests must reduce actual capacity.

A faction should not magically return to full strength after every encounter.

Recovery mechanisms:

```text
recruitment
release
healing
returning members
alliances
```

---

# 57. NAMED NPC LOSSES

Important NPC deaths cause stronger structural consequences.

Possible results:

```text
succession
cohesion drop
strategy change
betrayal
split
mission generation
```

---

# 58. SUCCESSION

Leader death triggers succession logic.

Possible inputs:

```text
authority
loyalty
ambition
relationships
faction structure
cohesion
```

Possible outcomes:

```text
clean succession
internal dispute
temporary vacuum
split
collapse
```

---

# 59. SOFT NARRATIVE IMMUNITY

Background faction simulation must respect the canonical protection of important NPCs.

Random low-level strategic resolution should not arbitrarily kill major story-relevant NPCs.

Their death requires appropriate causal strength such as:

```text
major operation
narrative consequence
direct player action
high-impact event
```

---

# 60. INTERNAL FACTION TENSIONS

A faction may contain internal conflicts.

Potential sources:

```text
low cohesion
ambitious lieutenant
financial weakness
failed leadership
player favoritism
succession dispute
```

These can create:

```text
betrayal
defection
subfaction
internal mission
```

---

# 61. SUBFACTIONS

Subfactions remain subordinate structures within a main faction.

They may possess:

```text
local members
territory
leadership
identity
```

but strategic behavior is usually constrained by the parent faction.

Strong internal conflict may later produce splits if design requires.

---

# 62. LOCAL GANGS

Civilian unrest may generate local gangs.

Initial properties:

```text
very low Respect
few members
weak weapons
local territory focus
```

They use a reduced configuration of the same faction simulation architecture.

---

# 63. LOCAL GANG GROWTH

Local gangs may:

```text
recruit
gain territory
gain dealers
fight main factions
```

but are initially constrained in scope.

Canonical target maximum Respect is around:

```text
LV50
```

unless later design revision changes it.

---

# 64. STRATEGIC INFORMATION

Faction AI should not automatically know every hidden fact.

Each faction may maintain a strategic knowledge model.

Examples:

```text
known rival presence
suspected dealer location
known territory strength
recent attacks
```

V1 may simplify this substantially.

But architecture should avoid omniscience where hidden information matters.

---

# 65. KNOWLEDGE AGE

Some information may become stale.

Example:

```text
Faction thinks rival has 8 members
```

but reality changed.

This creates:

```text
ambushes
miscalculations
risk
```

Potential advanced system; not required for first implementation.

---

# 66. ABSTRACT COMBAT RESOLUTION

When physical simulation is unnecessary:

```text
attack force
vs
defense force
```

produces an abstract result.

Inputs may include:

```text
manpower
equipment
leadership
local presence
surprise
terrain/context
morale/confidence
```

---

# 67. ABSTRACT COMBAT MUST PRODUCE ACTUAL CONSEQUENCES

Result may include:

```text
injuries
deaths
resource loss
retreat
territorial pressure changes
confidence changes
```

Not merely:

```text
winner = A
```

---

# 68. DETERMINISTIC RANDOMNESS

Faction decisions use dedicated runtime RNG stream.

Example:

```text
RuntimeRandomState.factions
```

Randomness affects:

```text
tie-breaking
uncertainty
combat variation
betrayal timing
```

but all outcomes remain reproducible after save/load.

---

# 69. SYSTEM ORDERING

Recommended strategic pipeline:

```text
1. process major events

2. update faction conditions

3. resolve completed operations

4. update diplomacy

5. evaluate threats/opportunities

6. select new goals

7. create operations

8. reserve resources

9. emit domain events

10. Territory/Market/Mission systems react
```

Exact scheduling may evolve.

---

# 70. CROSS-SYSTEM BOUNDARIES

FactionSystem owns:

```text
strategy
operations
resource commitment
diplomacy decisions
faction lifecycle
```

TerritorySystem owns:

```text
territory control state
presence evaluation
```

MarketSystem owns:

```text
supply
demand
prices
market share
```

MissionSystem owns:

```text
player-facing mission representation
```

Do not duplicate authority.

---

# 71. EXAMPLE — DEALER KILLED

```text
NpcKilled(dealer #81)
↓
Dealer role becomes inactive
↓
MarketSystem updates distribution
↓
FactionSystem registers loss
↓
confidence decreases
↓
retaliation utility increases
↓
local territorial presence weakens
↓
rival evaluates expansion
↓
operation created
```

This is the target systemic behavior.

---

# 72. EXAMPLE — PLAYER IGNORES DEFENSE MISSION

```text
Faction operation:
DEFEND DEALER

Mission offered to player
↓
player ignores mission
↓
mission expires
↓
faction operation continues
↓
NPC participants resolve event
↓
dealer may survive or die
↓
world changes
```

This satisfies:

```text
world does not wait for player
```

---

# 73. EXAMPLE — RIVAL COLLAPSE

```text
Faction B loses dealers
↓
income collapses
↓
recruitment slows
↓
territory presence falls
↓
members defect
↓
leader killed
↓
cohesion collapses
↓
remaining operations fail
↓
FactionLifecycle = COLLAPSED
```

Not:

```text
player completes "Destroy Faction"
↓
delete faction
```

---

# 74. RECOVERY

A weakened faction may recover through:

```text
recruitment
safe territory
new dealer network
alliance
leadership change
successful retaliation
```

Recovery should be possible without player intervention.

---

# 75. ANTI-SNOWBALLING

The simulation needs mild stabilizing forces.

Otherwise one faction may win automatically every seed.

Possible systemic stabilizers:

```text
larger territory costs more to defend
wars consume resources
rivals cooperate against dominant faction
police pressure increases on dominant criminal activity
long supply lines weaken expansion
```

These must remain believable consequences, not arbitrary rubber-banding.

---

# 76. NO FAKE RUBBER-BANDING

Avoid:

```text
if faction strongest:
    damage *= 0.5
```

or:

```text
weakest faction gets free money
```

Stabilization should emerge through actual systems.

---

# 77. DOMINANT FACTION RESPONSE

When one faction becomes extremely dominant:

```text
other factions
```

may perceive an existential threat.

This increases utility for:

```text
alliances
ceasefires
joint opposition
defensive strategy
```

---

# 78. PLAYER IMPACT

Player actions should be capable of breaking equilibrium.

The simulation must not always restore balance.

If the player creates overwhelming advantage:

```text
city domination
```

must remain possible.

Anti-snowballing slows natural runaway outcomes.

It does not invalidate player success.

---

# 79. FACTION HISTORY

Important faction events are recorded.

Examples:

```text
leader changed
war declared
major territory lost
alliance formed
player betrayal
faction collapsed
faction reformed
```

History supports:

```text
AI memory
story
dialogue
missions
debugging
```

---

# 80. EVENT LOG SIZE

Do not store every tiny strategic update forever.

Use:

```text
important historical events
```

plus optionally bounded recent-event history.

---

# 81. FACTION DEBUG VIEW

Development tools should expose:

```text
Faction #3

Lifecycle: ACTIVE
Leader: NPC #42

Money: 84,200
Members: 37
Committed: 14

Cohesion: 0.61
Confidence: 0.48

Current goals:
DEFEND 0.82
RECOVER 0.71
RETALIATE 0.64

Operations:
#17 DEFEND Territory #4
#18 ATTACK Dealer #81

Relations:
Faction #1 HOSTILE
Faction #2 TENSE
Faction #4 ALLIED
Faction #5 WAR
```

---

# 82. HEADLESS FACTION SIMULATION

Required test:

```text
generate world
↓
disable player
↓
simulate 365 game days
↓
validate
```

World must remain structurally valid.

---

# 83. LONG-RUN TEST

Example:

```text
10,000 seeds
×
5 simulated years
```

Check:

```text
no invalid faction IDs
no impossible negative manpower
no permanent operation leaks
no instant universal collapse
no infinite money
no dead leader references
```

---

# 84. EQUILIBRIUM TESTING

Measure:

```text
average faction lifespan
wars per year
territory turnover
market turnover
collapse frequency
reformation frequency
```

We should not hardcode target values yet.

These statistics are for balancing.

---

# 85. PLAYERLESS WORLD TEST

A world without player intervention should still generate:

```text
territorial change
alliances
wars
dealer losses
leadership changes
market movement
```

but should not generally self-destruct instantly.

---

# 86. DETERMINISM TEST

Given:

```text
seed
same runtime RNG state
same initial faction state
same simulation duration
```

result must be identical.

---

# 87. OPERATION VALIDATION

Every operation must reference:

```text
valid faction
valid target
valid participants/resources
valid state
```

Impossible operations are rejected before activation.

---

# 88. RESOURCE VALIDATION

Invariant:

```text
committed manpower
<=
available manpower
```

Likewise for money/resources.

No double-commitment.

---

# 89. COLLAPSE VALIDATION

A collapsed faction cannot continue normal offensive operations.

Outstanding operations are:

```text
cancelled
resolved
converted
```

according to context.

---

# 90. LEADER VALIDATION

An active faction must have either:

```text
valid leader
```

or:

```text
explicit leadership-vacuum state
```

No silent invalid `NpcId`.

---

# 91. INITIAL IMPLEMENTATION SCOPE

Do not implement every advanced feature immediately.

### Milestone 1

```text
FactionState
manpower
treasury
cohesion
territory awareness
basic utility decisions
```

Operations:

```text
DEFEND_TERRITORY
ATTACK_TERRITORY
```

---

# 92. MILESTONE 2

Add:

```text
dealers
market-aware strategy
recruitment
resource commitment
```

Operations:

```text
PROTECT_DEALER
ATTACK_DEALER
RECRUIT
```

---

# 93. MILESTONE 3

Add:

```text
diplomacy
alliances
war
retaliation memory
```

---

# 94. MILESTONE 4

Add:

```text
leadership succession
collapse
dormancy
reformation
```

---

# 95. MILESTONE 5

Add:

```text
player leadership orders
player-created faction
local gang integration
```

---

# 96. FIRST PLAYABLE STRATEGIC TEST

Starting world:

```text
5 factions
5 home territories
basic members
basic money
```

Simulate:

```text
30 game days
```

Expected:

```text
factions choose actions
resources become committed
operations resolve
territorial pressure changes
some relations change
world stays valid
```

No renderer required.

---

# 97. FIRST PLAYER-IMPACT TEST

Scenario:

```text
player kills important dealer
```

Then verify:

```text
dealer removed from operation
market affected
faction notices loss
strategy changes
territory pressure changes
mission opportunities may appear
```

This is one of the most important end-to-end system tests.

---

# 98. DESIGN RULE

Faction AI exists to generate:

```text
meaningful consequences
conflict
opportunities
world motion
```

Not to simulate organized crime at maximum possible complexity.

Every additional variable must justify itself through gameplay.

---

# 99. FACTION SYSTEM INVARIANTS

**F1** — Factions act even without player participation.

**F2** — Factions cannot spend unavailable resources.

**F3** — Strategic operations reserve actual capacity.

**F4** — Territory is influenced through actual presence and consequences.

**F5** — Missions do not determine faction reality.

**F6** — Ignored missions may resolve through NPC simulation.

**F7** — Important losses have persistent strategic impact.

**F8** — Factions may collapse without being deleted.

**F9** — Dormant factions may reform when valid conditions exist.

**F10** — Player-created faction uses the same fundamental simulation.

**F11** — Strong factions receive no arbitrary penalties.

**F12** — Weak factions receive no arbitrary free resources.

**F13** — Diplomacy has causes and consequences.

**F14** — Major decisions are reproducible under deterministic runtime RNG.

**F15** — Faction simulation remains viable headlessly.

---

# 100. STATUS

The faction simulation architecture is now defined.

The next specification should be:

```text
07_NPC_DATA_MODEL_AND_SIMULATION_SPEC.md
```

It must define:

```text
persistent NPC data
personality
loyalty
respect
relationships
roles
AI abstraction
daily activity
important vs generic NPCs
promotion
injury
death
arrest
succession participation
social decisions
```

before Mission and Story systems depend heavily on NPC behavior.