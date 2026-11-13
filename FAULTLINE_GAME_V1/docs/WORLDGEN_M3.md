# Worldgen M3 / generator version 3

Sources: parent GDD sections 7, 17–19, 49–51; WORLD_GENERATION_SPEC sections
33–43, 61–64, 74–75, 100; faction spec resource/diplomacy sections; NPC spec
identity, roles, authority, traits, sparse relationships and materialization;
entity/domain and spatial identity rules; roadmap Phase 7. Parent documents are
unchanged. The current playerless initialization is NOT the post-timeskip world.

## Version mapping and stage order

Version 3 retains v1 geography/road/district streams and v2 parcel/building/
territory geometry streams. Actor generation cannot consume their RNGs. Existing
v1 and v2 hashes remain supported. New stages use version 3 tags under `factions`,
`npc` and `relationships`, with separate name, home and slot-count streams and
per-FactionId/local-slot, per-NpcId or directed-pair keys where applicable.

The pipeline is topology -> parcels -> buildings -> territories -> factions and
initial presence -> major NPCs -> leader references -> relationships -> validation
-> hash. Stages return explicit result values. Generation is bounded and requires
no recovery/retry loop beyond at most 16 name attempts plus a unique ID suffix.

## Factions and resources

There are exactly five generated main factions. The family registry is precisely:
African-American street gang; Italian Mafia; Asian criminal organization /
Yakuza-like organization; Cartel. Start with one of each, draw one duplicate
uniformly using the family stream, then Fisher-Yates shuffle all five slots.
FactionIds are allocated in slot order. Runtime stores can still contain more
than five factions later; only the standard initial-generation contract is fixed.

Each faction has a deterministic family-compatible name, unique home, separate
economic/manpower/cohesion baselines and aggression/risk/market/territorial focus.
Profiles vary by FactionId substream, including the duplicated family. Their
personality and treasury values are guaranteed distinct as well as their home,
leader and name. Market focus is a strategic trait, not market simulation.

Home selection scores canonical territory candidates using family/zone affinity,
useful area/buildings, distance in the territory adjacency graph from existing
homes, and seed-derived tie variation. Homes must be distinct and have viable
geometry and real road access. Very large home candidates over 40% of city area
are excluded. This is an initial-generation safety rule, not runtime balancing.

Initial control grows from each home through shared positive-length territory
borders, favoring the home district and appropriate zone archetypes. Smaller
holdings get the next expansion opportunity. Personality sets a soft area budget
of approximately 16–21% of the city; a large home can exceed that soft target.
No disconnected acquisitions are allowed. Territories that cannot be claimed
coherently stay unassigned. No faction gets control by arbitrary map color alone.

Every assigned territory receives 3+ aggregate members depending on area, drawn
from the faction's initial 70–110 reserve members. Expansion leaves at least ten
in reserve. `reserveMembers` excludes garrisons and persistent named members.
Total manpower is reserve + territorial garrisons + living named members, each
counted once. NPCs and garrisons are initial semantic assets; there is no runtime
recruitment or operation. The viability validator checks leadership, important
member structure, home/presence, connected holdings and positive resources;
control above 50% of city area fails a broad sanity check.

These numeric ranges are explicit M3 initialization defaults, not finalized
canonical balancing formulas. Exactly five main factions and the four named
families are canonical requirements.

## Major NPCs and locations

Each faction draws 3–5 important members from an isolated slot stream, producing
15–25 in total. Per-faction/local-slot streams generate names, identity appearance
seed, age band, Respect, loyalty, six canonical personality traits and five
capability baselines. All major NPCs start ALIVE with no EntityId. Traits and
capabilities use normalized integer basis points [0,10000], avoiding NaN/drift.
Respect uses [0,100]. Traits provide baseline social factors and future AI inputs;
no decisions or combat are simulated at this milestone.

Role bits are compositional: all are faction members, the first is leader
(authority 5), the second is command (4), and the third is an important dealer
(2). Additional important members use senior authority (3). Leaders and deputies
can issue future missions, but no missions or dealer economy are generated.

WorldLocation now groups district, territory and optional BuildingId; it replaces
the old standalone homeDistrict field without duplicating district authority.
Important NPCs begin in their faction home territory, preferring appropriate
existing buildings (office/residential leadership, store/garage/warehouse dealer).
If no unused suitable building exists, a coherent territory location is retained.
Building and NPC identities remain domain data, independently of spatial entities.

`actor_types.hpp` is shared by generated records and runtime domain stores.
Generated records wrap the same state with a typed ID; runtime stores own the
mutable copy keyed by that ID. Generated data is an immutable baseline, not a
second mutable authority. GameState imports NPCs, factions, presence and relations,
advances allocators past generated IDs, and creates no physical NPC bodies.

## Relationships and causal context

Faction relationships have separate directed trust, hostility, fear, shared
interest and historical grievance. All 20 ordered pairs are stored; no symmetric
scalar friendship shortcut is assumed. Context flags come from shared borders,
shared family, similar strategic priorities or geographic separation. Trust and
hostility incorporate those facts plus personality; fear compares actual manpower,
treasury and aggression. FRIENDLY/NEUTRAL/TENSE/HOSTILE are derived summaries.
There is no WAR state or changing alliance logic at generation time.

NPC relationships are sparse directed leadership ties between each leader and
important subordinate, plus at most one rival leader pair grounded in a real
political border. Personal trust, fear, hostility and loyalty remain separate.
Historical grievance and personal debt are zero: there are no invented past wars,
betrayals, favors, chronology, story nodes or four-year events. Context is a
baseline world fact, not a fabricated history. The later history stage may add
actual historical causes using future shared simulation rules.

## Validation, hashing and tests

M3 validation requires five factions with family counts 1/1/1/2, unique IDs/names/
homes, valid leaders and member structure, coherent semantic locations, bounded
values, ALIVE major NPCs without bodies, valid ownership/presence, connected
control and viability. Relationships require valid distinct endpoints, unique
directed keys, bounded dimensions, supported context and sparse personal records.
Prior geometry validators remain unchanged and execute before actor validation.

`FAULTLINE.base.M3.v3` includes all v2 canonical geometry/configuration and all
new faction, NPC, territory assignment and relationship fields, in explicit stable
order. No pointers, caches or wall-clock values enter the hash. Runtime StateHash
schema advances to `FAULTLINE.state.v2` because domain state fields have expanded;
this does not change v1/v2 BaseWorldHash compatibility. No save files exist yet.

Fast CTest labels cover foundation/checkpoint and cross-process determinism for
versions 1–3. The default full build still runs the required 1000-seed stress test,
now collecting duplication frequency, NPC counts, territory shares and diplomatic
state counts. Timing is tool-only and appears only in batch output.

## Limits and next boundary

Names use a small finite deterministic catalogue with bounded collision handling.
Home preferences are soft archetype affinities rather than a demographic model.
Initial garrisons are aggregates, with no independently simulated generic members.
Some territory may remain neutral, and a neutral gap is a legitimate initial
political separation. Important NPCs are faction-affiliated; independent major
police figures are not generated. Physical travel does not yet update semantic
territory/building references automatically; that belongs to NPC/location systems.

Runtime faction AI, market, compressed history, story and missions remain absent.
After Phase 7 review, the roadmap's next task is Phase 8: semantic save/load,
version header, RNG/allocator persistence and StateHash round-trip validation.
