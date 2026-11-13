# Player Physical Controller (Phase 17)

The controller consumes semantic `PlayerInputFrame` through `Simulation::tick`.
There are no hardware keycodes, camera rules, rendering dependencies or wall-clock
movement. Player creation is explicit through `createPlayerEntity`; completing
FIRST DELIVERY itself remains a headless Phase 16 operation. An application can
create the body at the origin leader's semantic position after the prologue.

## Identity and authority

`PlayerStateVersion=1`. `PlayerState.controlledEntity` identifies one persistent
physical entity, distinct from persistent `NpcId`. The body has Transform, Health
and CharacterBody. Transform/WorldPosition is the position authority. CharacterBody
owns facing, movementSpeed (default 5 m/s), and stanceHeight (1.75 m).
`PlayerState.yaw` is a synchronized facing cache retained from the inherited API:
movement writes body facing first and copies it; validation rejects disagreement.
It is not an independent heading source. No NPC/vehicle link may alias the player.

## Movement and collision

One physical tick is 50 ms, so full cardinal motion is 0.25 m/tick. Accelerated
game-time milliseconds are deliberately NOT used for physical movement. World-axis
X/Z movement preserves analog magnitude below one and clamps larger vectors to
unit length; diagonals cannot move faster. A nonzero aim vector selects yaw using
atan2(X,Z); otherwise nonzero movement selects facing, otherwise heading stays.
The signed heading interval is [-pi,pi].

Movement retains cell + local float coordinates and existing normalization, with
signed and multiple-cell crossings. Shared world-layer `moveCharacter` supplies
kinematic X-then-Z sliding against swept, radius-expanded building/water rectangles
(horizontal radius 0.35 m); it clips motion at walls rather than relying only on
endpoint overlap. World bounds constrain actors starting within those bounds.
An actor already inside a building may exit: interiors/doors are not implemented.
Legacy/foundation positions outside generated bounds are not teleported inside.

Interaction presses produce a scoped, one-tick `PlayerInteractionIntent` containing
actor, transform, yaw and tick. They do not grant items/rewards or complete mission
objectives implicitly. Game clears pressed inputs after one tick. Camera/snapshot
extraction and target-specific gameplay consumers are outside this foundation.

## Streaming and deterministic continuation

Streaming commits at the structural synchronization boundary after movement,
using the existing 512 m activation / 640 m deactivation hysteresis. Ordinary
transient NPC bodies are still omitted from semantic saves; Transform/Health sync
into NpcState, and the spatial index is rebuildable.

One piece of streaming state cannot be discarded as a cache: logical residency
and its already allocated handle reservation. In the current runtime, residency
determines hysteresis and whether NPC rules use semantic or physical activity.
`PlayerState.residents` records ordered `(NpcId, EntityId allocation lease)` pairs.
The semantic identity remains NpcId. The reserved handle identifies no new NPC.
These leases are hashed and persisted; they contain no NPC body/component data.

After load, NPCs initially remain abstract. BEFORE any tick subsystem inspects
materialization, `restorePlayerResidency` rebuilds leased bodies and back-links
from authoritative NpcState using the exact reserved handles. This does not call
the allocator or rewind its high-water mark. Repeated restoration is idempotent.
An actual streaming exit releases the lease; a later entry allocates normally.
At tick end, the residency list is synchronized in NpcId order. Thus the original
and restored simulation see the same hysteresis, NPC activity, entity allocations,
RNG consumption and future semantic state. This is not a policy that makes transient
IDs nondeterministic or excludes the registry allocator from StateHash.

The validator permits a leased body to be physically absent after load but rejects
missing NPCs, duplicate/nonordered leases, colliding IDs, ineligible residents,
travel/residency contradictions and handles outside the persisted high-water mark.
External structural changes involving player-managed residents must preserve this
logical residency invariant; arbitrary mutation of a public store is not a gameplay API.

## Persistence and verification

SaveFormatVersion 11 / StateHash `FAULTLINE.state.v13` retain the inherited intended
version bumps exactly once. PlayerStateVersion is 1, GameVersion 0.1.0, GeneratorVersion
3, HistoryVersion 1, StoryGraphVersion 1 and PrologueStateVersion 1. See
[SAVE_FORMAT_V11.md](SAVE_FORMAT_V11.md) and [PHASE17_VERIFICATION.md](PHASE17_VERIFICATION.md).

`faultline_player --seed 12345 --ticks 12000 --save build-release/player.flsave`
executes a versioned semantic movement/aim/interaction script. It saves halfway,
loads into a separate Simulation, and compares subsequent player/entity state and
RNG every tick, full StateHash periodically, and full canonical bytes at the end.
The two-process CTest gate compares complete stdout and save SHA-256. A separate
25-seed, 12000-tick gate retains all previous regression gates.

## Deliberate limits

No Character AI, physical NPC navigation, combat, physics dynamics, gravity,
character-to-character collision, vertical collision, interior traversal, jump,
camera, renderer, vehicles or target-specific interaction execution is included.
Static collision is a conservative ground-plane foundation, with a linear scan
of generated footprints. It can later use a derived spatial acceleration cache
without changing authority. No Phase 18 behavior is implemented.
