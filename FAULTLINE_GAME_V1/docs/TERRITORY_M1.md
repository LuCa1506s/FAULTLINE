# TerritorySystem M1 - Phase 11

Territories remain persistent semantic domains, independent of cells, streaming,
physical NPC entities, mission flags or rendering. Generated geometry and initial
assignments are immutable. Runtime data belongs to GameState.territorySimulation.

## Presence and resources

Sparse ordered per-faction entrenchment uses integer strategic units, default range 1..10000;
zero is represented by an absent entry. It describes a causally established local
foothold, not additional people. Effective presence is the saturating sum of this
foothold and concrete resident garrison members times 20. Existing residentMembers
is the single manpower count. New garrisonFaction records its actual affiliation,
independent of control: capturing a territory never grants another faction its
surviving soldiers. Faction resource totals count garrisons by this affiliation.

Initialization imports the existing owner/map without regeneration, adding 120
entrenchment and another 100 at the owning faction's home. Every owned region
starts Controlled by the same faction; neutral regions start Uncontrolled. Homes
have a robust but destructible foothold: no bonus is regenerated on reevaluation.
Initial entrenchment represents existing generated local structure; subsequent
changes require actual operation outcomes. No passive random gain, decay, market,
dealer economy, full civilian simulation or police pressure is introduced.
Local resistance begins at zero, preserving every imported map.

## Control and hysteresis

Rules are centralized and persistent. Find the largest and second-largest
presence, using ascending FactionId for stable ordering:

* Top below 100 -> Uncontrolled, no controller.
* Opposition = max(second-largest faction presence, local resistance).
* Opposition >=60 and top-minus-opposition below the required margin -> Contested,
  no controller.
* Otherwise Controlled by the top faction.

Required margin is 80 to acquire control and 60 to retain it. The minimal
persisted incumbent latch remembers held control; losing control clears it.
For example, a 59-unit advantage loses held control; restoring a 60-unit advantage
does not instantly reacquire it, which needs 80. This is real temporal hysteresis,
not floating-point epsilon or random tie breaking. Presence, local resistance, rules and this
explicit history modifier are authoritative inputs. Control enum/controller are
derived; an unpersisted cache and the legacy owner wire mirror are validated.
No faction-owned-territory list or independent count is authoritative.

## Operation consequences

FactionSystem still reserves/pays resources, computes aggregate outcomes and
casualties, and returns survivors. It never assigns territory ownership. Runtime
attack/defense targets use current derived control/presence and existing semantic
adjacency, not generated assignments. A controlled target's controller is its
primary defender. In contested/uncontrolled regions, surviving garrisons or the
strongest other meaningful presence identify a primary opponent. Residual foreign
garrisons never hide a new controller; their manpower remains with their faction.
Only the selected defender's garrison participates in that aggregate battle.
Multi-party tactical battle simulation is outside this milestone.

Successful attack foothold = floor(floor(strategicPressure * survivors /
committedForce) / 20). Add it to the attacker and subtract it from defender
entrenchment. Garrison casualties already lower the concrete contribution once;
no second manpower subtraction occurs. A failed attack removes attacker foothold
proportional to its aggregate losses, without inventing a defender bonus.

A defense adds 5 foothold units per surviving committed member and removes the
same attacker foothold on success, half on failure. This is a paid, surviving
reinforcement, not a free strength grant. All influence changes clamp to 0..10000;
no force is created and no generic recovery/recruitment/income is added.

Results are consumed exactly once using a persisted set of OperationIds. Stable
OperationId processing batches all simultaneous effects for each target, then
reevaluates it once in TerritoryId order. This avoids false intermediate control
events between one attack and its linked defense. Each actual state/controller
transition emits one scoped event with territory, previous/new state/controller,
and tick. Unchanged results emit nothing.

The shared fixed/headless pipeline is: resolve faction operations -> consume
territorial consequences -> cancel stale operation targets and release their
commitments -> scheduled faction evaluation -> semantic NPC update -> structural
commit. Control changes never recursively run strategic AI. Current reality is
visible at the next scheduled evaluation. Cancellations have no territorial effect.

## Local resistance

Roadmap section 15 and GDD section 54 require a local-resistance control factor.
M1 models local opposition to criminal control through one bounded integer per
territory. It belongs to no faction, contributes no soldiers or foothold, cannot
be selected as a defender and cannot claim an empty territory. It competes with
the strongest faction using the existing contest threshold and gain/hold margins.
Even without rival entrenchment, sufficiently resisted criminal presence becomes
Contested and clears its controller/latch through the ordinary event path.
Below-minimum faction presence remains Uncontrolled regardless of resistance.

Each newly consumed attack outcome raises resistance by (ownLosses +
opposingLosses) * resistanceCasualtyUnit, default 5, capped at maximumPresence.
These local battle casualties proxy the disruption causing opposition; they are
not civilian casualty records. Garrison losses are already included in opposing
losses. A linked defense reports the same fight, so it never adds casualties a
second time. Successful defense relieves resistance by surviving committed
members * resistanceReliefUnit, default 2, clamped at zero. Failed defense and
cancelled operations grant no relief. Forces and mobilization remain paid by
FactionSystem. Zero units explicitly disable their respective contribution.

The existing exactly-once ledger protects these effects on load and replay.
OperationId order applies the attack's disruption before its later-created
linked defense's relief, then reevaluates the target once. No RNG, elapsed-time
decay, passive faction penalty, civilian entities, gang creation or new scheduler
is needed. Resistance persists until a causal operation changes it; detailed
civilian sentiment/recovery is outside this minimal Phase 11 model.

## Persistence and scope

Save v5 preserves presence inputs, local resistance, garrison affiliation, hysteresis, consumption
ledger and fixed-size statistics; derived caches are rebuilt. Old saves initialize
from saved map/garrisons and acknowledge past results without replaying them.
Active old operations resolve through the new rules in the future. Runtime
StateHash v7 includes all semantic territorial data; BaseWorldHashes are frozen.
See SAVE_FORMAT_V5.md and PHASE11_VERIFICATION.md for encoding and actual gates.

Territory counts are derived from Controlled/controller, never top contender.
Zero-controlled factions remain alive with resources, NPCs and home identity.
They may defend actual local presence, but new attacks require adjacency from a
currently controlled region; without one they temporarily cannot expand. Explicit
base/recovery operations, supply lines, retreat/occupation of residual garrisons,
faction collapse, diplomacy, market and systemic anti-snowballing are deferred.
No special home-loss confidence penalty is added; normal battle morale already
applies and strategy reads the changed map. No physical/entity or NPC death logic.
