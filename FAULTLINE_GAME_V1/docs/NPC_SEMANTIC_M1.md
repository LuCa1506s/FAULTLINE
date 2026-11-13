# NPC semantic simulation M1 (Phase 10)

NpcId is persistent identity; EntityId is an optional transient body. Generated
major NPC records stay frozen in the generated baseline. NpcState.semantic owns
runtime activity, home, optional journey, next update tick and one OperationId.
The existing lifecycle, membership, roles, traits, capabilities, current semantic
location/position and relationships remain their single sources of truth.

Normal creation initializes every living actor at current location, Idle, with
its next update in 30 game minutes (800 ticks). Decisions process ascending NpcId;
arrival is handled at its own exact deadline. Factions update first, NPCs then
consume the resulting operation states, and structural changes commit last.
The normal fixed-tick path and headless advancement run the same rules. A cached
minimum NPC deadline avoids scanning actors at 20 Hz; it is derived and rebuilt
from per-actor times on load. APIs that introduce an actor must initialize its
semantic schedule; raw domain stores remain low-level infrastructure.

Decision order: release finished assignment, complete travel, lifecycle gate,
explicit travel/assignment, faction responsibility, return home, ordinary role
activity. Living leaders coordinate remotely from base and do not wander.
Members may support an operation when courage + loyaltyBias + ambition/2 +
leadership/2 reaches 14000 basis points. Home defense outranks other defense,
which outranks attack; ties use OperationId. Dealers accept only defense with
courage >=5000 and otherwise work, or hide if courage <5000 and faction confidence
<3000. Other members may visit their leader with bounded 1/8 probability from the
persisted runtime NPC stream; they return home after the visit. No citizen
schedule, revenue, combat/pathfinding or major-NPC death is implemented.

Semantic travel stores origin/destination and departure/arrival ticks. Current
location remains origin until arrival (or explicit cancellation); completion
updates both domain location and normalized position. Endpoints must resolve
inside current generated geography, including building/territory coherence.
The estimate is Manhattan center distance times 1.25 at 120 meters/game minute,
rounded up to an integer tick with an 80-tick minimum. This is a conservative
semantic journey estimate over the existing connected city, not a road route or
collision trajectory. Building centers are exact half-meter values. Travel
requires Alive, no existing journey and no physical body. To avoid inventing
physical navigation/interpolation, materialization is deferred while travelling.
Restrictive lifecycle transitions cancel travel explicitly; Dead cannot revive.
Leadership-vacuum transitions require a future explicit leadership system.

Named NPCs are outside aggregate reserve/garrison/operation-force pools, as in
Phase 9; each counts once in total faction membership. Their operation link is
semantic coordination/support and grants no extra force, casualty roll, money
or aggregate commitment. Capability/traits influence willingness, not the Phase
9 combat formula. Consequently this phase intentionally does not rebalance
strategic pacing. A named actor can support only one operation; resolution or
cancellation clears the assignment and interrupts dependent travel. This cleanup
runs in the same tick as faction transitions. Direct low-level operation edits
must synchronize NPCs before the next validated save boundary.

RelationshipSystem is the sole runtime personal-relationship rule owner. Helped,
Threatened, WorkedWith and Abandoned events update separate directed trust, fear,
hostility, personal loyalty and debt dimensions, clamped to [0,10000]. Existing
edges retain their original context; meaningful new interactions create one edge
with neutral defaults and MeaningfulInteraction context. Finished operation
support produces WorkedWith toward the faction leader (or a living colleague
for the leader). There is no pairwise population matrix, decay database or
invented history. Faction diplomatic relationships remain unchanged.

EphemeralActor is a noncopyable, transient promotion token, without a persistent
NpcId. Promotion validates a candidate transaction, allocates through the existing
NpcStore high-water mark, preserves identity/traits/health/location and useful
stationary activity, initializes scheduling, and marks the token consumed.
An existing body receives NpcLink in place; neither entity ID nor components are
respawned. A faction actor must explicitly originate in reserve: exactly one
aggregate reserve member transfers to persistent membership, preserving total
manpower and existing operation commitments. Civilians do not consume faction
resources. Invalid sources, duplicate promotion, missing bodies, exhausted IDs
or invalid data leave the original state/allocator unchanged. Persistence saves
the promoted NPC and allocator, not the transient token/body.

Injury/arrest/missing/dead states restrict activity to Recovering/Arrested/Hiding/
Idle and disable ordinary scheduling, travel and participation. Current Phase 10
does not cause these lifecycle changes autonomously. Living generated actors
require valid semantic domain locations. Old foundation worlds without generated
geography may use their existing normalized WorldPosition without inventing a
district; they cannot start semantic travel without generated geography.

Validation covers endpoints, domain/entity backlinks, unique IDs, memberships,
living leaders, activity/lifecycle/assignment coherence, deadlines, bounded sparse
relationships and exact travel accounting (started=completed+cancelled+active).
Semantic event buffers and earliest-deadline indexes are transient/derived.
Config, per-NPC state, existing allocator/RNG and bounded counters are persistent;
see SAVE_FORMAT_V3.md. Event buffers do not become a historical log.
