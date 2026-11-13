# Four-year strategic history (Phase 13)

`beginStrategicHistory` and `advanceStrategicHistory` implement the structural
transition over an existing generated prologue world. The explicit input records
FIRST DELIVERY completion, selected origin faction, rival, witness, location,
seed-selected incident family and ordered delivery/discovery ticks. Candidate
selection never completes a mission. Only tools/tests explicitly mark fixture
evidence complete. The Phase 16 mission will supply runtime evidence; no mission
or story graph is implemented here.

The immutable generator-v3 geography/actors remain the prehistory base identity.
The transition produces a complete post-timeskip semantic GameState, including
its versioned history and market. Runtime transition effects never rewrite
BaseWorldHash. This preserves generator v1/v2/v3 fixtures and older saves while
making the history stage independently reproducible from base + prologue inputs
+ HistoryVersion 1. The selected faction is an explicit input, not a hidden global.
`faultline_history` assembles and validates this post-timeskip world headlessly.

## Coarse causal rules

* Exactly 48 monthly decisions partition 1460 game days using integer boundaries
  `floor(month * 1460 / 48)`. The clock moves directly between daily economic
  boundaries. There are 1460 actual market updates and no Simulation::tick calls.
* Initially only the origin and discovered rival activate their existing semantic
  dealers. Observable distribution informs one remaining faction per month;
  all five enter by month 3. Each entry records its information source. This is
  deliberately coarse information propagation, not a runtime knowledge system.
* A prepared month permits one dealer investment per informed faction. Utility
  reads unmet district demand, existing same-faction dealers, graph distance,
  current territorial security and market focus. Distant expansion costs more.
  An isolated seed/month stream rotates evaluation order. Allocated NpcIds and
  deterministic identities remain stable. No runtime RNG stream is consumed.
* Investments cost 600 + 100 per territory-graph hop and retain a 5000 treasury
  buffer and deployable reserves. One real reserve member becomes one persistent
  Dealer/FactionMember with a valid semantic location and physical spawn position.
  Capacity is bounded 400..1200 and shares/sales/prices/income come entirely from
  the existing MarketSystem. No final market shares are assigned.
* Adjacent uncontrolled sites can be settled for 1000 and six actual reserve
  members. Garrison membership and existing TerritorySystem thresholds decide
  control. No rival garrison transfers with control. A reserve buffer and finite
  real personnel limit expansion; factions receive no free recovery or money.
* Each operating dealer pays monthly network support (20 + capacity/20). Paid
  activity earns at most ten influence units/month up to the smaller of
  `TerritoryRules::maximumPresence` and 100 + capacity/10;
  stronger preexisting entrenchment is preserved. TerritorySystem reevaluates
  control from this influence, garrisons, hysteresis and local resistance.
* Shared markets and historical borders create grievance, hostility and reduced
  trust. Noncompeting networks build trust/shared interest. Annual causal records
  summarize those pressures. RelationshipSystem applies personal worked-with or
  threatened events to the relevant leaders and dealer/leader pairs. Friendly
  relationships feed the existing runtime diplomacy/utility interpretation;
  advanced alliance/war operations remain later roadmap work.
* Repeated support payments are compressed into one monthly faction cost record.
  Annual disputes/cooperation, individual dealer investments, settlements and
  market entries retain stable actor/territory references. No daily market log,
  fixed tick log, cache or physical entity is stored in history.

Daily economic calculations reuse Phase 12's bounded/damped rules, with a daily
interval during history. Original market rules are restored at each monthly
save boundary. Faction and NPC runtime deadlines are rescheduled at that boundary;
there are no discarded operations/travel commitments. Ordinary runtime ticking
rejects incomplete history, including before headless clock movement. Once month
48 completes the normal simulation resumes without an equilibrium-restoring rule.

## Validation and scope

Entry requires a viable generated five-faction, entity-free world, empty operation
store and living actors without travel/operation commitments. Invalid prologue,
repeat entry and clock overflow reject before mutation. Every monthly step checks
history/reference/order, market, faction, territory and NPC invariants. Full base
geometry validation runs at entry, completed output and save/load, rather than
48 times over immutable geometry. Completed-output validation requires demand and
supply in every district, bounded prices below the permanent cap, all five
participating with resources, living leadership and controlled territory, no
faction at 80% of city territory/supply, and a meaningful unresolved tension.
Those output conditions reject invalid generated outcomes; they never recolor,
revive or subsidize a faction. They are not enforced as runtime balance rules.

This history level models market growth, peaceful adjacent settlement and paid
presence/political competition. It does not replay four years of runtime warfare,
randomly kill major NPCs, implement succession, recruit free manpower, replace
dead dealers, create story nodes or implement FIRST DELIVERY. Phase 14 is untouched.
History rules/version are explicit; future incompatible history generation must
change HistoryVersion and define its continuation policy independently of the
frozen prehistory generator algorithms.

## Tool examples

```powershell
build/faultline_history.exe --seed 12345 --origin 1 --fixture-prologue --replay --save build/history.flsave
build/faultline_history.exe --seed 12345 --fixture-prologue --months 12 --save build/history-year1.flsave
build/faultline_history.exe --load build/history-year1.flsave --months 48 --save build/history-resumed.flsave
build/faultline_sim.exe --load build/history.flsave --days 365
build/faultline_history.exe --range 1 250 --fixture-prologue --replay
build/faultline_history.exe --range 1 1000 --small --fixture-prologue --replay
```

The tool requires `--fixture-prologue` for fresh generation, clearly separating a
headless fixture from completion of a real mission. `--months` is an absolute
history month, not an additional duration. `--load` resumes persisted history.
`--replay` independently replays every selected transition from the same initial
snapshot and compares StateHash. Independent-process CTest compares complete
stdout. The final completed state may be saved and consumed by the normal simulator.
