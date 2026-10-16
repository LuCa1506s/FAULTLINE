#include "world/generation/actor_generation.hpp"
#include "world/generation/actor_names.hpp"
#include <algorithm>
#include <deque>
#include <limits>
#include <set>

namespace faultline::world {
TerritoryAdjacency territoryAdjacency(std::span<const GeneratedTerritory> territories) {
    TerritoryAdjacency graph;
    for (const auto& t : territories) graph.emplace(t.id,std::vector<TerritoryId>{});
    for (std::size_t i=0; i<territories.size(); ++i) for (std::size_t j=i+1; j<territories.size(); ++j)
        if (adjacent(territories[i].boundary,territories[j].boundary)) {
            graph.at(territories[i].id).push_back(territories[j].id); graph.at(territories[j].id).push_back(territories[i].id);
        }
    for (auto& [id,neighbors] : graph) { (void)id; std::sort(neighbors.begin(),neighbors.end()); }
    return graph;
}
namespace {
int affinity(FactionFamily family, ZoneArchetype zone) {
    switch (family) {
    case FactionFamily::AfricanAmericanStreetGang:
        return zone==ZoneArchetype::Residential || zone==ZoneArchetype::Suburbs || zone==ZoneArchetype::Entertainment ? 1000 : 100;
    case FactionFamily::ItalianMafia:
        return zone==ZoneArchetype::Downtown || zone==ZoneArchetype::Financial || zone==ZoneArchetype::Residential ? 1000 : 100;
    case FactionFamily::AsianCriminalOrganization:
        return zone==ZoneArchetype::Downtown || zone==ZoneArchetype::Entertainment || zone==ZoneArchetype::University ? 1000 : 100;
    case FactionFamily::Cartel:
        return zone==ZoneArchetype::Industrial || zone==ZoneArchetype::Harbor || zone==ZoneArchetype::Waterfront ? 1000 : 100;
    }
    return 0;
}
std::map<TerritoryId,std::uint32_t> distanceFromHomes(const TerritoryAdjacency& graph, const std::set<TerritoryId>& homes) {
    std::map<TerritoryId,std::uint32_t> distances; std::deque<TerritoryId> pending;
    for (auto home : homes) { distances[home]=0; pending.push_back(home); }
    while (!pending.empty()) {
        const auto id=pending.front(); pending.pop_front();
        for (auto next : graph.at(id)) if (!distances.contains(next)) { distances[next]=distances.at(id)+1; pending.push_back(next); }
    }
    return distances;
}
std::uint32_t garrison(const GeneratedTerritory& t, std::uint64_t totalArea) {
    return static_cast<std::uint32_t>(3 + static_cast<std::uint64_t>(area(t.boundary))*20/totalArea);
}
}
FactionGenerationResult generateFactions(const GenerationContext& context, const GeneratedWorld& world) {
    core::require(world.territories.size() >= mainFactionCount,"M3 requires at least five territories");
    const auto graph=territoryAdjacency(world.territories);
    std::map<TerritoryId,const GeneratedTerritory*> territories;
    std::map<DistrictId,ZoneArchetype> zones;
    for (const auto& d : world.districts) zones.emplace(d.id,d.archetype);
    std::uint64_t totalArea=0;
    for (const auto& t : world.territories) { territories.emplace(t.id,&t); totalArea+=static_cast<std::uint64_t>(area(t.boundary)); }
    core::require(totalArea>0,"M3 requires positive territory area");
    std::array<FactionFamily,5> families{FactionFamily::AfricanAmericanStreetGang,FactionFamily::ItalianMafia,
        FactionFamily::AsianCriminalOrganization,FactionFamily::Cartel,FactionFamily::Cartel};
    core::DeterministicRng familyRng{core::deriveSeed(context.stageSeed("factions"),"families")};
    families.back()=static_cast<FactionFamily>(familyRng.uniform(4));
    for (std::size_t i=families.size()-1; i>0; --i) std::swap(families[i],families[static_cast<std::size_t>(familyRng.uniform(i+1))]);
    FactionGenerationResult result; std::set<std::string> names; std::set<TerritoryId> homes;
    std::map<TerritoryId,TerritoryAssignment> assignments;
    for (const auto& [id,t] : territories) { (void)t; assignments.emplace(id,TerritoryAssignment{id,{},{}}); }
    for (std::size_t i=0; i<mainFactionCount; ++i) {
        const FactionId id{i+1}; const auto family=families[i];
        core::DeterministicRng rng{core::deriveSeed(context.stageSeed("factions"),"faction",id)};
        core::DeterministicRng nameRng{core::deriveSeed(context.stageSeed("faction.names"),"faction",id)};
        FactionState state; state.family=family;
        state.name=actor_detail::uniqueName(family,false,nameRng,id.value(),names);
        state.treasury=static_cast<std::int64_t>(rng.uniform(1000,2501)+static_cast<std::uint64_t>(family)*150)*100+static_cast<std::int64_t>(id.value());
        state.reserveMembers=static_cast<std::uint32_t>(rng.uniform(70,111));
        state.cohesion=static_cast<std::uint16_t>(rng.uniform(5500,9001));
        state.personality={static_cast<std::uint16_t>(rng.uniform(20,81)*100+id.value()),
            static_cast<std::uint16_t>(rng.uniform(2000,8501)),static_cast<std::uint16_t>(rng.uniform(1500,9001)),
            static_cast<std::uint16_t>(rng.uniform(1500,9001))};
        const auto distances=distanceFromHomes(graph,homes);
        TerritoryId home; std::int64_t best=std::numeric_limits<std::int64_t>::min();
        for (const auto& [candidate,t] : territories) {
            // Broad initialization safety cap, not a runtime rubber-banding rule.
            if (homes.contains(candidate) || static_cast<std::uint64_t>(area(t->boundary))*100 > totalArea*40 || graph.at(candidate).empty()) continue;
            const auto hops=homes.empty() ? 0U : distances.at(candidate);
            const auto quality=static_cast<std::int64_t>(std::min<std::uint64_t>(1000,static_cast<std::uint64_t>(area(t->boundary))*10000/totalArea));
            core::DeterministicRng tie{actor_detail::pairSeed(context.stageSeed("faction.homes"),id.value(),candidate.value())};
            const auto score=static_cast<std::int64_t>(hops)*2000+affinity(family,zones.at(t->district))+quality+
                static_cast<std::int64_t>(std::min<std::uint32_t>(t->buildingCount,100))+static_cast<std::int64_t>(tie.uniform(400));
            if (score>best) { best=score; home=candidate; }
        }
        core::require(static_cast<bool>(home),"No viable distinct faction home");
        state.homeTerritory=home; homes.insert(home);
        const auto members=garrison(*territories.at(home),totalArea);
        state.reserveMembers-=members; assignments.at(home)={home,id,members};
        result.factions.push_back({id,std::move(state)});
    }
    std::array<std::uint64_t,5> ownedArea{};
    for (std::size_t i=0; i<mainFactionCount; ++i) ownedArea[i]=static_cast<std::uint64_t>(area(territories.at(result.factions[i].state.homeTerritory)->boundary));
    // Every expansion is adjacent to existing presence and consumes available manpower.
    for (std::size_t step=0; step<territories.size(); ++step) {
        std::array<std::size_t,5> order{0,1,2,3,4};
        std::sort(order.begin(),order.end(),[&](auto a,auto b) { return ownedArea[a]==ownedArea[b] ? a<b : ownedArea[a]<ownedArea[b]; });
        bool grew=false;
        for (auto i : order) {
            auto& faction=result.factions[i]; const auto home=territories.at(faction.state.homeTerritory);
            const auto budget=totalArea*(1600+faction.state.personality.territorialFocus/20)/10000;
            TerritoryId chosen; std::int64_t best=std::numeric_limits<std::int64_t>::min();
            for (const auto& [candidate,t] : territories) {
                if (assignments.at(candidate).owner) continue;
                const auto members=garrison(*t,totalArea);
                const auto size=static_cast<std::uint64_t>(area(t->boundary));
                if (ownedArea[i]+size>budget || faction.state.reserveMembers<members+10) continue;
                bool borders=false;
                for (auto neighbor : graph.at(candidate)) borders |= assignments.at(neighbor).owner==faction.id;
                if (!borders) continue;
                const auto score=affinity(faction.state.family,zones.at(t->district))+(home->district==t->district ? 800 : 0)+
                    static_cast<std::int64_t>(size*1000/totalArea);
                if (score>best) { best=score; chosen=candidate; }
            }
            if (!chosen) continue;
            const auto members=garrison(*territories.at(chosen),totalArea);
            faction.state.reserveMembers-=members; assignments.at(chosen)={chosen,faction.id,members};
            ownedArea[i]+=static_cast<std::uint64_t>(area(territories.at(chosen)->boundary)); grew=true;
            break;
        }
        if (!grew) break;
    }
    for (const auto& [id,a] : assignments) { (void)id; result.assignments.push_back(a); }
    return result;
}
std::uint64_t factionManpower(const GeneratedWorld& world,FactionId faction) {
    std::uint64_t total=0;
    for (const auto& f : world.factions) if (f.id==faction) total+=f.state.reserveMembers;
    for (const auto& a : world.territoryAssignments) if (a.owner==faction) total+=a.residentMembers;
    for (const auto& n : world.majorNpcs) if (n.state.faction==faction && n.state.lifecycle==NpcLifecycle::Alive) ++total;
    return total;
}
}
