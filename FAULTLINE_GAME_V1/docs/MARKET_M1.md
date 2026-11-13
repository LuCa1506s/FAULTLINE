# District Market M1 / Phase 12

MarketSystem owns district demand, supply, price, participation and dealer economic
assignments. Faction treasuries remain the sole money balance. TerritorySystem
retains all physical control authority. All scores are fictional strategic units.
No production, realistic quantities, consumption or logistics are simulated.

## Initialization and ownership

Every existing district receives a runtime market; the frozen generated base is
untouched. For generated districts, baseline demand is min(10000, 100 + population/20
+ wealth*5), sensitivity is 40 + archetype ordinal*4, and accessibility is
min(10000, 7000 + density*30). Foundation districts without generated profiles
start with zero baseline demand; callers may configure their runtime baseline.
Baseline demand can be zero and the demand scale can reach zero. There is no
artificial floor preventing market destruction.

Existing semantic Dealer roles register once with their current territory and
capacity 400 + business/10. New registration requires a real NpcId, Dealer role,
faction and territory. It allocates no NPCs or aggregate members, and duplicate
registration fails. Assignment and enabled status persist independently of
physical representation. Daily NPC travel does not relocate the economic network.
A dead, injured, arrested, missing, disabled, role-less, reassigned-faction or
zero-health dealer contributes zero. Active health comes from its authoritative
body; after save/abstraction the synchronized semantic health gives the same result.
Records remain for history; no automatic replacement or resurrection occurs.

## Scheduled market calculations

Default interval: one game hour (1600 fixed ticks). Rules allow 1..38400 ticks;
price response divisor defaults to 4, disruption recovery to 250 basis points per
update. Near clock exhaustion, the deadline becomes the existing neverTick sentinel.
Updates run after faction operation/territory consequences and reconciliation,
before due faction strategy and NPC decisions. The headless runner includes the
market deadline in its minimum, using the same normal tick pipeline. Repeated calls
at the same tick pay once. Late deadlines reject rather than replay stale economics.
Market calculations consume no RNG, including initialization and migration.

All multiplication intermediates are bounded 64-bit integers. Demand and supply
are 0..1,000,000 strategic units. Effective dealer capacity receives a territory
access multiplier: own controlled 10000, rival controlled 6500, uncontrolled 8000,
contested 5000 basis points. Rivals retain access under foreign physical control;
market shares never grant or overwrite territory control or garrison membership.
District accessibility and (10000 - disruption) further multiply capacity.

Demand multiplies baseline by demandScale/10000, (10000 - disruption/4)/10000,
and (10000 - saturation)/10000. Saturation is min(1000, previousSupply*1000 /
baselineDemand), or zero for zero baseline. Each division truncates in that order.
Disruption accumulates up to 10000 and decays after a paid update; it therefore
survives multiple updates and save/load. The explicit disruption API is available
for causal system effects; M1 does not add dealer attack operations or police AI.

Faction supply is its effective capacity apportioned within the district limit.
Shares apportion exactly 10000 basis points across supplied capacity, or zero when
supply is zero. Sales apportion min(demand,supply) across those same suppliers.
Integer floors are followed by ascending FactionId remainder allocation to positive
weights. Sales never exceed supplied capacity, including saturation at the limit.
Zero-participation accounting records remain, with zero current share/supply/sales.

Price target = clamp(reference + reference*(demand-supply)*sensitivity /
max(demand,supply,1)/100, minimum, maximum), using signed integer division toward
zero. Defaults are reference 100, minimum 50, maximum 200. Price moves toward target
by ceil(abs(target-price)/responseDivisor), preventing both overshoot and rounding
stalls. District configuration allows positive bounded prices up to 10000 and
sensitivity 0..100. No exponential feedback or profit-driven capacity creation exists.

## Revenue accounting

For each faction/district, income numerator is previous fractional remainder plus
sold*price*intervalTicks. Credit is numerator/(38400*100); the modulus persists for
the next payment. This time-scales revenue and avoids losing fractional earnings
with short intervals. No unsatisfied demand earns money. Credit goes directly to
the authoritative faction treasury exactly once, alongside a historical credited
counter. Credits saturate at int64 treasury and uint64 counter headroom; excess is
not retained as a deferred liability. There is no second spendable market balance.

The simulator reports market update count, credited income, total abstract supply
and demand. Treasury spending now sums actual newly paid operation commitments,
avoiding unsigned underflow when revenue exceeds spending. Integration tests require
initial treasury + market credits - paid operation costs = current treasury.

## Persistence and limits

SaveFormatVersion 6 appends section 14; StateHash schema v8 covers all market
rules, deadlines, updates, district values/modifiers, per-faction accounting and
dealer assignments. Readers accept v1..v6. See SAVE_FORMAT_V6.md and
PHASE12_VERIFICATION.md. GeneratorVersion 3 and GameVersion 0.1.0 are unchanged.

This is the first revenue loop, not final economic balance. Existing generated
Dealer roles provide one network per main faction; districts without dealers
naturally have zero supply. City-wide dealer spread and four-year history belong
to Phase 13. Recruitment, paid replacement/expansion strategy, police pressure,
advanced faction collapse, missions and physical dealer AI remain later work.
The economy can weaken to zero and long-term treasury growth is linear and finite;
it does not replenish lost aggregate manpower or manufacture dealers.
