# Combat foundation

Phase 19 adds an authoritative, renderer-independent combat pipeline. Player input
and active Character AI produce transient `CombatIntent` values. `CombatSystem`
validates physical and semantic links, equipped weapon, cooldown, ammunition,
global `WorldPosition` range and static line of sight before editing the existing
`Health` component. Character AI never edits Health.

The immutable registry contains pistol, shotgun and SMG definitions. Runtime
`WeaponInstance` records contain magazine and reserve ammunition; combatant records
contain the equipped instance and tick-based cooldown/reload deadlines. The combat
registry is sparse: an NPC without an explicitly assigned loadout is valid and
unarmed. All deadlines use the authoritative 20 Hz clock.

Hit resolution is deterministic. A valid in-range shot with clear static line of
sight hits; range and building/water obstruction produce explicit misses. This
foundation consumes no RNG. Static line of sight uses generated simulation geometry,
never renderer raycasts. There is no penetration or dynamic character collision.

Attacks in one tick are ordered by semantic actor key (player, then ascending
`NpcId`) and target key. Resolution is sequential. Damage clamps existing Health
at zero and is applied once.

NPC death removes the transient EntityId body and marks the persistent NpcId dead
through NPC lifecycle authority. A dead leader is replaced deterministically by an
existing living member; no NPC or resource is created. Mission and Story systems
observe the resulting death event. Player Health at zero is a persistent terminal
physical state: movement and attacks stop, with no UI or police behavior here.

Snapshots persist semantic NPC Health plus weapon instances, ammo, reload/cooldown
deadlines and combat statistics. NPC bodies and pending tick intents remain
transient. Save v12 migration initializes deterministic default loadouts for v1-v11
saves without RNG consumption.

Limitations: instantaneous ray resolution, no projectile body, cover, armor, limb
damage, advanced ballistics, dynamic crowd collision, corpse simulation, witness,
police, wanted state, vehicle combat or renderer effects.
