# Save format v14

Phase 21 changes the persistent schema because vehicles now have semantic
identity, ownership, condition, location and abstract motion that must survive
save/load independently of any physical body. `SaveFormatVersion` therefore
advances from 13 to 14 and StateHash from `FAULTLINE.state.v15` to
`FAULTLINE.state.v16`. GameVersion remains 0.1.0 and GeneratorVersion remains 3.

V14 appends section 22, VehicleSimulation. It serializes VehicleSimulationVersion
1 rules and the driven `VehiclePersistentId`; ordered residency leases
(`VehiclePersistentId` + leased `EntityId`); and one semantic record per vehicle:
store key, transient active entity (always null in a valid v14 save), category,
archetype, ownership, condition, location, abstract position, yaw, speed and
passenger capacity. Rules and semantic records are encoded explicitly in
`src/persistence/save.cpp`; field lists live in `src/persistence/schema_v14.hpp`.
Diagnostic statistics are excluded. No renderer, Map2D, spatial-cache, transient
body/component, or wall-clock state is serialized.

The historical Vehicles section keeps its original activeEntity-only wire encoding
(`schema_v1.hpp`). Vehicle bodies are transient: v14 saves must contain no
`VehicleLink` components and every `VehicleState.activeEntity` must be null.
Residency leases preserve the physical allocation so restoring a saved game does
not consume new `EntityId` values or change semantic state.

V1-v13 migration initializes neutral `VehicleSimulationState` v1 and never seeds a
fresh fleet or consumes gameplay RNG. Legacy vehicle records are reconstructed
deterministically: the archetype is inferred from the legacy health maximum
(Sedan 100, Hatchback 85, PoliceCruiser 130), ownership/location have no legacy
meaning and remain empty, and abstract motion is zero. A legacy active body is
converted into a coherent residency lease with matching `VehicleBody`/
`VehiclePhysics`, so migrated state validates without inventing vehicles or
locations. Existing v12/v13 section payloads are preserved byte-for-byte.

The reader rejects unsupported VehicleSimulation versions, unsorted, duplicate or
dangling residencies, semantic-count mismatches, non-null active entities, and
vehicle bodies/links in a v14 save. World validation additionally requires valid
category/archetype pairing, valid owner references, finite bounded motion,
normalized positions, matching body dimensions, and a healthy owned body for an
active entity.

Known scope: vehicle bodies are rebuilt from semantic state, so active body
identity is a deterministic allocator lease, not a durable semantic ID. The
persistent identity is the `VehiclePersistentId` store key.
