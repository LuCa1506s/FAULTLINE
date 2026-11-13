# Save format v6

SaveFormatVersion is 6; GeneratorVersion remains 3; GameVersion remains 0.1.0.
Readers accept v1..v6 and generator v1..v3. Existing framing, little-endian
primitives, bounded collections, UTF-8 and typed error diagnostics are unchanged.
There are 14 required ordered sections. Sections 1..13 retain their exact v5 wire
encoding, including the complete resistance extension of section 13.

Section 14 (MarketSimulation), in order:

1. Rules: u64 intervalTicks, u32 priceResponseDivisor, u32 disruptionRecovery.
2. u64 nextUpdateTick, u64 updates.
3. u32 district count, then ascending district records: u64 DistrictId;
   eleven u32 values (baselineDemand, demandScaleBps, demand, supply,
   referencePrice, minimumPrice, maximumPrice, price, sensitivity,
   accessibilityBps, disruptionBps); u32 participation count; ascending records
   of u64 FactionId, three u32 values (supply, shareBps, sold),
   u64 incomeRemainder, u64 creditedIncome.
4. u32 dealer count; ascending records of u64 NpcId, u64 FactionId,
   u64 TerritoryId, u32 capacity, bool enabled (one canonical byte).

IDs must be nonzero, strictly ascending, unique and refer to existing domain
records. Market coverage must match all districts. Numeric bounds, exact total
supply/share/sales, fractional remainder, rule and scheduler ranges are validated.
Old samples may remain until the next update after a dealer dies or a modifier
changes; validation does not incorrectly require last-sample values to equal new
inputs. Market state is never regenerated over a v6 load.

The migration from v5 initializes only market runtime, after old faction/NPC/
territory migrations and control-cache reconstruction. Existing Dealer roles
receive economic assignments and bounded baseline district profiles; current
supply/share samples are calculated without paying, advancing time, consuming RNG
or replaying historical effects. The first payment is savedTick + intervalTicks.
V1..v4 migrations retain their previous steps, then apply this same v5-to-v6 market
initialization. All historical field values, RNG, allocators, resources and base
identity are preserved. Historical files cannot preserve the old phase's future
runtime trajectory once the new market system begins earning income.

The complete market schema is included in runtime StateHash FAULTLINE.state.v8.
Fractional income, disruption memory, disabled dealers and custom rules therefore
continue deterministically after load. No new pointer, physical entity or cache
is part of the economic state. Older readers reject v6 as an unknown version.

The frozen real v5 day-30 fixture preserves all thirteen old section payloads
byte for byte through migration. Existing frozen v1/v2/v3/v4 fixtures remain
unchanged. See tests/fixtures/README.md for provenance and hashes.
