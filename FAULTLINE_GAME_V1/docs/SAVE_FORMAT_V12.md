# Save Format v12 — Combat runtime state

V12 keeps the v11 little-endian encoding and all nineteen historical sections
unchanged. It appends section 20, `Combat`.

The section encodes CombatStateVersion 1, the ordered weapon-instance store and
allocator, player combatant state, an ascending sparse `NpcId -> CombatantState`
map, and aggregate statistics. Weapon instances store definition ID, magazine and
reserve ammunition. Combatants store equipped instance and tick deadlines.
Immutable definitions are code rules and are not duplicated in saves.

V1-v11 migration deterministically creates the same initial loadouts as a new
runtime state and consumes no RNG. StateHash `FAULTLINE.state.v14` includes this
canonical state. Transient intents, events, line-of-sight scratch data and physical
NPC bodies remain excluded.
