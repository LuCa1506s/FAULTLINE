#include "world/generation/actor_generation.hpp"
#include "world/generation/actor_names.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>

namespace faultline::world {
RelationshipGenerationResult generateInitialRelationships(const GenerationContext& context,const GeneratedWorld& world,
    std::span<const GeneratedFaction> factions,std::span<const TerritoryAssignment> assignments,std::span<const GeneratedNpc> npcs) {
    const auto graph=territoryAdjacency(world.territories);
    std::map<TerritoryId,FactionId> owners;
    for (const auto& a : assignments) owners.emplace(a.territory,a.owner);
    std::set<std::pair<FactionId,FactionId>> borders;
    for (const auto& [id,neighbors] : graph) for (auto next : neighbors)
        if (owners.at(id) && owners.at(next) && owners.at(id)!=owners.at(next)) borders.emplace(owners.at(id),owners.at(next));
    const auto manpower=[&](FactionId id) {
        std::int64_t total=0;
        for (const auto& f : factions) if (f.id==id) total+=f.state.reserveMembers;
        for (const auto& a : assignments) if (a.owner==id) total+=a.residentMembers;
        for (const auto& n : npcs) if (n.state.faction==id) ++total;
        return total;
    };
    RelationshipGenerationResult result;
    for (const auto& from : factions) for (const auto& to : factions) if (from.id!=to.id) {
        core::DeterministicRng rng{actor_detail::pairSeed(context.stageSeed("relationships.factions"),from.id.value(),to.id.value())};
        const bool shared=from.state.family==to.state.family, border=borders.contains({from.id,to.id});
        const auto interests=10000-std::abs(static_cast<int>(from.state.personality.marketFocus)-to.state.personality.marketFocus);
        std::uint8_t flags=static_cast<std::uint8_t>(border ? FactionRelationContext::Border : FactionRelationContext::Distant);
        if (shared) flags |= static_cast<std::uint8_t>(FactionRelationContext::SharedFamily);
        if (interests>=8500) flags |= static_cast<std::uint8_t>(FactionRelationContext::SimilarPriorities);
        const auto trust=actor_detail::bps((shared ? 5100 : 2800)+interests/5+(border ? -1100 : 300)+static_cast<std::int64_t>(rng.uniform(800)));
        const auto hostility=actor_detail::bps((border ? 4000 : 800)+from.state.personality.aggression/(border ? 3 : 8)-(shared ? 500 : 0));
        const auto fear=actor_detail::bps(2000+(manpower(to.id)-manpower(from.id))*40+(to.state.treasury-from.state.treasury)/100+
            to.state.personality.aggression/5);
        result.factions.push_back({from.id,to.id,trust,hostility,fear,static_cast<std::uint16_t>(interests),0,flags});
    }
    std::map<NpcId,const GeneratedNpc*> byId;
    for (const auto& n : npcs) byId.emplace(n.id,&n);
    const auto addNpc=[&](NpcId from,NpcId to,NpcRelationContext why,const FactionRelationship* political) {
        const auto& a=byId.at(from)->state; const auto& b=byId.at(to)->state;
        core::DeterministicRng rng{actor_detail::pairSeed(context.stageSeed("relationships.npc"),from.value(),to.value())};
        const auto trust=political ? political->trust/2 : (a.loyalty+b.loyalty)/2-static_cast<int>(a.traits.ambition)/10;
        const auto fear=actor_detail::bps(1000+(static_cast<int>(b.respect)-a.respect)*50+b.traits.aggression/5);
        const auto hostility=political ? political->hostility : a.traits.ambition/8;
        result.npcs.push_back({from,to,actor_detail::bps(trust+static_cast<std::int64_t>(rng.uniform(500))),fear,
            static_cast<std::uint16_t>(hostility),political ? std::uint16_t{0} : a.loyalty,0,why});
    };
    for (const auto& f : factions) for (const auto& n : npcs) if (n.state.faction==f.id && n.id!=f.state.leader) {
        addNpc(f.state.leader,n.id,NpcRelationContext::LeadershipTie,nullptr);
        addNpc(n.id,f.state.leader,NpcRelationContext::LeadershipTie,nullptr);
    }
    const FactionRelationship* rival=nullptr;
    for (const auto& relation : result.factions) if ((relation.context & static_cast<std::uint8_t>(FactionRelationContext::Border))!=0 &&
        (!rival || relation.hostility>rival->hostility)) rival=&relation;
    if (rival) {
        NpcId a,b;
        for (const auto& f : factions) { if (f.id==rival->from) a=f.state.leader; if (f.id==rival->to) b=f.state.leader; }
        addNpc(a,b,NpcRelationContext::RivalLeadership,rival);
        const auto opposite=std::find_if(result.factions.begin(),result.factions.end(),[&](const auto& r){return r.from==rival->to && r.to==rival->from;});
        addNpc(b,a,NpcRelationContext::RivalLeadership,&*opposite);
    }
    std::sort(result.factions.begin(),result.factions.end(),[](const auto& a,const auto& b){return std::pair{a.from,a.to}<std::pair{b.from,b.to};});
    std::sort(result.npcs.begin(),result.npcs.end(),[](const auto& a,const auto& b){return std::pair{a.from,a.to}<std::pair{b.from,b.to};});
    return result;
}
}
