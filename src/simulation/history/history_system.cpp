#include "simulation/history/history_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/market/market_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include <algorithm>
#include <queue>

namespace faultline::simulation {
namespace {
bool informed(const GameState& s,FactionId f) {
    return std::find(s.history.informed.begin(),s.history.informed.end(),f)!=s.history.informed.end();
}
void record(GameState& s,HistoryEventKind kind,FactionId f,FactionId other={},NpcId npc={},TerritoryId t={},std::int64_t cost=0) {
    s.history.events.push_back({kind,s.history.months,f,other,npc,t,cost});
}
void checkEvidence(const GameState& s,const PrologueEvidence& p) {
    core::require(p.firstDeliveryCompleted && p.origin && p.rival && p.origin!=p.rival &&
        s.world.factions.exists(p.origin) && s.world.factions.exists(p.rival) &&
        s.world.npcs.exists(p.witness) && s.world.npcs.get(p.witness).faction==p.rival &&
        s.world.territories.exists(p.location) && p.incident<=DiscoveryIncident::InformationLeak &&
        p.deliveryTick<=p.discoveryTick && p.discoveryTick<=s.clock.tick(),"History requires FIRST DELIVERY then evidenced rival discovery");
}
std::map<TerritoryId,std::uint32_t> distances(const GameState& s,FactionId f) {
    std::map<TerritoryId,std::uint32_t> result;
    std::queue<TerritoryId> queue;
    const auto home=s.world.factions.get(f).homeTerritory; result[home]=0; queue.push(home);
    while (!queue.empty()) {
        const auto t=queue.front(); queue.pop();
        const auto found=s.factionSimulation.adjacency.find(t);
        if (found==s.factionSimulation.adjacency.end()) continue;
        for (auto next : found->second) if (!result.contains(next)) { result[next]=result[t]+1; queue.push(next); }
    }
    return result;
}
void reschedule(GameState& s) {
    s.factionSimulation.nextEvaluationTick=s.clock.tick()+s.factionSimulation.config.evaluationIntervalTicks;
    for (const auto& [id,n] : s.world.npcs.entries()) {
        if (n.lifecycle==world::NpcLifecycle::Alive)
            s.world.npcs.get(id).semantic.nextUpdateTick=s.clock.tick()+s.npcSimulation.config.updateIntervalTicks;
    }
    rebuildNpcSchedule(s);
}
void enterMarket(GameState& s,FactionId f,FactionId source) {
    s.history.informed.push_back(f);
    std::sort(s.history.informed.begin(),s.history.informed.end());
    for (auto& [id,d] : s.marketSimulation.dealers) { (void)id; if (d.faction==f) d.enabled=true; }
    record(s,HistoryEventKind::MarketEntry,f,source,{},s.world.factions.get(f).homeTerritory);
}
void expand(GameState& s,FactionId f) {
    auto& faction=s.world.factions.get(f);
    if (faction.reserveMembers<=8) return; // Keep a deployable reserve; dealers are real members.
    const auto distance=distances(s,f);
    TerritoryId best; std::int64_t bestScore=0,bestCost=0;
    for (const auto& [t,territory] : s.world.territories.entries()) {
        if (!distance.contains(t)) continue;
        const auto& market=s.marketSimulation.districts.at(territory.district);
        bool already=false;
        for (const auto& [id,d] : s.marketSimulation.dealers) { (void)id;
            if (d.faction==f && s.world.territories.get(d.territory).district==territory.district) already=true;
        }
        if (already || market.supply>=market.baselineDemand) continue;
        const auto cost=600+static_cast<std::int64_t>(distance.at(t))*100;
        if (faction.treasury<cost+5000) continue;
        const auto control=territoryControl(s,t);
        const auto security=control.controller==f ? 800 : control.controller ? -100 : 200;
        const auto score=static_cast<std::int64_t>(market.baselineDemand-market.supply)*10+security+
            faction.personality.marketFocus/10-static_cast<std::int64_t>(distance.at(t))*120;
        if (score>bestScore) { best=t; bestScore=score; bestCost=cost; }
    }
    if (!best) return;
    // A month of preparation has elapsed. Transfer one aggregate member into a
    // persistent dealer, never create extra manpower or a physical entity.
    const auto& home=s.world.npcs.get(faction.leader);
    world::NpcState n;
    n.faction=f; n.location={s.world.territories.get(best).district,best,{}};
    for (const auto& t : s.world.base->territories) if (t.id==best) {
        n.position=world::fromGlobal({static_cast<double>(t.boundary.x)+t.boundary.width/2.0,0,
            static_cast<double>(t.boundary.z)+t.boundary.depth/2.0},s.world.spatial.cellSize());
        break;
    }
    n.identity.name=faction.name+" associate "+std::to_string(s.world.npcs.nextId());
    n.identity.age=world::AgeBand::Adult;
    n.identity.appearanceSeed=core::deriveSeed(s.metadata.seed.value,"history.dealer.v1")^s.world.npcs.nextId();
    n.authority=world::AuthorityTier::Member; n.loyalty=faction.cohesion; n.traits=home.traits; n.capabilities=home.capabilities;
    n.roles.add(world::NpcRole::FactionMember); n.roles.add(world::NpcRole::Dealer);
    const auto id=s.world.npcs.create(n); --faction.reserveMembers; faction.treasury-=bestCost;
    initializeNpc(s,id);
    const auto demand=s.marketSimulation.districts.at(n.location.district).baselineDemand;
    registerMarketDealer(s,id,best,std::clamp(demand/2,400U,1200U));
    record(s,HistoryEventKind::DealerExpansion,f,{},id,best,bestCost);
    NpcEventBuffer events;
    RelationshipSystem::apply(s,{RelationshipEventKind::WorkedWith,id,faction.leader},events);
}
void settle(GameState& s,FactionId f) {
    auto& faction=s.world.factions.get(f);
    if (faction.reserveMembers<12 || faction.treasury<6000) return;
    // Secure an adjacent neutral site with actual members. Rival garrisons are
    // never recolored or transferred; this is not a simulated military victory.
    for (const auto& [t,territory] : s.world.territories.entries()) {
        if (territoryControl(s,t).state!=TerritoryControlState::Uncontrolled || territory.residentMembers) continue;
        bool adjacent=false;
        const auto it=s.factionSimulation.adjacency.find(t);
        if (it!=s.factionSimulation.adjacency.end())
            for (auto neighbor : it->second) if (controlsTerritory(s,f,neighbor)) adjacent=true;
        if (!adjacent) continue;
        auto& local=s.world.territories.get(t); local.residentMembers=6;
        auto& runtime=s.territorySimulation.territories.at(t); runtime.garrisonFaction=f;
        faction.reserveMembers-=6; faction.treasury-=1000;
        TerritoryEventBuffer events; reevaluateTerritoryControl(s,t,events);
        record(s,HistoryEventKind::TerritorialSettlement,f,{}, {},t,1000); break;
    }
}
void politics(GameState& s) {
    NpcEventBuffer events;
    const auto adjust=[](std::uint16_t& value,int delta) { value=static_cast<std::uint16_t>(std::clamp(static_cast<int>(value)+delta,0,10000)); };
    for (auto& r : s.world.factionRelationships) {
        if (!informed(s,r.from) || !informed(s,r.to)) continue;
        TerritoryId overlap;
        for (const auto& [id,m] : s.marketSimulation.districts) {
            if (m.factions.contains(r.from) && m.factions.at(r.from).supply &&
                m.factions.contains(r.to) && m.factions.at(r.to).supply) {
                for (const auto& [t,d] : s.world.territories.entries()) if (d.district==id) { overlap=t; break; }
                break;
            }
        }
        const bool border=(r.context & static_cast<std::uint8_t>(world::FactionRelationContext::Border))!=0;
        const bool dispute=overlap || border;
        if (dispute) {
            adjust(r.hostility,90); adjust(r.historicalGrievance,50); adjust(r.trust,-30);
        } else {
            // Noncompeting networks cooperate; trust affects the same runtime
            // diplomatic classification/attack utilities used after the skip.
            adjust(r.trust,80); adjust(r.sharedInterest,60); adjust(r.hostility,-40);
        }
        // Compress repeated pressure into annual causal records, not daily logs.
        if (s.history.months%12==0) {
            const auto from=s.world.factions.get(r.from).leader,to=s.world.factions.get(r.to).leader;
            RelationshipSystem::apply(s,{dispute ? RelationshipEventKind::Threatened : RelationshipEventKind::WorkedWith,from,to},events);
            record(s,dispute ? HistoryEventKind::MarketDispute : HistoryEventKind::Cooperation,r.from,r.to,from,
                overlap ? overlap : s.world.factions.get(r.from).homeTerritory);
        }
    }
}
}
PrologueEvidence historyPrologueCandidate(const GameState& s,FactionId origin) {
    core::require(s.world.factions.exists(origin),"Unknown starting faction");
    std::vector<FactionId> rivals;
    for (const auto& [id,f] : s.world.factions.entries()) { (void)f; if (id!=origin) rivals.push_back(id); }
    core::require(!rivals.empty(),"Prologue needs a rival");
    core::DeterministicRng rng{core::deriveSeed(core::deriveSeed(s.metadata.seed.value,"history.discovery.v1"),"origin",origin)};
    const auto rival=rivals.at(rng.uniform(static_cast<std::uint32_t>(rivals.size())));
    return {origin,rival,s.world.factions.get(rival).leader,s.world.factions.get(origin).homeTerritory,
        static_cast<DiscoveryIncident>(rng.uniform(6)),s.clock.tick(),s.clock.tick(),false};
}
void beginStrategicHistory(GameState& s,const PrologueEvidence& p) {
    validateState(s); checkEvidence(s,p);
    core::require(s.history.version==0 && s.world.base && s.world.factions.entries().size()==5 &&
        s.world.entities.entries().empty() && s.factionSimulation.operations.entries().empty(),"History requires an untransitioned headless prologue world");
    for (const auto& [id,n] : s.world.npcs.entries()) { (void)id;
        core::require(!n.semantic.travel && !n.semantic.operation && n.lifecycle==world::NpcLifecycle::Alive,"History cannot discard actor commitments");
    }
    for (const auto& [id,f] : s.world.factions.entries()) {
        core::require(f.lifecycle==world::FactionLifecycle::Active && f.treasury>=10000 &&
            f.reserveMembers>=12 && controlsTerritory(s,id,f.homeTerritory),"Invalid prehistory faction viability");
    }
    core::require(historyDays*gameDayTicks+s.factionSimulation.config.evaluationIntervalTicks+s.npcSimulation.config.updateIntervalTicks+
        s.marketSimulation.rules.intervalTicks<=core::SimulationClock::maxTick-s.clock.tick(),"History exceeds clock range");
    s.history.version=currentHistoryVersion; s.history.startTick=s.clock.tick(); s.history.prologue=p;
    record(s,HistoryEventKind::FirstDelivery,p.origin,{}, {},p.location);
    record(s,HistoryEventKind::RivalDiscovery,p.rival,p.origin,p.witness,p.location);
    for (auto& [id,d] : s.marketSimulation.dealers) { (void)id; d.enabled=false; }
    enterMarket(s,p.origin,{}); enterMarket(s,p.rival,p.origin);
    refreshMarkets(s);
}
void advanceStrategicHistory(GameState& s) {
    validateStrategicHistory(s);
    core::require(s.history.version==currentHistoryVersion && s.history.months<historyMonths,"No pending strategic history month");
    const auto month=s.history.months+1;
    // Integer partition of exactly four 365-day strategic years: 30/31 day steps.
    const auto end=s.history.startTick+static_cast<std::uint64_t>(month)*historyDays/historyMonths*gameDayTicks;
    const auto rules=s.marketSimulation.rules;
    s.marketSimulation.rules.intervalTicks=gameDayTicks;
    s.marketSimulation.nextUpdateTick=s.clock.tick()+gameDayTicks;
    while (s.clock.tick()<end) {
        s.clock=core::SimulationClock{s.marketSimulation.nextUpdateTick}; updateMarkets(s);
    }
    s.marketSimulation.rules=rules;
    s.marketSimulation.nextUpdateTick=s.clock.tick()+rules.intervalTicks;
    s.history.months=month;
    // Public distribution supplies an explicit information path, one new faction
    // per elapsed month. The seed selects witnesses, never arbitrary final shares.
    for (const auto& [id,f] : s.world.factions.entries()) if (!informed(s,id)) {
        FactionId source;
        for (const auto& [npc,d] : s.marketSimulation.dealers) { (void)npc;
            if (d.enabled && informed(s,d.faction)) { source=d.faction; break; }
        }
        core::require(static_cast<bool>(source),"No observable distribution for market discovery");
        (void)f; enterMarket(s,id,source); break;
    }
    // Rotate evaluation order by an isolated month seed, so the lowest ID does
    // not always take the first available district. No runtime RNG is consumed.
    std::vector<FactionId> order=s.history.informed;
    core::DeterministicRng rng{core::deriveSeed(s.metadata.seed.value,"history.month.v1") ^ month};
    const auto offset=rng.uniform(static_cast<std::uint32_t>(order.size()));
    std::rotate(order.begin(),order.begin()+static_cast<std::ptrdiff_t>(offset),order.end());
    for (auto id : order) { expand(s,id); settle(s,id); refreshMarkets(s); }
    // Paid dealer presence leaves bounded territorial influence. Costs grow with
    // network size, and no dealer output or member is counted twice.
    std::map<FactionId,std::int64_t> maintenance;
    for (const auto& [id,d] : s.marketSimulation.dealers) {
        const auto& n=s.world.npcs.get(id);
        if (!d.enabled || n.lifecycle!=world::NpcLifecycle::Alive) continue;
        auto& f=s.world.factions.get(d.faction); const auto cost=static_cast<std::int64_t>(d.capacity/20+20);
        if (f.treasury<cost) continue;
        f.treasury-=cost; maintenance[d.faction]+=cost;
        auto& influence=s.territorySimulation.territories.at(d.territory).entrenchment[d.faction];
        const auto limit=std::min(s.territorySimulation.rules.maximumPresence,100U+d.capacity/10);
        influence+=std::min(10U,limit>influence ? limit-influence : 0U);
    }
    for (const auto& [id,cost] : maintenance)
        record(s,HistoryEventKind::NetworkMaintenance,id,{}, {},s.world.factions.get(id).homeTerritory,cost);
    TerritoryEventBuffer territoryEvents;
    for (const auto& [t,r] : s.territorySimulation.territories) { (void)r; reevaluateTerritoryControl(s,t,territoryEvents); }
    refreshMarkets(s); politics(s); reschedule(s);
    validateTerritorySimulation(s); validateFactionSimulation(s); validateNpcSimulation(s);
    validateMarketSimulation(s); validateStrategicHistory(s);
    if (month==historyMonths) validatePostTimeskipWorld(s);
}
void validateStrategicHistory(const GameState& s) {
    const auto& h=s.history;
    if (!h.version) { core::require(h==StrategicHistory{},"Unversioned history payload"); return; }
    core::require(h.version==currentHistoryVersion && h.months<=historyMonths && h.events.size()<=2048 &&
        h.informed.size()>=2 && h.informed.size()<=5,"Invalid strategic history header");
    checkEvidence(s,h.prologue);
    const auto end=h.startTick+static_cast<std::uint64_t>(h.months)*historyDays/historyMonths*gameDayTicks;
    core::require(h.startTick<=core::SimulationClock::maxTick-historyDays*gameDayTicks &&
        h.prologue.discoveryTick<=h.startTick && (h.months==historyMonths ? s.clock.tick()>=end : s.clock.tick()==end),"Invalid history time");
    FactionId previous;
    for (auto f : h.informed) { core::require(f>previous && s.world.factions.exists(f),"Invalid informed faction"); previous=f; }
    core::require(informed(s,h.prologue.origin) && informed(s,h.prologue.rival) && h.events.size()>=4 &&
        h.events[0].kind==HistoryEventKind::FirstDelivery && h.events[1].kind==HistoryEventKind::RivalDiscovery,"Invalid history prologue order");
    std::uint32_t month=0;
    for (const auto& e : h.events) {
        core::require(e.kind<=HistoryEventKind::NetworkMaintenance && e.month>=month && e.month<=h.months &&
            s.world.factions.exists(e.faction) && (!e.other || s.world.factions.exists(e.other)) &&
            (!e.npc || s.world.npcs.exists(e.npc)) && s.world.territories.exists(e.territory) && e.cost>=0,"Invalid history event/reference");
        month=e.month;
    }
}
void validatePostTimeskipWorld(const GameState& s) {
    validateState(s);
    core::require(s.history.months==historyMonths && s.history.informed.size()==5,"Incomplete four-year transition");
    std::map<FactionId,std::uint64_t> supply;
    std::uint64_t total=0;
    for (const auto& [id,m] : s.marketSimulation.districts) { (void)id;
        core::require(m.demand>0 && m.supply>0 && m.price<m.maximumPrice,"Post-timeskip market is not city-wide/playable");
        for (const auto& [f,e] : m.factions) { supply[f]+=e.supply; total+=e.supply; }
    }
    bool tension=false;
    for (const auto& r : s.world.factionRelationships) if (r.historicalGrievance && r.hostility>=4500) tension=true;
    core::require(tension,"History has no meaningful unresolved tension");
    for (const auto& [id,f] : s.world.factions.entries()) {
        core::require(f.lifecycle==world::FactionLifecycle::Active && f.treasury>0 && f.reserveMembers>=4 &&
            f.leader && s.world.npcs.get(f.leader).lifecycle==world::NpcLifecycle::Alive &&
            controlledTerritoryCount(s,id)>0 && supply[id]>0 && supply[id]*100<total*80 &&
            controlledTerritoryCount(s,id)*100<s.world.territories.entries().size()*80,"Nonviable or already dominant post-timeskip faction");
    }
}
void appendStrategicHistoryHash(core::StateHash& h,const GameState& s) {
    const auto& x=s.history; h.u64(x.version); h.u64(x.months); h.u64(x.startTick);
    const auto& p=x.prologue;
    for (auto v : {p.origin.value(),p.rival.value(),p.witness.value(),p.location.value(),static_cast<std::uint64_t>(p.incident),
        p.deliveryTick,p.discoveryTick,static_cast<std::uint64_t>(p.firstDeliveryCompleted)}) h.u64(v);
    h.u64(x.informed.size()); for (auto f : x.informed) h.u64(f.value());
    h.u64(x.events.size()); for (const auto& e : x.events) {
        h.u64(static_cast<std::uint64_t>(e.kind)); h.u64(e.month); h.u64(e.faction.value()); h.u64(e.other.value());
        h.u64(e.npc.value()); h.u64(e.territory.value()); h.u64(static_cast<std::uint64_t>(e.cost));
    }
}
}
