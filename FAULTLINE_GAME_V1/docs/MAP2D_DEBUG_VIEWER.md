# FAULTLINE 2D Debug Map Viewer

## Purpose

`faultline_map2d` is a **non-authoritative** development/debug visualization tool that renders
the FAULTLINE world as a 2D top-down SVG map. It is intended for developers and designers
who need to visually inspect generated worlds and saved game state without running the full
simulation or a graphical renderer.

**This tool does NOT:**

- Advance simulation ticks.
- Consume runtime RNG.
- Modify GameState.
- Allocate authoritative EntityIds as a side effect.
- Change territory, faction state, save format, StateHash, or GeneratorVersion.
- Materialize NPCs just for rendering.
- Run Character AI, Combat, or any gameplay systems.
- Affect saves (load mode only reads).

## CLI

```
faultline_map2d --seed N --output PATH [--no-buildings] [--no-parcels]
                [--runtime] [--no-territories] [--no-npcs] [--labels] [--width PX]

faultline_map2d --load PATH --output PATH [--no-buildings] [--no-parcels]
                [--no-territories] [--no-npcs] [--labels] [--width PX]
```

### Required

- `--seed N` or `--load PATH` (mutually exclusive; cannot use both)
- `--output PATH`

### Optional

- `--no-buildings` — skip rendering building footprints
- `--no-parcels` — skip rendering parcel outlines
- `--no-territories` — skip rendering territory fills
- `--no-npcs` — skip rendering NPC markers (both semantic and active)
- `--labels` — add text labels for districts, factions, and NPCs
- `--width PX` — SVG canvas width in pixels (default: 1200)
- `--diagnostics` — print spatial bounds report to stdout
- `--help` — print usage

## Modes

### Seed Mode (`--seed`)

Generates a fresh world from the specified seed using the current generator version (v3).
Visualizes the initial generated world state only: roads, water, districts, parcels,
buildings, territories, factions, and major NPCs at their semantic positions. No player
or active NPCs are shown (they do not exist in the initial state).

### Runtime Seed Mode (`--seed ... --runtime`)

Constructs the canonical `GameState`, creates the player through
`createPlayerEntity`, and performs one zero-input streaming residency commit without
advancing the simulation clock. This is a playable-state initialization view, not a
fake marker and not a simulation tick. It normally shows the player and legitimately
resident physical NPCs. The spawn rule is the same deterministic rule used by the
Phase 17 headless player tool: the first generated major NPC semantic position.

### Load Mode (`--load`)

Loads a saved game file (.flsave) using the canonical persistence layer and visualizes
the actual saved world state, including:

- Current territory control state (authoritative runtime control)
- Player position (if a physical player entity exists)
- Active NPC bodies reconstructed from persisted residency leases, without advancing a tick
- Semantic major NPC positions

## Coordinate Mapping

The viewer maps the FAULTLINE world coordinate system to SVG coordinates using a general
form that works for arbitrary world bounds:

```
SVG x = mapLeft + (world X - worldMinX) * scale
SVG y = mapTop  + (world Z - worldMinZ) * scale
```

World X maps right and world Z maps down. The viewer does not assert a north convention
that is absent from the authoritative spatial model.

The transform fields stored in `SvgTransform`:
- `scale` — uniform scale factor (world meters → SVG pixels), preserving aspect ratio
- `worldMinX`, `worldMinZ` — world bounding box minimum (subtracted before scaling)
- `mapLeft`, `mapTop` — SVG position of the mapped world-min corner (includes margin + centering)

The world bounding rectangle is centered within the available map area. The scale is
the minimum of the X and Z axis scales (aspect-ratio-preserving), so the longer world
dimension fills the map area and the shorter dimension is centered with extra padding
on the unused axis.

World coordinates use a cell/local system:
- `CellCoord` {x, z} — integer cell index (128m per cell by default)
- `LocalPosition` {x, y, z} — float offset within the cell (meters)
- Global position = cell * cellSize + local

The `toGlobal()` function converts WorldPosition to global double-precision meters. The
viewer uses this canonical conversion and does not alter authoritative positions.

Bounds are derived from the generated world's `bounds` Rect, not hardcoded.

The transform preserves aspect ratio, scaling the world to fit within the SVG canvas
(width minus margins) with a 0.75 height-to-width ratio for the map area.

## Layers

Layers are rendered in back-to-front order with named SVG `<g>` groups:

| Layer | SVG Group | Source Data | Rendering |
|-------|-----------|-------------|-----------|
| 1. Background | `background` | World bounds | Light gray fill |
| 2. Bounds | `bounds` | World bounds Rect | Dashed black outline |
| 3. Water | `water` | `GeneratedWorld::water` | Blue fill |
| 4. Territories | `territories` | `territoryControl()` / `TerritoryAssignment` | Translucent fills (20% opacity) |
| 5. Districts | `districts` | `GeneratedWorld::districts` | Thin gray outline |
| 6. Roads | `roads` | `RoadGraph::edges` | Stroked lines, width varies by category |
| 7. Parcels | `parcels` | `GeneratedWorld::parcels` | Very thin gray outlines |
| 8. Buildings | `buildings` | `GeneratedWorld::buildings` | Dark gray rounded rects |
| 9. Factions | `factions` | no geometry | Explicitly empty: the current model has home territories, not canonical HQ points |
| 10. NPCs | `npcs` | `NpcState` active links + `GeneratedNpc` semantic | Red filled (active) / orange hollow (semantic) |
| 11. Player | `player` | controlled entity `Transform`; `CharacterBody` facing | Blue triangle with direction line |
| 12. Metadata | `metadata` | GameState metadata | Info box |
| 13. Legend | `legend` | Static | Color/shape reference |

## Faction Colors

Factions receive stable deterministic colors (debug UI concern, NOT canonical game state):

| FactionId | Color | Hex |
|-----------|-------|-----|
| 1 | Red | `#e41a1c` |
| 2 | Blue | `#377eb8` |
| 3 | Green | `#4daf4a` |
| 4 | Purple | `#984ea3` |
| 5 | Orange | `#ff7f00` |
| 0 / contested / uncontrolled | Gray | `#999999` |

Territory fills use 20% opacity so roads and buildings remain visible.

## NPC Representation

- **Semantic major NPCs** (from `GeneratedWorld::majorNpcs`): hollow orange circles at their
  semantic WorldPosition. These exist even when not physically materialized.
- **Active NPC bodies** (from `GameState::world.npcLinks`): solid red circles at their
  current Transform position. These only exist when NPCs have been materialized by the
  streaming system. NpcId is the semantic identity; EntityId is the transient body handle.

## Determinism

The viewer is deterministic: same input (seed or save file) and same CLI flags produce
byte-identical SVG output. The viewer:

- Uses `std::locale::classic()` for all number formatting (no locale-dependent separators).
- Does **not** embed timestamps or absolute paths in the SVG output.
- Iterates over data in stable ID order (EntityId, NpcId, FactionId, etc. are stored in
  `std::map` ordered by ID).
- Does **not** consume RNG state.
- Does **not** advance simulation.
- The `--diagnostics` flag only affects stdout output; the SVG file is byte-identical
  with or without `--diagnostics`.

## Spatial Diagnostics (`--diagnostics`)

When `--diagnostics` is supplied, the viewer prints a structured spatial bounds report
to stdout after rendering the SVG. This report includes:

- **WORLD BOUNDS** — the generated world's Rect (minX, maxX, minZ, maxZ, width, depth)
- **ROAD NODES** — count and spatial extent of all road nodes (actual geometry extents,
  including edge endpoints)
- **DISTRICTS** — count and spatial extent of district boundaries
- **PARCELS** — count and spatial extent of parcel boundaries
- **BUILDINGS** — count and spatial extent of building footprints
- **TERRITORIES** — count and spatial extent of territory boundaries
- **WATER** — count and spatial extent of water region boundaries

Each layer reports:
- `count` — number of objects
- `minX`, `maxX`, `minZ`, `maxZ` — bounding box in world meters
- `width`, `depth` — extent size
- `coverageXPercent`, `coverageZPercent` — percentage of world width/depth occupied

If a layer is empty, it prints `bounds: NONE` instead of fake zeros.

The report concludes with:
- **CITY / WORLD COVERAGE SUMMARY** — road, building, and district coverage percentages
- **VIEW TRANSFORM** — the SVG coordinate mapping parameters (world min/max, SVG dimensions,
  scale factors, chosen scale, and offset)
- **ACTUAL SVG GEOMETRY BOUNDS** — the transformed world corners and per-layer SVG-space
  bounds, including a comparison of expected vs. actual rendered coordinates
- **EXTREME OBJECT TRACE** — source world coordinates, expected SVG coordinates, and actual
  emitted SVG coordinates for extreme road nodes and buildings
- **SCALE RATIO ANALYSIS** — effective scale, canvas fill ratio, and bug location diagnostics

This output helps distinguish whether a visible city occupies only part of the SVG world
extent due to (A) actual worldgen coordinates or (B) the viewer coordinate transform.

## Performance

For seed 12345, the viewer handles:

- ~210 road nodes, ~391 road edges
- 18 districts
- ~8007 parcels
- ~8007 buildings
- 41 territories
- 5 factions

SVG output is streamed directly to the output file via `std::ofstream`. No GPU acceleration
or external graphics libraries are required.

## Known Limitations

1. **Player/NPC bodies in seed mode**: In pure `--seed` mode, no player or active NPC bodies
   are shown because they are not part of the initial generated state. Use `--load` with a
   save file that has a physical player to see the player marker.

2. **Active NPCs in saved state**: Physical bodies are transient and stripped by
   `persistentSnapshot`; load mode calls the canonical `restorePlayerResidency` and reuses
   saved allocation leases. This does not advance time, consume an ID or change StateHash.

3. **Parcels layer**: With ~8000 parcels, the SVG can become large. Use `--no-parcels` to
   skip rendering them.

4. **No interactive features**: The SVG is static. Layer visibility can be controlled in
   a browser's SVG inspector if the file is opened as XML.

5. **Road width**: The viewer renders roads as stroked lines. Actual road half-widths are
   available in `UrbanGenerationConfig::roadHalfWidths` but are not applied to line thickness
   to keep the map readable at default scale.

6. **No faction HQ markers**: `FactionState::homeTerritory` is not a headquarters
   position. The viewer does not fabricate a point at its center.

7. **No north indicator or scale bar**: These are listed as optional quality improvements
   and may be added in future revisions.
