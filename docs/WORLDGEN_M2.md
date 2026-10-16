# Worldgen M2 / generator version 2

Design sources: parent `WORLD_GENERATION_SPEC.md` sections 19, 26–32, 63–73,
98–100; entity/domain model sections 18–22; spatial model sections 14–15 and
48–50; roadmap Phase 6. The GDD remains authoritative. No parent files change.

## Compatibility and stage boundaries

`--version 1` retains M1 generation and its original canonical hash. The current
default at the M2 checkpoint was version 2 (M3 now defaults to version 3).
Version 2 explicitly reuses the version 1 geography,
macro-zone, road and district algorithms/stream versions. New urban stages use
their own version 2 streams: `parcels` with district/block keys, `buildings` with
ParcelId, `territories` with DistrictId. This compatibility mapping is deliberate;
altering building weights cannot perturb topology or lot layout.

Each urban stage returns a value (`ParcelGenerationResult`,
`BuildingGenerationResult`, `TerritoryGenerationResult`), without mutating input
world data. The generator orchestrates topology -> parcels -> buildings ->
territories -> full validation. `GameState` owns the generated semantic base and
initializes TerritoryStore keys from it; buildings create no ECS entities.

## Parcels and road access

All M1 roads are orthogonal. M2 derives reserved road rectangles from centerlines,
configured half-widths and square intersection/end caps, clipped to world bounds.
The default half-widths are 12/8/5/3 meters for arterial/major/local/service roads.
These are configurable M2 defaults, not canonical balancing requirements.

Sorted road axes define blocks. Each block is inset by the largest road width on
its bounding axes, then clipped to its parent district. Available perimeter strips
are carved in fixed north/south/west/east order, so corner space is assigned once.
Strip depth is at most 64 meters by default. Equal integer subdivisions of each
strip target a stream-varied frontage around 48 meters. Lots below 12 meters in
either dimension are omitted. Defaults and building weights live in
`UrbanGenerationConfig` and are part of the v2 hash.

Each emitted parcel references a real RoadEdgeId, face direction and exact shared
frontage interval, at least the minimum lot span. Road/water collisions are
rejected before emission. Unusable slivers and disconnected interior space are
left without parcels. The buildable flag is explicit; current generation emits
only buildable lots. Parcel IDs follow sorted DistrictId, X/Z block axes, side
order and longitudinal lot order. No allocation addresses or unordered traversal
contribute to identities.

## Buildings

At most one building is emitted per buildable parcel. A per-ParcelId RNG determines
footprint occupancy within a conservative inset (default two meters), then chooses
an eligible archetype using district-specific weight profiles and minimum usable
footprint areas. Industrial/harbor districts favor warehouse/industrial use;
downtown/financial/entertainment districts favor stores/offices; residential
profiles favor houses/apartments. Profiles are directly configurable.

The front follows the parcel road frontage. Front setback is preserved, with
remaining empty space behind the building. Center coordinates use exact half-meter
integers and orientation is cardinal: north=-Z, east=+X, south=+Z, west=-X. Flags
explicitly specify no interior. BuildingId follows sorted ParcelId among emitted
buildings. Unsupported small lots can remain empty rather than force an invalid
building for a customized profile.

## Territories

Each district starts as one connected rectangle. A bounded target uses district
area and actual building count (default target area 1.2 km², at most six regions).
Candidate splits must lie on real arterial/major road axes with continuous graph
segment coverage across the entire proposed boundary. Splits require minimum
area/span (64,000 m² / 64 m defaults), favor balanced areas and important roads,
and break ties through the district's territory RNG. Repeated road graph segments
on one axis do not weight a cut multiple times.

If no valid real boundary exists, subdivision stops and preserves the connected
region. Final regions are sorted geometrically within DistrictId before ID
allocation. They cover the parent district exactly, including road surfaces.
Baseline metadata records contained building count and buildable parcel area.
Control/owner is implicitly unassigned: no faction distribution exists in M2.
Faction cohesion and political inputs are deferred until their domain exists.
Territory geometry lives in the generated base; runtime TerritoryStore references
that geometry by the same TerritoryId.

## Geometry and validation

`Rect` is an implicit four-vertex axis-aligned polygon. Integer meter coordinates,
64-bit area arithmetic and half-meter building transforms avoid floating-point
epsilon decisions. Edge contact is permitted; positive-area overlap is rejected.
The shared rectangle utilities define area, containment, intersection and inset.
The static `RectIndex` AABB tree accelerates road, parcel, building and territory
overlap checks and metadata queries. It is rebuilt from canonical geometry and
does not enter hashes, saves or streaming state.

Validation checks unique non-null typed IDs, district/parcel/road references,
positive contained geometry, exact area/frontage/transform metadata, road/water
exclusion, parcel/building overlaps, one building per parcel, territory boundary
features, minimum subdivision size, coverage, overlaps and building membership.
Failures include seed/version at the tool/generator boundary and object IDs in
urban validation errors. There are no random retries or unbounded repair loops.
Parcel growth has an explicit default limit of 100,000, configurable up to
1,000,000, and fails with a reproducible error rather than truncate the city.

## Limits

This milestone supports the existing orthogonal, flat M1 topology; no arbitrary
polygon clipping, curved roads, slopes, interiors, entrances or multiple buildings
per parcel. Projecting partial road axes across blocks is conservative and can
leave additional unused land; frontage still requires an actual road segment.
Territories are road-aligned connected rectangles, independently of streaming
cells. The interior of a large block may remain vacant. These limits prioritize
valid geometry and do not claim a final urban density model.
