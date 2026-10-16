#include "world/generation/actor_generation.hpp"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <set>

namespace faultline::world {
ValidationReport validateFactionViability(const GeneratedWorld& world,FactionId faction) {
    ValidationReport report;
    const auto fail=[&](const char* message){report.errors.push_back(std::string{message}+" [FactionId="+std::to_string(faction.value())+"]");};
    const auto f=std::find_if(world.factions.begin(),world.factions.end(),[&](const auto& x){return x.id==faction;});
    if (f==world.factions.end()) {fail("Missing faction"); return report;}
    if (f->state.lifecycle!=FactionLifecycle::Active || f->state.treasury<=0 || f->state.reserveMembers==0 || factionManpower(world,faction)==0)
        fail("Invalid initial strategic resources/lifecycle");
    const auto leader=std::find_if(world.majorNpcs.begin(),world.majorNpcs.end(),[&](const auto& n){return n.id==f->state.leader;});
    if (leader==world.majorNpcs.end() || leader->state.faction!=faction || !leader->state.roles.has(NpcRole::FactionLeader)) fail("Invalid faction leader");
    std::size_t members=0, seconds=0, dealers=0;
    for (const auto& n : world.majorNpcs) if (n.state.faction==faction && n.state.lifecycle==NpcLifecycle::Alive && n.state.major) {
        ++members; seconds+=n.state.authority==AuthorityTier::Command; dealers+=n.state.roles.has(NpcRole::Dealer);
    }
    if (members<3 || seconds==0 || dealers==0) fail("Incomplete important member structure");
    const auto home=std::find_if(world.territoryAssignments.begin(),world.territoryAssignments.end(),[&](const auto& a){return a.territory==f->state.homeTerritory;});
    if (home==world.territoryAssignments.end() || home->owner!=faction || home->residentMembers==0) fail("Invalid home presence");
    const auto homeGeometry=std::find_if(world.territories.begin(),world.territories.end(),[&](const auto& t){return t.id==f->state.homeTerritory;});
    std::map<RoadNodeId,RoadNode> nodes; for (const auto& n : world.roads.nodes) nodes.emplace(n.id,n);
    bool accessible=false;
    if (homeGeometry!=world.territories.end()) for (const auto& edge : world.roads.edges) if (nodes.contains(edge.a) && nodes.contains(edge.b)) {
        const auto a=nodes.at(edge.a), b=nodes.at(edge.b); const auto r=homeGeometry->boundary;
        accessible |= a.x==b.x ? a.x>=r.x && a.x<=right(r) && std::max(a.z,b.z)>r.z && std::min(a.z,b.z)<bottom(r)
            : a.z>=r.z && a.z<=bottom(r) && std::max(a.x,b.x)>r.x && std::min(a.x,b.x)<right(r);
    }
    if (!accessible) fail("Faction home has no road access");
    const auto graph=territoryAdjacency(world.territories);
    std::set<TerritoryId> owned,visited; std::deque<TerritoryId> pending;
    for (const auto& a : world.territoryAssignments) if (a.owner==faction) owned.insert(a.territory);
    if (graph.contains(f->state.homeTerritory) && owned.contains(f->state.homeTerritory)) {pending.push_back(f->state.homeTerritory); visited.insert(f->state.homeTerritory);}
    while (!pending.empty()) {
        const auto id=pending.front(); pending.pop_front();
        for (auto next : graph.at(id)) if (owned.contains(next) && visited.insert(next).second) pending.push_back(next);
    }
    if (owned.empty() || owned!=visited) fail("Disconnected initial control");
    std::uint64_t total=0,controlled=0;
    for (const auto& t : world.territories) if (valid(t.boundary)) {
        total+=static_cast<std::uint64_t>(area(t.boundary));
        if (owned.contains(t.id)) controlled+=static_cast<std::uint64_t>(area(t.boundary));
    }
    if (controlled==0 || controlled*2>total) fail("Absurd initial territorial share");
    return report;
}
ValidationReport validateActors(const GeneratedWorld& world) {
    ValidationReport report;
    const auto fail=[&](const char* message,std::uint64_t id=0){report.errors.push_back(std::string{message}+" [id="+std::to_string(id)+"]");};
    if (world.factions.size()!=mainFactionCount) fail("Standard M3 world must contain exactly five main factions");
    if (world.majorNpcs.size()<15 || world.majorNpcs.size()>25) fail("Invalid major NPC count");
    std::map<FactionId,const FactionState*> factions; std::set<std::string> factionNames,npcNames;
    std::set<TerritoryId> homes; std::array<std::uint32_t,4> families{};
    std::map<TerritoryId,const GeneratedTerritory*> territories;
    std::map<DistrictId,const GeneratedDistrict*> districts;
    std::map<BuildingId,const BuildingState*> buildings;
    for (const auto& t : world.territories) territories.emplace(t.id,&t);
    for (const auto& d : world.districts) districts.emplace(d.id,&d);
    for (const auto& b : world.buildings) buildings.emplace(b.id,&b);
    for (const auto& f : world.factions) {
        if (!f.id || !factions.emplace(f.id,&f.state).second) fail("Duplicate/null faction ID",f.id.value());
        if (f.state.name.empty() || !factionNames.insert(f.state.name).second) fail("Duplicate/empty faction name",f.id.value());
        if (f.state.family>FactionFamily::Cartel) fail("Invalid canonical family",f.id.value());
        else ++families[static_cast<std::size_t>(f.state.family)];
        if (!territories.contains(f.state.homeTerritory) || !homes.insert(f.state.homeTerritory).second) fail("Invalid/duplicate faction home",f.id.value());
        for (auto value : {f.state.cohesion,f.state.personality.aggression,f.state.personality.riskTolerance,f.state.personality.marketFocus,f.state.personality.territorialFocus})
            if (value>10000) fail("Faction normalized value outside bounds",f.id.value());
    }
    auto sortedFamilies=families; std::sort(sortedFamilies.begin(),sortedFamilies.end());
    if (sortedFamilies!=std::array<std::uint32_t,4>{1,1,1,2}) fail("Four families must have exactly one duplicate");
    std::map<TerritoryId,TerritoryAssignment> assignments;
    for (const auto& a : world.territoryAssignments) {
        if (!territories.contains(a.territory) || !assignments.emplace(a.territory,a).second) fail("Invalid/duplicate territory assignment",a.territory.value());
        if (a.owner && !factions.contains(a.owner)) fail("Territory owner does not exist",a.territory.value());
        if (static_cast<bool>(a.owner)!=(a.residentMembers>0)) fail("Territory presence/owner mismatch",a.territory.value());
    }
    if (assignments.size()!=territories.size()) fail("Incomplete territory assignment table");
    if (!report.passed()) return report;
    std::map<NpcId,const NpcState*> npcs;
    for (const auto& n : world.majorNpcs) {
        const auto& s=n.state;
        if (!n.id || !npcs.emplace(n.id,&s).second) fail("Duplicate/null NPC ID",n.id.value());
        if (s.identity.name.empty() || !npcNames.insert(s.identity.name).second) fail("Duplicate/empty major NPC name",n.id.value());
        if (!factions.contains(s.faction) || !territories.contains(s.location.territory) || !districts.contains(s.location.district)) {
            fail("Invalid major NPC faction/location",n.id.value()); continue;
        }
        const auto& f=*factions.at(s.faction); const auto& t=*territories.at(s.location.territory);
        if (s.lifecycle!=NpcLifecycle::Alive || !s.major || s.activeEntity || s.health.current<=0 || s.health.current>s.health.maximum ||
            !std::isfinite(s.health.current) || !std::isfinite(s.health.maximum)) fail("Major NPC must start alive and semantic",n.id.value());
        if (s.identity.age>AgeBand::OlderAdult || s.authority>AuthorityTier::Leader || s.respect>100 || s.loyalty>10000 || (s.roles.bits & ~15U)!=0 ||
            !s.roles.has(NpcRole::FactionMember) || s.authority<AuthorityTier::Member ||
            s.roles.has(NpcRole::FactionLeader)!=(f.leader==n.id) || s.roles.has(NpcRole::FactionLeader)!=(s.authority==AuthorityTier::Leader) ||
            (s.roles.has(NpcRole::FactionLeader) && s.roles.has(NpcRole::Dealer))) fail("Invalid major NPC roles/identity",n.id.value());
        for (auto value : {s.traits.aggression,s.traits.ambition,s.traits.riskTolerance,s.traits.loyaltyBias,s.traits.greed,s.traits.courage,
            s.capabilities.combat,s.capabilities.driving,s.capabilities.leadership,s.capabilities.streetKnowledge,s.capabilities.business})
            if (value>10000) fail("NPC normalized value outside bounds",n.id.value());
        if (t.district!=s.location.district || f.homeTerritory!=t.id || assignments.at(t.id).owner!=s.faction) fail("Incoherent faction home location",n.id.value());
        try {
            if (normalize(s.position)!=s.position) fail("Unnormalized NPC position",n.id.value());
            const auto p=toGlobal(s.position);
            if (p.x<t.boundary.x || p.x>static_cast<double>(right(t.boundary)) || p.z<t.boundary.z || p.z>static_cast<double>(bottom(t.boundary))) fail("NPC position outside location",n.id.value());
            if (s.location.building) {
                const auto b=buildings.find(s.location.building);
                if (b==buildings.end() || b->second->district!=s.location.district || !inside(b->second->footprint,t.boundary)) fail("Invalid NPC building reference",n.id.value());
                else if (p.x<b->second->footprint.x || p.x>static_cast<double>(right(b->second->footprint)) || p.z<b->second->footprint.z || p.z>static_cast<double>(bottom(b->second->footprint)))
                    fail("NPC position outside building",n.id.value());
            }
        } catch (const std::exception&) {fail("Non-finite/invalid NPC position",n.id.value());}
    }
    for (std::size_t i=0; i<world.factions.size(); ++i) for (std::size_t j=i+1; j<world.factions.size(); ++j) {
        const auto& a=world.factions[i].state; const auto& b=world.factions[j].state;
        if (a.family==b.family && (a.personality==b.personality || a.treasury==b.treasury)) fail("Duplicated family factions are clones");
    }
    if (!report.passed()) return report;
    for (const auto& f : world.factions) {
        const auto viability=validateFactionViability(world,f.id); report.errors.insert(report.errors.end(),viability.errors.begin(),viability.errors.end());
    }
    const auto graph=territoryAdjacency(world.territories);
    std::set<std::pair<FactionId,FactionId>> borders;
    for (const auto& [id,neighbors] : graph) for (auto next : neighbors)
        if (assignments.at(id).owner && assignments.at(next).owner && assignments.at(id).owner!=assignments.at(next).owner)
            borders.emplace(assignments.at(id).owner,assignments.at(next).owner);
    std::map<std::pair<FactionId,FactionId>,const FactionRelationship*> relations;
    for (const auto& r : world.factionRelationships) {
        if (!factions.contains(r.from) || !factions.contains(r.to) || r.from==r.to || !relations.emplace(std::pair{r.from,r.to},&r).second) {
            fail("Invalid/duplicate faction relationship"); continue;
        }
        for (auto v : {r.trust,r.hostility,r.fear,r.sharedInterest,r.historicalGrievance}) if (v>10000) fail("Faction relationship outside bounds");
        const auto a=factions.at(r.from), b=factions.at(r.to);
        const auto interest=10000-std::abs(static_cast<int>(a->personality.marketFocus)-b->personality.marketFocus);
        std::uint8_t expected=static_cast<std::uint8_t>(borders.contains({r.from,r.to}) ? FactionRelationContext::Border : FactionRelationContext::Distant);
        if (a->family==b->family) expected|=static_cast<std::uint8_t>(FactionRelationContext::SharedFamily);
        if (interest>=8500) expected|=static_cast<std::uint8_t>(FactionRelationContext::SimilarPriorities);
        if (r.context!=expected || r.sharedInterest!=interest || r.historicalGrievance!=0) fail("Faction relationship has unsupported causal context/history");
    }
    if (relations.size()!=mainFactionCount*(mainFactionCount-1)) fail("Incomplete initial faction relationships");
    std::set<std::pair<NpcId,NpcId>> personal;
    for (const auto& r : world.npcRelationships) {
        if (!npcs.contains(r.from) || !npcs.contains(r.to) || r.from==r.to || !personal.emplace(r.from,r.to).second) {
            fail("Invalid/duplicate NPC relationship"); continue;
        }
        for (auto v : {r.trust,r.fear,r.hostility,r.personalLoyalty,r.debt}) if (v>10000) fail("NPC relationship outside bounds");
        const auto a=npcs.at(r.from), b=npcs.at(r.to);
        if (r.debt!=0) fail("M3 cannot invent historical personal debts");
        if (r.context==NpcRelationContext::LeadershipTie) {
            if (a->faction!=b->faction || !(a->roles.has(NpcRole::FactionLeader) || b->roles.has(NpcRole::FactionLeader))) fail("Unsupported leadership relationship");
        } else if (r.context==NpcRelationContext::RivalLeadership) {
            if (a->faction==b->faction || !a->roles.has(NpcRole::FactionLeader) || !b->roles.has(NpcRole::FactionLeader) || !borders.contains({a->faction,b->faction}))
                fail("Unsupported rival leadership relationship");
        } else fail("Unknown NPC relationship context");
    }
    for (const auto& n : world.majorNpcs) if (n.id!=factions.at(n.state.faction)->leader) {
        const auto leader=factions.at(n.state.faction)->leader;
        if (!personal.contains({leader,n.id}) || !personal.contains({n.id,leader})) fail("Missing important leadership tie",n.id.value());
    }
    if (personal.size()>world.majorNpcs.size()*2+2) fail("NPC relationships must remain sparse");
    return report;
}
}
