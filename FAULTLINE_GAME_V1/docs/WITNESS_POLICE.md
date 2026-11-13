# Witness and Police

Phase 20 implements the authoritative chain `Combat fact -> CrimeEvent -> witness
perception -> report/direct observation -> police knowledge -> Wanted FSM -> police
response`. No stage writes through another stage's authority.

## Crime and perception

`CrimeEventId` and `CrimeReportId` are monotonic typed IDs. A crime records its
type, generic actor and victim (`Unknown`, `Player`, or persistent `NpcId`), normalized
cell-aware position, tick, severity, and visual/audible observability. Combat events
are ingested once by stable `(tick,event index)` source identity; CombatSystem never
edits Wanted state.

Witness discovery occurs only when a new crime is processed. SpatialGrid supplies a
bounded local candidate set, which is deduplicated and sorted by `NpcId`. Eligible
witnesses must have a live/injured persistent NPC, a legitimately active physical
body, be neither actor nor victim, and perceive the event through cell-aware range
and authoritative static geometry. Camera, renderer, Map2D, and HUD state are never
read. Visual range is 35 m and audible range is 50 m in PoliceStateVersion 1.

Civilian observations schedule a report 60 simulation ticks later. The report is a
persistent semantic record keyed by witness `NpcId`; abstraction does not erase it.
Death or another restrictive lifecycle before completion interrupts it. Police
observation is explicitly marked direct and enters knowledge immediately without a
fictional civilian report. Multiple reports add evidence/heat but police knowledge
has one record per crime and does not duplicate the primary escalation.

## Wanted state machine

The canonical high-level states are represented directly:

| State | Normal exits |
| --- | --- |
| `CLEAR` | `SUSPICION`, `REPORTED`, `LOCATED` |
| `SUSPICION` | `CLEAR`, `REPORTED`, `LOCATED` |
| `REPORTED` | `SEARCHING`, `LOCATED` |
| `SEARCHING` | `REPORTED`, `LOCATED`, `COOLDOWN` |
| `LOCATED` | `PURSUIT`, `ESCAPED_PENDING` |
| `PURSUIT` | `ESCAPED_PENDING` |
| `ESCAPED_PENDING` | `LOCATED`, `SEARCHING`, `COOLDOWN` |
| `COOLDOWN` | `CLEAR`, `REPORTED`, `LOCATED` |

Self-transitions are idempotent; every other edge is rejected. Reports lead to a
one-tick `REPORTED` acknowledgement and then `SEARCHING`. Direct sight leads to
`LOCATED`, then `PURSUIT`. Forty ticks without valid officer sight during pursuit
leads to `ESCAPED_PENDING`; after 100 more ticks it becomes `SEARCHING`. Search lasts
600 ticks and cooldown 5400 ticks, so complete hidden search/decay is 6000 ticks,
the canonical approximately five real-time minutes at 20 Hz. Suspicion expires in
200 ticks. These integer values are versioned PoliceStateVersion 1 policy because
the canonical specification defines behavior and approximate real duration, not a
complete numeric transition table.

Police knowledge separately stores crime identity, suspect attribution, reported
position, source/tick, and whether observation was direct. Wanted stores heat,
identification, last-known position, last-confirmed tick, deadline, and confidence
radius. Active officers may refresh last-known position only through range plus
static LoS. Hidden movement never updates it.

## Response, streaming, and arrest

Response size is derived deterministically from heat. Officers are persistent NPCs
with `NpcRole::PoliceOfficer` and stable `NpcId`; their physical `EntityId` remains a
streaming-owned lease. Abstract dispatch has an 80-tick travel deadline. Once active,
search and pursuit produce normal Character AI intents and use the shared character
movement/collision code. Police attack intent flows through CombatSystem; Character
AI and PoliceSystem do not mutate Health.

At close range an officer may arrest a living player whose Health is at or below 25.
Arrest releases response, advances the same authoritative simulation by exactly 120
game days, then recreates the player. The generated world currently has no semantic
police-station object; release therefore uses a deterministic point just inside the
arrest district boundary as the station-exterior proxy. There is no transport,
cinematic, vehicle, or handcuff system.

Known reported crime adds bounded district police pressure and calls the existing
MarketSystem disruption hook. PoliceSystem owns that consequence; missions and
StoryGraph do not manufacture police truth.

## Tick order

Within an authoritative tick: residency restoration; strategic systems and commits;
player movement/streaming; ordinary Character AI; existing report/FSM deadlines;
police response movement and fresh officer perception; CombatSystem; combat-fact
crime ingestion; witness perception; report/FSM processing for new facts; downstream
death observation; interaction intent; clock advancement; pending arrest resolution.
A newly reported response therefore first acts on the following physical tick.

Transient event buffers, candidate arrays, and geometry scratch are not saved or
hashed. Crime/report allocators, processed combat identities, observations, reports,
knowledge, Wanted context/deadlines, responders, district pressure, arrest state,
and rules are persistent and included in StateHash v15. Diagnostic counters are
neither serialized nor hashed.

