# Map2d independent audit

The audit started from commit `33f798a` with the viewer uncommitted. It found and
fixed four material defects: tests mirrored implementation instead of inspecting
emitted SVG; a stale header retained the broken origin transform; home-territory
centres were incorrectly presented as faction HQs; and load mode read generated NPC
positions instead of current runtime NPC state. CSS alpha bytes also used the wrong
`#AARRGGBB` order.

Raw `--seed` mode is now explicitly generated-world data and therefore has no player
or active physical NPC. `--seed ... --runtime` constructs `GameState`, creates the
player through `createPlayerEntity`, and performs a zero-input streaming commit. Load
mode uses the persistence layer and restores saved residency leases without advancing
the clock. Both operations preserve the semantic StateHash during visualization.

Seed 12345 raw data: 210 road nodes, 391 emitted road edges, 18 districts,
8007 emitted parcels, 8007 emitted buildings, 41 emitted territories, five factions,
22 semantic major NPCs and one water region. Bounds map from `(-4000,-4000)..(4000,4000)`
to `(156,8)..(1044,896)` in the 1200 by 1104 SVG. The west water region maps to
`x=156..235.92`, `y=8..896`.

The runtime verification produces player EntityId 1 at world `(1638,2714.5)`, SVG
`(781.82,753.31)`, yaw 0, and one legitimately resident NPC without advancing a tick.
An authentic SaveFormatVersion 12 combat save loads with the same semantic StateHash,
a real player and three reconstructed NPC bodies. No HQ markers are emitted because
the data model contains only `homeTerritory`, not an authoritative HQ point.

`map2d_process_determinism` now parses actual SVG output, checks raw/runtime/v12-save
modes, emitted geometry counts, layer switches, marker attributes and byte identity.
The focused doctest suite retains arbitrary-origin and non-square transform coverage.
No worldgen, persistence schema, StateHash schema or gameplay authority changed.
