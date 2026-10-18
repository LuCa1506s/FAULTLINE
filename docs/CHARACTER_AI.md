# Basic Character AI (Phase 18)

Phase 18 adds deterministic local physical execution for persistent NPCs that
streaming has already materialized. `NpcId` remains the semantic identity and
`EntityId` remains its transient body. Character AI neither materializes actors
nor replaces semantic travel, activity, personality, relationships or faction
assignments.

## Authority and update order

The fixed-tick runtime order is:

1. restore saved logical residency leases without allocating IDs;
2. update faction, territory, market, semantic NPC, mission and story state;
3. commit structural changes;
4. move the player and commit streaming residency;
5. derive and execute physical NPC intents in ascending `NpcId` order;
6. emit scoped player/AI diagnostics and advance the clock.

Only `PlayerState.residents`, the ordered ACTIVE set maintained by streaming, is
visited at 20 Hz. Abstract NPCs receive no physical update. The system validates
the `NpcId`/`NpcLink`/`EntityId` round trip and requires Transform, Health and
CharacterBody before execution. It never moves the player entity.

## Intent and decision model

`CharacterAiIntent` is transient and contains actor IDs, behavior, an optional
WorldPosition target and optional semantic target NpcId. Its bounded behaviors are:

- `Idle`: no displacement and canonical reconstructed facing;
- `MoveTo`: return to an authoritative semantic work location;
- `Follow`: follow a locally perceived faction leader during a meeting;
- `Flee`: retreat to the actor's existing semantic home when hiding or when an
  injured low-courage actor needs safety;
- `AttackIntent`: approach the territory of an assigned attack operation;
- `Protect`: approach the territory of an assigned defense operation.

Attack intent is locomotion only. It does not choose weapons, enter combat, aim,
damage, or kill. Priority is survival, then live operation assignment, then local
role/activity. Idle actors do not invent random wandering. Meeting follow targets
must exist as valid physical actors within the deterministic 30 m spatial query;
operation and home locations are explicit semantic knowledge, not perception.

## Movement and facing

NPC bodies use CharacterBody at 3.5 m/s and 1.75 m stance height. One 50 ms fixed
tick moves at most 0.175 m. Direction is normalized and clamped to the remaining
distance. CharacterBody owns physical facing; movement/target direction sets yaw
with `atan2(x,z)`. Idle facing is reconstructed as zero. No second persistent NPC
yaw exists.

Both player and NPC movement call the same `world::moveCharacter` primitive, so
cell/local normalization, swept building/water collision, world bounds and axis
sliding follow the same rules. There is no navmesh in this milestone. Direct
steering can stop at an obstacle it cannot slide around.

## Streaming, abstraction and persistence

Materialization creates Transform, Health, CharacterBody and NpcLink. Abstraction
copies physical position and health back to NpcState and destroys the transient
body. Rematerialization uses the same NpcId and the existing deterministic lease/
allocator rules. Physical progress therefore survives streaming and save/load.

The intent, perception results and path scratch state are derived each tick. NPC
CharacterBody/facing is transient across abstraction. Authoritative progress is
already represented by NpcState position, semantic activity/assignment, world
state and residency leases. Phase 18 therefore keeps SaveFormatVersion 11 and
StateHash `FAULTLINE.state.v13`; no AI state version or migration is introduced.
Canonical save snapshots continue to discard NPC bodies after synchronizing them.

## Limits

There is no combat, damage decision, weapon logic, cover, police, vehicle AI,
dynamic character collision, crowds, advanced pathfinding, doors, interiors,
vertical physics or renderer dependency. Semantic long-distance travel remains
exclusive with physical materialization. Phase 19 owns combat.

