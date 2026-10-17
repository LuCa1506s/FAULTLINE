# Story graph foundation

The story graph is persistent semantic domain data, distinct from a mission
objective graph. It projects opportunities; it neither executes objectives nor
changes NPCs, factions, relationships, territories, markets, history or resources.
It requires no player, renderer or physical entities. FIRST DELIVERY remains
Phase 16. No Phase 15 mission implementation is included.

## Initialization and context

`initializeStoryGraph(GameState&)` is explicit at the completed-history bootstrap
boundary. It validates the source state, requires HistoryVersion 1 / 48 months,
and rejects reinitialization without destroying progressed nodes. No history is
replayed. Existing Phase 13 history tools retain their behavior.

StoryGraphVersion 1 contains stable `StoryNodeId` keys in a DomainStore, with its
own allocator. For each faction, deterministic ascending-ID generation creates
contact, boss-operation, betrayal, dealer-conflict, market-change and succession
possibilities, plus territorial-conflict and alliance possibilities for each
other faction. The standard five-faction graph has 70 nodes and 60 dependency
edges. These include latent, currently impossible possibilities; their existence
does not assert that an alliance/betrayal/war has happened.

Nodes reference actual factions, semantic NPCs, territories and districts.
Territorial counterparts bind to an actual currently controlled region, preferring
the lowest TerritoryId. Dealer nodes bind to a real enabled, positive-capacity
dealer and its actual market location. A one-based compressed history record
index supplies provenance: the most recent record for the faction/counterpart,
or zero if no matching context exists. History records remain immutable.

## Lifecycle and dependencies

Authoritative outcome: Unresolved, Succeeded or Failed. Resolution is an explicit
external integration hook, allowed only for an eligible node, once. It pays no
rewards and executes no mission. Availability is derived on demand:

* Closed: outcome has already been recorded.
* Invalidated: live world prerequisites are false, or a dependency resolved to an
  incompatible outcome.
* Locked: world facts are eligible but an upstream outcome remains unresolved.
* Eligible: live facts and all explicit prerequisite outcomes match.

World invalidation is reversible when actual conditions recover. Closed outcomes
are historical and never repaired away. Availability is not separately cached or
serialized. Mutually exclusive success/failure continuations use opposite outcomes
of the same contact node: successful contact enables boss/conflict/alliance/dealer
or betrayal branches, while failed contact can enable a market-change alternative.
There is no additional general-purpose scripting/exclusion language.

Prerequisites are an AND list sorted by strictly increasing StoryNodeId. Every
edge points backward to an existing node. This admits shared prerequisites and
branches while rejecting cycles, self edges and duplicate/contradictory entries.
Contact and succession are roots; all other kinds require a prerequisite. A node
can be structurally reachable while world-ineligible, by design.

## World predicates

Both referenced factions must be Active/Reformed, have positive treasury and an
actual living leader. A contact must be living, positive-health, in that faction,
and either important or a MissionIssuer; dealer nodes instead require the actual
enabled dealer role/market registration. Further conditions are purpose-built:

* boss operation: an actual usable faction leader;
* succession: original leader dead and an actual different leader already exists;
* betrayal: non-leader contact ambition exceeds loyalty, and faction cohesion
  below 5000 or a directed relationship to the leader has hostility >=6000 or
  personal loyalty <3000;
* dealer conflict: dealer region not currently controlled by its faction;
* market change: actual district demand exceeds supply;
* territorial conflict: directed political hostility >=4500 and the counterpart
  still controls the bound region;
* alliance: directed trust >=6000 and hostility <4500.

These are eligibility rules for possibilities, not strategic actions or assertions
of final outcomes. Losing a region invalidates that particular opportunity;
repair does not move its target to manufacture a replacement territorial conflict.

## Observation and repair

`repairStoryGraph` deterministically rebinds unresolved invalid contacts using
existing eligible actors only: important status, authority, loyalty, leadership,
Respect, then lowest NpcId. Dealer substitutions also update semantic location.
Betrayal uses a non-leader important/issuer candidate with the largest
ambition-minus-loyalty difference. With no valid candidate the contact becomes
NONE and availability explicitly becomes Invalidated. Historical original contact
IDs remain resolvable even after death or changed membership. No NPC is allocated,
resurrected, promoted to leader, transferred to another faction or granted immunity.

An organizational succession fixture first changes the world's leader through an
explicit external decision, then kills the former leader through the existing NPC
lifecycle API. Story repair only observes those facts; Phase 14 does not implement
the future organizational succession system.

Normal Simulation calls repair after scoped faction/NPC/territory events or market
updates. Idle 20 Hz ticks never scan the graph. Foreign mutations made outside
Simulation must explicitly call repair at their consistent semantic boundary.
Availability always queries current facts, including before repair. There is no
universal event bus or recursive world-strategy callback. Repeated repair is
idempotent; progressed outcomes and node IDs remain stable.

## Persistence and determinism

Save v8 appends section 16; StateHash `FAULTLINE.state.v10` includes graph version,
allocator and every node field in stable ID order. No unrelated runtime RNG is
consumed by generation, queries, resolution or repair. GeneratorVersion stays 3,
HistoryVersion stays 1 and GameVersion stays 0.1.0. BaseWorldHash excludes story.
All old save migrations preserve their world and default to an uninitialized graph;
initialization is explicit and never invents missing prologue evidence.

The validator rejects unsupported versions, malformed empty graphs, excessive
nodes, duplicate semantic possibilities, invalid enums/references/locations,
wrong contact affiliation, bad history provenance and invalid dependencies.
Dead historical actor references are valid records, not dangling live assignments.
The loader does not repair corrupt references. See SAVE_FORMAT_V8.md.

## Tool

```powershell
build/faultline_story.exe --seed 12345 --fixture-prologue --save build/story.flsave
build/faultline_story.exe --range 1 100 --fixture-prologue
build/faultline_story.exe --load build/history.flsave
ctest --test-dir build -L story --output-on-failure
```

Fresh tool worlds require an explicitly synthetic prologue fixture, then the real
Phase 13 transition. This is test/bootstrap tooling, never runtime FIRST DELIVERY.
Each batch world independently initializes twice, compares canonical bytes,
round-trips persistence and validates a deterministic important-dealer death on a
copy. Counts cover nodes, dependencies, eligible opportunities, major contacts,
faction coverage and contact repair/invalidation. No uniform balance target is
enforced. Limitations: fixed coarse opportunity families, no mission instantiation,
no rewards/dialogue, no automated organizational succession, no dynamically
authored arbitrary conditions or graph replacement on every world change.
