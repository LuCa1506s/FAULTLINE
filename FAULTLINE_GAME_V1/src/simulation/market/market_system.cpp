#include "simulation/market/market_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include <algorithm>
#include <limits>

namespace faultline::simulation {
namespace {
std::uint64_t nextTick(std::uint64_t tick,std::uint64_t interval) {
    return interval>core::SimulationClock::maxTick-tick ? neverTick : tick+interval;
}
// Exact, stable apportionment. The sum equals total; zero weights never receive units.
std::map<FactionId,std::uint32_t> allocate(const std::map<FactionId,std::uint64_t>& weights,std::uint32_t total) {
    std::uint64_t sum=0; for (const auto& [id,w] : weights) { (void)id; sum+=w; }
    std::map<FactionId,std::uint32_t> result;
    if (sum==0) return result;
    std::uint32_t remaining=total;
    for (const auto& [id,w] : weights) { result[id]=static_cast<std::uint32_t>(w*total/sum); remaining-=result[id]; }
    for (auto& [id,value] : result) if (remaining && weights.at(id)>0) { ++value; --remaining; }
    return result;
}
void calculate(GameState& state,bool pay) {
    auto& s=state.marketSimulation;
    std::map<DistrictId,std::map<FactionId,std::uint64_t>> capacities;
    for (const auto& [id,d] : s.dealers) {
        const auto& n=state.world.npcs.get(id);
        const auto& f=state.world.factions.get(d.faction);
        const auto health=n.activeEntity ? state.world.health.get(n.activeEntity).current : n.health.current;
        if (!d.enabled || !n.roles.has(world::NpcRole::Dealer) || n.faction!=d.faction ||
            n.lifecycle!=world::NpcLifecycle::Alive || health<=0 ||
            (f.lifecycle!=world::FactionLifecycle::Active && f.lifecycle!=world::FactionLifecycle::Reformed)) continue;
        const auto district=state.world.territories.get(d.territory).district;
        const auto control=territoryControl(state,d.territory);
        // Rivals retain access; physical control never grants a market monopoly.
        const std::uint32_t access=control.state==TerritoryControlState::Contested ? 5000U :
            control.controller==d.faction ? 10000U : control.controller ? 6500U : 8000U;
        capacities[district][d.faction]+=static_cast<std::uint64_t>(d.capacity)*access/10000;
    }
    for (auto& [id,m] : s.districts) {
        const auto saturation=m.baselineDemand==0 ? 0U : std::min<std::uint64_t>(1000,static_cast<std::uint64_t>(m.supply)*1000/m.baselineDemand);
        m.demand=static_cast<std::uint32_t>(static_cast<std::uint64_t>(m.baselineDemand)*m.demandScaleBps/10000*
            (10000-m.disruptionBps/4)/10000*(10000-saturation)/10000);
        auto weights=capacities[id];
        std::uint64_t total=0;
        for (auto& [f,value] : weights) { (void)f;
            value=value*m.accessibilityBps/10000*(10000-m.disruptionBps)/10000; total+=value;
        }
        m.supply=static_cast<std::uint32_t>(std::min<std::uint64_t>(marketUnitLimit,total));
        const auto supplies=allocate(weights,m.supply);
        std::map<FactionId,std::uint64_t> supplied;
        for (const auto& [f,v] : supplies) supplied[f]=v;
        const auto shares=allocate(supplied,10000);
        const auto sales=allocate(supplied,std::min(m.demand,m.supply));
        const auto imbalance=static_cast<std::int64_t>(m.demand)-m.supply;
        const auto denominator=std::max({m.demand,m.supply,1U});
        const auto target=std::clamp<std::int64_t>(m.referencePrice+
            static_cast<std::int64_t>(m.referencePrice)*imbalance*m.sensitivity/denominator/100,m.minimumPrice,m.maximumPrice);
        if (pay) {
            const auto delta=target-m.price;
            const auto step=(std::abs(delta)+s.rules.priceResponseDivisor-1)/s.rules.priceResponseDivisor;
            m.price=static_cast<std::uint32_t>(static_cast<std::int64_t>(m.price)+(delta<0 ? -step : step));
        }
        // Retain zero-participation records for accounting history; never pay them stale shares.
        for (auto& [f,value] : m.factions) { (void)f; value.supply=0; value.shareBps=0; value.sold=0; }
        for (const auto& [f,value] : supplies) {
            auto& entry=m.factions[f]; entry.supply=value; entry.shareBps=shares.at(f); entry.sold=sales.at(f);
            if (pay) {
                const auto numerator=entry.incomeRemainder+static_cast<std::uint64_t>(entry.sold)*m.price*s.rules.intervalTicks;
                const auto earned=numerator/marketIncomeDenominator;
                entry.incomeRemainder=numerator%marketIncomeDenominator;
                auto& treasury=state.world.factions.get(f).treasury;
                const auto credit=std::min({earned,static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()-treasury),
                    std::numeric_limits<std::uint64_t>::max()-entry.creditedIncome});
                treasury+=static_cast<std::int64_t>(credit); entry.creditedIncome+=credit;
            }
        }
        if (pay) m.disruptionBps=m.disruptionBps>s.rules.disruptionRecovery ? m.disruptionBps-s.rules.disruptionRecovery : 0;
    }
}
}
void refreshMarkets(GameState& state) { calculate(state,false); }
void registerMarketDealer(GameState& state,NpcId id,TerritoryId territory,std::uint32_t capacity) {
    const auto& n=state.world.npcs.get(id);
    core::require(n.roles.has(world::NpcRole::Dealer) && n.faction && state.world.factions.exists(n.faction) &&
        state.world.territories.exists(territory) && capacity<=10000,"Invalid dealer registration");
    core::require(!state.marketSimulation.dealers.contains(id),"Dealer already registered");
    state.marketSimulation.dealers.emplace(id,DealerState{n.faction,territory,capacity,true});
}
void initializeMarketSimulation(GameState& state) {
    auto& s=state.marketSimulation; s=MarketSimulationState{};
    s.nextUpdateTick=nextTick(state.clock.tick(),s.rules.intervalTicks);
    for (const auto& [id,d] : state.world.districts.entries()) { (void)d;
        DistrictMarketState m;
        if (state.world.base) for (const auto& g : state.world.base->districts) if (g.id==id) {
            // Population, economic profile and identity are abstract demand inputs.
            m.baselineDemand=static_cast<std::uint32_t>(std::min<std::uint64_t>(10000,100+g.population/20+g.wealth*5));
            m.sensitivity=40+static_cast<std::uint32_t>(g.archetype)*4;
            m.accessibilityBps=7000+g.density*30;
            m.accessibilityBps=std::min(m.accessibilityBps,10000U);
        }
        s.districts.emplace(id,m);
    }
    for (const auto& [id,n] : state.world.npcs.entries())
        if (n.roles.has(world::NpcRole::Dealer) && n.faction && n.location.territory)
            registerMarketDealer(state,id,n.location.territory,400+n.capabilities.business/10);
    calculate(state,false); // No revenue, time advancement, RNG or historical replay during initialization/migration.
}
void disruptDistrictMarket(GameState& state,DistrictId id,std::uint32_t severity) {
    core::require(severity<=10000,"Invalid market disruption");
    auto& m=state.marketSimulation.districts.at(id);
    m.disruptionBps=std::min(10000U,m.disruptionBps+severity);
}
void updateMarkets(GameState& state) {
    auto& s=state.marketSimulation;
    if (s.nextUpdateTick==neverTick || state.clock.tick()<s.nextUpdateTick) return;
    core::require(state.clock.tick()==s.nextUpdateTick,"Missed market deadline");
    core::require(s.updates!=neverTick,"Market update counter overflow");
    calculate(state,true); ++s.updates;
    s.nextUpdateTick=nextTick(state.clock.tick(),s.rules.intervalTicks);
}
void validateMarketSimulation(const GameState& state) {
    const auto& s=state.marketSimulation;
    core::require(s.rules.intervalTicks>0 && s.rules.intervalTicks<=gameDayTicks && s.rules.priceResponseDivisor>0 &&
        s.rules.priceResponseDivisor<=1000 && s.rules.disruptionRecovery<=10000,"Invalid market rules");
    core::require(s.nextUpdateTick==neverTick || (s.nextUpdateTick>=state.clock.tick() && s.nextUpdateTick<=core::SimulationClock::maxTick),"Invalid market deadline");
    core::require(s.updates<=state.clock.tick(),"Invalid market update counter");
    core::require(s.districts.size()==state.world.districts.entries().size(),"Market district coverage mismatch");
    for (const auto& [id,d] : s.dealers) {
        core::require(state.world.npcs.exists(id) && state.world.factions.exists(d.faction) &&
            state.world.territories.exists(d.territory) && d.capacity<=10000,"Invalid dealer reference/capacity");
    }
    for (const auto& [id,m] : s.districts) {
        core::require(state.world.districts.exists(id) && m.baselineDemand<=marketUnitLimit && m.demandScaleBps<=10000 &&
            m.demand<=marketUnitLimit && m.supply<=marketUnitLimit && m.minimumPrice>0 && m.maximumPrice<=10000 &&
            m.minimumPrice<=m.referencePrice && m.referencePrice<=m.maximumPrice && m.price>=m.minimumPrice && m.price<=m.maximumPrice &&
            m.sensitivity<=100 && m.accessibilityBps<=10000 && m.disruptionBps<=10000,"Invalid district market bounds");
        std::uint64_t shares=0,supply=0,sold=0;
        for (const auto& [f,e] : m.factions) {
            core::require(state.world.factions.exists(f) && e.shareBps<=10000 && e.supply<=marketUnitLimit && e.sold<=e.supply &&
                e.incomeRemainder<marketIncomeDenominator && (e.supply>0 || e.shareBps==0),"Invalid market share/accounting");
            shares+=e.shareBps; supply+=e.supply; sold+=e.sold;
        }
        core::require(supply==m.supply && shares==(m.supply ? 10000U : 0U) && sold==std::min(m.supply,m.demand),"Inconsistent district market totals");
    }
}
void appendMarketSimulationHash(core::StateHash& h,const GameState& state) {
    const auto& s=state.marketSimulation;
    h.u64(s.rules.intervalTicks); h.u64(s.rules.priceResponseDivisor); h.u64(s.rules.disruptionRecovery);
    h.u64(s.nextUpdateTick); h.u64(s.updates); h.u64(s.districts.size());
    for (const auto& [id,m] : s.districts) {
        h.u64(id.value());
        for (auto v : {m.baselineDemand,m.demandScaleBps,m.demand,m.supply,m.referencePrice,m.minimumPrice,m.maximumPrice,
            m.price,m.sensitivity,m.accessibilityBps,m.disruptionBps}) h.u64(v);
        h.u64(m.factions.size());
        for (const auto& [f,e] : m.factions) { h.u64(f.value()); h.u64(e.supply); h.u64(e.shareBps); h.u64(e.sold); h.u64(e.incomeRemainder); h.u64(e.creditedIncome); }
    }
    h.u64(s.dealers.size());
    for (const auto& [id,d] : s.dealers) { h.u64(id.value()); h.u64(d.faction.value()); h.u64(d.territory.value()); h.u64(d.capacity); h.u64(d.enabled); }
}
}
