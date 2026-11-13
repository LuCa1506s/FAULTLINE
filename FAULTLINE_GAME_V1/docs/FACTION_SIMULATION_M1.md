# Phase 9 / aggregate faction simulation

The implementation is purpose-built, single-threaded and headless. It owns
decisions, operation resource allocation and aggregate results. It creates no
entities or NPCs, changes no diplomacy, and never writes territory ownership.
The canonical sources remain in the parent directory and are unchanged.

## Resources and ownership

Existing `FactionState::treasury`, `reserveMembers`, `cohesion`, personality and
`TerritoryState::residentMembers` remain authoritative. New strategic state adds
confidence (integer basis points), evaluation count/time, next attack time and the
last chosen target/utility. No duplicate authoritative manpower/capacity counters
or per-faction operation vectors exist.

`FactionOperationStore` belongs to GameState's `FactionSimulationState`, with
typed monotone OperationIds. Preparing/Active operations own their committed
aggregate members; launch removes those members from unallocated reserves.
Completion/cancellation returns survivors exactly once. Mobilization money is
paid once at launch and is consumed even if the operation is cancelled.

Resource queries derive:

* total manpower = unallocated reserves + live operation commitments + resident
  garrisons + living named faction NPCs;
* committed manpower = Preparing/Active operation forces;
* available manpower = total minus committed;
* deployable manpower = unallocated reserves only. Named NPCs and fixed garrisons
  are not a second pool of free mobile soldiers;
* operational capacity = minimum of configured slots (default 2), mobile pool
  divided by minimum force, and financial capacity. The latter counts remaining
  treasury and mobilization already paid for live operations, using conservative
  integer divisions by the minimum operation cost.

Commitment and capacity therefore cannot be increased just by adding an index
entry. Operation status/resources are the authority; pending-ID/deadline sets
are rebuildable indexes. Initial reserves/garrisons are those generated in M3.
There is no recruitment, passive income, collapse or named-NPC casualty loop.

## Integer scheduling

At 2250 game milliseconds per fixed tick, a game hour is exactly 1600 ticks and
a day is 38,400 ticks. Default configuration:

| Parameter | Game time | Ticks |
|---|---|---|
| Strategic evaluation | every 4 hours | 6400 |
| Attack preparation | 2 hours | 3200 |
| Attack active duration | 6 hours | 9600 |
| Defense preparation | 1 hour | 1600 |
| Attack cooldown from launch | 12 hours | 19200 |

One persistent next-evaluation tick schedules all factions in ascending FactionId
order. Operations advance in ascending OperationId order at due boundaries.
The normal 20 Hz tick only checks the next due boundary; it does not evaluate
strategy at 20 Hz. Resolution precedes new decisions on the same tick.
No wall clock or float accumulator enters the rules. Configurable intervals are
positive and bounded; scheduling beyond SimulationClock's maximum uses the
explicit `neverTick` sentinel or rejects a launch that cannot finish.
An earlier-ID faction's newly preparing operation can be observed by a later-ID
faction on the same evaluation boundary; the ordering is explicit and stable.

`Simulation::advanceHeadless` skips only idle ticks, then uses the normal `tick`
pipeline for every scheduled mutation. It accepts no input and requires no
physical entities. It is an explicit current-runtime API, not permission for
future systems to skip their schedules. Fixed versus coarse advancement is tested
for equal final StateHash, bytes and event sequence count.

## Targeting and utility

Adjacency reuses the existing territory-boundary topology, independently of
streaming cells. It is derived once and rebuilt after load. Supported generator
topology limits are checked before rebuilding the quadratic adjacency index from
untrusted saves (at most 3 districts per macro-zone and the configured territory
limit per district for generators 1..3).

An attack needs a valid adjacent foreign/neutral territory, an eligible faction,
resources, an unused target, capacity and elapsed cooldown. Only one live attack
can target a territory at a time. A defense must reference one actual pending
rival attack on an owned territory; one defense can bind that threat. Safe
territories generate no defense candidates. Mobilized force is capped by the
configured bounds, reserves, personality baseline and actual affordability.

Attack utility is an integer sum of baseline benefit, aggression, territorial
focus, confidence/cohesion, target building value and relative strength, minus
cost and risk. Directed hostility increases preference; trust/shared interest
reduce it. Defense utility strongly prioritizes a real threat, especially to the
home territory. Nonpositive defense and below-threshold attack utilities are
rejected. Highest utility wins, with ties ordered by type, TerritoryId and threat
OperationId. AI choices consume no randomness; faction RNG is reserved for bounded
resolution uncertainty. Unaffordable larger forces are reduced to a viable
smaller force rather than blocking a feasible operation.

Coefficients/costs are initial architectural-test tuning, not final balancing.
Default minimum/maximum force: 4/12. Mobilization costs: attack 600 + 75/member,
defense 300 + 75/member. Minimum attack utility: 1500.

## Lifecycle and aggregate outcomes

Legal transitions:

```text
PREPARING -> ACTIVE -> RESOLVED / FAILED / CANCELLED
PREPARING -> CANCELLED
```

Terminal operations cannot restart/cancel again. They retain immutable outcome
records for causal inspection. Defense shares its attack's resolution deadline;
cancelled threats cancel their responses. Changed target claims/ineligible
launching factions cause cancellation at a synchronization boundary.

Resolution uses committed attacker force versus current garrison plus the
specifically bound active defense force. Quality is `5000 + cohesion/4 +
confidence/4`; garrisons receive a 20% integer-rounded entrenchment multiplier.
Each side receives a faction-stream draw in [90%, 110%]. Greater effective
attacker strength succeeds. Aggregate casualty draws are small and bounded by
their force/garrison; survivors return to reserves. Victory raises confidence by
120 and cohesion by 25 basis points; defeat reduces them by 160/30, clamped to
[0, 10000]. A defense result mirrors the associated battle rather than rerolling
or applying morale twice. No major NPC participates in aggregate casualty rolls.

Successful attacks record a bounded strategic-pressure result backed by the
operation's committed force. This is evidence for the future TerritorySystem,
not an accumulated ownership score or a second control system. Garrison damage
can exhaust the old garrison while ownership stays unchanged: the old claim is
left for Phase 11 to reevaluate. Runtime validation permits an owned exhausted
garrison but forbids a neutral territory carrying a faction garrison. Initial
generated-world validation is unchanged and remains stricter.

Scoped transient events are Created, Started, Resolved, Failed and Cancelled,
with operation/faction/opponent/territory IDs, tick and outcome. Simulation owns
the ordered faction event buffer; no global bus or gameplay file I/O exists.

## Source interpretation and limits

Faction specification section 14 gives 24 members with 15 committed and 9 left.
Section 88 writes `committed manpower <= available manpower`; interpreted as
remaining uncommitted members, that would contradict its own example. This phase
uses the user's explicit invariant: `0 <= committed <= total`, with
`available = total - committed`; launch additionally requires real deployable
reserves. This preserves both the GDD's real resources and section 14's example.

Broad future faction/roadmap examples also mention control/diplomacy/market
changes. The session explicitly limits Phase 9 to operations and resource effects;
those future authorities are not implemented here. Configuration is persisted per
session; changing operation cost definitions with historical operations requires
a compatible migration rather than editing saved receipts silently.

Without recruitment/revenue/control reevaluation, long runs naturally exhaust
some resources. Zero-resource factions remain semantic records; lifecycle and
reformation are deliberately deferred. Strategic knowledge currently uses public
territory/garrison data, without a hidden-information model. All operations are
aggregate; physical operation materialization is outside Phase 9.

CLI operation totals cover all stored operations, including history loaded from
a save. Resource losses/spending and the monthly creation rate describe the
current invocation's interval; commitment peak includes commitments at entry.
