# Determinism contract v1

Supported scope: same build, platform and runtime conditions, with equal seed,
configuration, initial state, ordered input and RNG state. Cross-platform floating
point bit identity is not promised (Core Runtime specification section 35).

* IDs: distinct types wrapping unsigned 64-bit integers; zero is null.
* Byte hash: FNV-1a 64, offset 14695981039346656037, prime 1099511628211.
* Canonical state encoding: unsigned integers are eight little-endian bytes;
  strings have an eight-byte byte-count prefix. Never hash padding or pointers.
* Seed derivation: length-prefixed `FAULTLINE.seed.v1`, world seed, length-prefixed
  domain tag. Object derivation hashes the derived seed then the typed ID value.
  Tags must identify both the subsystem and object domain; ID tag names are not
  inferred from C++ compiler type names.
* RNG: xoshiro256** 1.0, expanded from a seed with SplitMix64. `nextU32` takes
  the upper 32 bits of one `nextU64`. Four words of state must be saved in order.
  Zero state is rejected. Bounded integers use rejection sampling, with half-open
  intervals. Probability uses an exact integer numerator/denominator and consumes
  a draw even for probability zero/one.
* Time: 50 ms per tick, 2250 integer game milliseconds per tick. This implements
  the GDD baseline of one game day per 32 real minutes without rounding drift.
  Overflow fails explicitly; wall-clock time never enters authoritative state.

Reference algorithms: https://prng.di.unimi.it/xoshiro256starstar.c and
https://prng.di.unimi.it/splitmix64.c . FNV vectors:
https://www.isthe.com/chongo/src/fnv/test_fnv.c .

These choices are part of the generator compatibility contract. Incompatible
changes require a generator version change after v1 is established.

## Worldgen M1 / generator version 1

Each stage uses `GenerationContext::stageSeed(tag)`: hash the domain-derived seed
followed by the generator version. Geography orientation, water, macro-zones,
roads and districts own separate streams. Districts further derive per-zone
streams. No stage reads NPC/faction RNG.

M1 geography is flat land with an optional rectangular coast. Largest-region
splitting produces contiguous macro-zones; all rectangle boundaries and selected
interior axes contribute to a connected road grid. Every geometric crossing is
a road node. District partitioning is bounded to 1–3 districts per macro-zone.
This is the initial topology baseline, not the final city detail pipeline.

## Worldgen M2 / generator version 2

Version 1 remains callable and keeps the M1 compatibility vector. Version 2
explicitly reuses the v1 stage seeds for upstream topology.
New `parcels`, `buildings` and `territories` streams use version 2, with stable
district/block/parcel subkeys. See [WORLDGEN_M2.md](WORLDGEN_M2.md) for algorithms,
configurable defaults and geometric limits.

The v2 base hash uses `FAULTLINE.base.M2.v2`, the same upstream field encoding,
then all urban configuration fields and length-prefixed parcel, building and
territory vectors in generation order. Every semantic field is included, including
frontage intervals, transform/orientation, flags and territory baseline metadata.
Signed integer coordinates use sign extension to uint64 before little-endian
encoding. No geometry index, cache, timing or memory representation is hashed.
Default seed 12345 / version 2 produces `5f3a446fe0d2f974`; two independent
processes were compared after the complete M2 test gate. Version 1 continues to
produce `2938f0ca42162bc9`.

## M1 canonical encoding and runtime state

`BaseWorldHash` starts with `FAULTLINE.base.M1.v1`, then metadata, configuration,
geography, water, zones, adjacency, road nodes/edges and districts. Every vector
has a length prefix and generation order is explicit. Signed coordinates are
sign-extended to 64 bits and encoded little-endian. No padding, pointers or
transient state are hashed. Seed 12345 with default configuration produces
`2938f0ca42162bc9`; a regression test locks this value.

Runtime StateHash also covers ID allocators, ordered live entities, components,
semantic domain records and base-world hash. Physical/semantic float fields use
IEEE-754 bits with negative zero canonicalized. Spatial index membership is
excluded; configured cell size is included because it determines coordinates.
Rebuilding the index cannot change StateHash. Phase 8 now uses a synchronized
semantic snapshot for NPC representation, as described below.

## Worldgen M3 / generator version 3

The v3 pipeline explicitly preserves the v1 topology and v2 urban stream versions.
New actor/name/slot/home/relationship streams use version 3, with typed IDs or
directed ID pairs as subkeys. Standard faction count is five; four canonical family
values are fixed in the actor registry. A separate family stream selects one
duplicate and shuffles the five slots. NPC changes do not move earlier geometry.

`FAULTLINE.base.M3.v3` includes the same canonical geometry/configuration fields,
then generated faction and NPC states with ID keys, every territory assignment
including neutral entries, and all directed relationship records/context fields.
Normalized social/trait/capability values are integer basis points. Names use
length-prefixed bytes; coordinates and physical float fields keep the existing
canonical encodings. Input/config/registry/algorithm changes that alter v3 worlds
require a new generator version after this checkpoint.

The seed 12345 v3 vector is `888c5dbd10c15886`, protected by an automated regression.
The v1 `2938f0ca42162bc9` and v2 `5f3a446fe0d2f974` vectors are also protected.
CTest compares full CLI output from two independent processes for each version.

Phase 7 runtime StateHash schema started with `FAULTLINE.state.v2` and included all
expanded semantic actor fields, territory owner/garrison and relationship vectors,
plus allocators, RNG, clock and existing world/component state. Generated actor
records are immutable baselines; mutable authority belongs to domain stores.
No wall-clock batch metrics, entity lookup caches or memory addresses are hashed.

## Save Foundation / independent save format v1

Phase 8 runtime StateHash started with `FAULTLINE.state.v3`. Before hashing or saving,
the same snapshot synchronizes active NPC position/health, removes NPC bodies and
sorts directed runtime relationship sets. Allocator high-water marks are retained;
other authoritative data-only components are retained. Thus an active NPC can load
abstract with an equal semantic fingerprint. Spatial membership remains excluded.
See [SAVE_FORMAT_V1.md](SAVE_FORMAT_V1.md) for complete wire encoding and scope.
The three BaseWorldHash algorithms, field order and compatibility vectors above
are unaffected. SaveFormatVersion remains independent of GeneratorVersion.

## Phase 9 / faction simulation and save v2

Phase 9 runtime StateHash started with `FAULTLINE.state.v4`. It preserves the Phase 8
semantic snapshot and adds faction strategic state, integer scheduling config,
next evaluation tick, every operation/commitment/outcome field and the typed
operation allocator. Derived pending/deadline/adjacency indexes are excluded and
rebuilt. The frozen generator actor encoding does not include new runtime-only
strategy fields; all three base hashes above remain unchanged.

Faction evaluation uses stable FactionId order every four game hours by default;
due operations use stable OperationId order. Utility selection has no random
branch. Aggregate resolution uses only RuntimeRandomState.factions, with bounded
integer uncertainty/casualties. No wall-clock values enter authoritative state.
Fixed/coarse scheduled advancement and mid-operation save/load continuation must
produce equal StateHashes and canonical save bytes. Separate processes compare
complete 30-day simulator output, including all resources/statistics.

Save v2 adds one runtime section and genuinely migrates v1 without consuming RNG
or changing old resources/generated identity. See [SAVE_FORMAT_V2.md](SAVE_FORMAT_V2.md)
and [FACTION_SIMULATION_M1.md](FACTION_SIMULATION_M1.md).

## Phase 10 / NPC semantics and save v3

Phase 10 runtime StateHash started with `FAULTLINE.state.v5`. NPC activity, home,
optional travel, per-actor deadlines, one operation assignment and fixed-size
statistics extend the snapshot; generated NpcState field hashing stays frozen.
Relationships retain their canonical directed-edge ordering. The NPC deadline
minimum is derived and excluded. Runtime promotion uses the existing persisted
NpcStore allocator, so generated and runtime IDs cannot collide or be recycled.

NPC decisions use ascending NpcId, integer game-time deadlines and only the
runtime NPC RNG stream. Faction resolution still uses only its faction stream;
semantic named support grants no extra aggregate force or random named casualties.
Normal ticks and safe headless advancement share faction -> NPC -> structural
commit ordering. Arrival has its own exact deadline, independent of the regular
30-minute decision interval. Restrictive lifecycle states cannot travel/work.

Save v3 appends section 12 and reads v1/v2 through deterministic defaults, without
consuming RNG or changing old fields. A real Phase 9 save with eight pending
operations is retained as a fixed compatibility fixture; its historical section
bytes must survive migration unchanged. See SAVE_FORMAT_V3.md, NPC_SEMANTIC_M1.md
and PHASE10_VERIFICATION.md for implementation and actual verification status.

## Phase 11 / territory presence and save v5

Phase 11 StateHash used FAULTLINE.state.v7. Territory rules, sparse causal
entrenchment, local resistance and its two rule units, concrete garrison affiliation, the incumbent hysteresis latch,
derived control state/controller, exactly-once operation consumption and fixed-size
counters extend the canonical state. The separate control-cache map is excluded.
No generated field encoding or BaseWorldHash changes; GeneratorVersion stays 3.

Control is derived from effective presence and explicit hysteresis memory. Gain
margin 80 and hold margin 60 are integer strategic units; contested/uncontrolled
regions have no controller. Operation outcomes are consumed in OperationId order,
batched per target and reevaluated in TerritoryId order. One logical transition
emits one scoped event. No RNG or wall-clock value is used by TerritorySystem.

The shared fixed/headless pipeline resolves operations, applies territory effects,
reconciles stale operations, performs only due faction evaluation, updates NPCs
and commits structural changes. AI reads current control, not generation-time
assignment. Garrison membership never follows a controller flip. Thus operation
counts/outcomes may change naturally from prior phases; their old runtime hashes
and 468-operation baseline are historical, not Phase 11 compatibility contracts.

Save v4 appends section 13 and preserves previous section encodings. V1/v2/v3
migration initializes the saved map deterministically and acknowledges historical
terminal results without replay. Active operations remain pending for future
resolution. Presence, hysteresis and the consumption ledger preserve mid-contest
and day-30-to-year-end continuation. See SAVE_FORMAT_V4.md, TERRITORY_M1.md and
PHASE11_VERIFICATION.md for actual schema, algorithms and verified results.

Save v5 retains the entire v4 payload prefix of section 13 and appends the two
resistance rules plus one resistance value per ordered territory. V1..v4 migration
starts resistance at zero without replaying casualties. Attack casualties raise
bounded opposition; surviving successful defense relieves it. Stable operation
ordering, exactly-once consumption and one reevaluation per target also govern
resistance. No civilian simulation, RNG or extra deadline is introduced. See
SAVE_FORMAT_V5.md and the repair section of PHASE11_VERIFICATION.md.

## Source interpretation

The architecture specification section 28 gives a broad initial layering,
including persistence before generation. The dedicated roadmap sections 4–12
give the detailed implementation sequence, placing save foundation at phase 8.
We follow the explicit roadmap requested by the task while keeping every earlier
type semantically serializable. No source files in the parent are modified.

The world-generation specification section 103 illustrates the complete future
generator. Its sections 98–102 explicitly stage implementation; M1 validation
does not claim validation of buildings, factions, NPCs, market or story.

## Phase 12 / district market and save v6

Current StateHash is FAULTLINE.state.v8. It adds all persistent market fields,
fractional revenue, disruption memory, assignments and economic scheduler/rules.
Calculation uses stable DistrictId/FactionId/NpcId order and bounded integer
arithmetic; no RNG draws or wall-clock input. Fixed/headless paths include market
deadlines and pay once per scheduled update. The pipeline places market after
operation/territory consequences and reconciliation, before faction evaluation
and NPC decisions. Physical health and synchronized abstract health have equal
economic consequences. See MARKET_M1.md and SAVE_FORMAT_V6.md.

GeneratorVersion and all historical BaseWorldHash fixtures remain unchanged.
Save v1..v5 migrates without historical revenue or RNG consumption. New runtime
hashes and resource trajectories naturally differ from Phase 11 as treasury
income supports future paid operations. Historical runtime hashes document their
old implementations; they are not promises that market-enabled futures match.


## Phase 13 / strategic history and save v7

Current StateHash is FAULTLINE.state.v9. The structural history API consumes
explicit delivery/discovery evidence and HistoryVersion 1; it does not silently
run during construction or old-save migration. Its 48 monthly decisions and
1460 daily market updates reuse semantic territory, market and relationship
rules. Separate discovery/origin and history/month seed domains consume no
runtime RNG. Stable domain ordering, existing NPC allocators and monthly
checkpoint persistence make interrupted histories reproduce exact hashes/bytes.
History costs conserve treasury and reserve-to-NPC/garrison transfers conserve
actual manpower. The stage preserves generator v1/v2/v3 BaseWorldHash fixtures;
its consequences are part of the resulting semantic state. Runtime simulation
resumes after completion with no permanent balance caps. See STRATEGIC_HISTORY.md,
SAVE_FORMAT_V7.md and PHASE13_VERIFICATION.md. No Phase 14 story logic is included.

## Phase 14 / story graph and save v8

Current StateHash is FAULTLINE.state.v10; the v9 description above records the
historical Phase 13 contract. StoryGraphVersion 1 adds ordered semantic nodes,
allocator state, immutable history provenance and explicit outcome prerequisites.
Generation and repair consume no runtime RNG and mutate only story state. Live
availability is derived from authoritative world facts and saved outcomes.
Independent-process tests compare both stdout/StateHash and canonical save SHA256.
V1..v7 saves default to an uninitialized graph without changing their existing
payloads or inventing prologue/history. Worldgen and HistoryVersion stay unchanged.
See STORY_GRAPH.md and SAVE_FORMAT_V8.md. No Phase 15 mission state is added.

## Phase 15 / mission core and save v9

Current StateHash is FAULTLINE.state.v11; the v10 description above records the
historical Phase 14 contract. Mission system adds MissionId allocator state,
mission instance state, objective state, and mission event buffer. Generation and
repair consume no runtime RNG and mutate only mission state. Live mission
eligibility is derived from authoritative world facts and saved outcomes.
Independent-process tests compare both stdout/StateHash and canonical save SHA256.
V1..v8 saves default to an uninitialized mission system without changing their
existing payloads. Worldgen and HistoryVersion stay unchanged.
See MISSION_CORE.md and SAVE_FORMAT_V9.md. No Phase 16 drug market state is added.


## Phase 17 / physical player and save v11

Current StateHash is `FAULTLINE.state.v13`; Phase 16 was v12 (prologue).
Physical movement uses the 50 ms fixed timestep, not the accelerated 2250 ms
semantic game-time increment. Position/body/player identity and logical NPC
streaming allocation leases are hashed. The EntityRegistry allocator remains
persisted/hashed, including after transient NPC bodies are removed from snapshots.
Leased bodies are reconstructed without allocation before tick rules run. This
preserves hysteresis and semantic NPC scheduling across save/load, not just the
immediate snapshot. Real movement process tests compare stdout and canonical save
SHA-256; per-tick continuation tests compare IDs, allocator and RNG as well as
StateHash. See PLAYER_CONTROLLER.md and PHASE17_VERIFICATION.md. No Phase 18 AI.
