#include "simulation/story/story_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include <algorithm>
#include <tuple>
#include <limits>
#include <set>
#include "simulation/history/history_system.hpp"

namespace faultline::simulation {
namespace {
constexpr std::size_t maximumStoryNodes=4096;
bool active(const GameState& s,FactionId id) {
    if (!s.world.factions.exists(id)) return false;
    const auto v=s.world.factions.get(id).lifecycle;
    const auto& f=s.world.factions.get(id);
    return (v==world::FactionLifecycle::Active || v==world::FactionLifecycle::Reformed) &&
        f.treasury>0 && f.leader && s.world.npcs.exists(f.leader) &&
        s.world.npcs.get(f.leader).lifecycle==world::NpcLifecycle::Alive;
}
bool usable(const GameState& s,NpcId id,FactionId faction,bool dealer=false) {
    if (!s.world.npcs.exists(id)) return false;
    const auto& n=s.world.npcs.get(id);
    const auto health=n.activeEntity ? s.world.health.get(n.activeEntity).current : n.health.current;
    if (n.faction!=faction || n.lifecycle!=world::NpcLifecycle::Alive || health<=0) return false;
    if (!dealer) return n.major || n.roles.has(world::NpcRole::MissionIssuer);
    const auto found=s.marketSimulation.dealers.find(id);
    return n.roles.has(world::NpcRole::Dealer) && found!=s.marketSimulation.dealers.end() &&
        found->second.enabled && found->second.faction==faction && found->second.capacity>0;
}
NpcId replacement(const GameState& s,FactionId faction,bool dealer) {
    NpcId best;
    std::tuple<bool,unsigned,unsigned,unsigned,unsigned> score{};
    for (const auto& [id,n] : s.world.npcs.entries()) if (usable(s,id,faction,dealer)) {
        // Semantic candidate only; FactionSystem retains organizational succession.
        const auto candidate=std::tuple{n.major,static_cast<unsigned>(n.authority),static_cast<unsigned>(n.loyalty),
            static_cast<unsigned>(n.capabilities.leadership),static_cast<unsigned>(n.respect)};
        if (!best || candidate>score) { best=id; score=candidate; }
    }
    return best; // ascending stable ID breaks exact ties
}
void bind(const GameState& s,StoryNode& n) {
    const bool dealer=n.kind==StoryKind::DealerConflict;
    if (n.kind==StoryKind::Betrayal && (!n.contact || n.contact==s.world.factions.get(n.faction).leader ||
        !usable(s,n.contact,n.faction))) {
        n.contact={}; int best=std::numeric_limits<int>::min();
        for (const auto& [id,candidate] : s.world.npcs.entries())
            if (id!=s.world.factions.get(n.faction).leader && usable(s,id,n.faction)) {
                const auto score=static_cast<int>(candidate.traits.ambition)-candidate.loyalty;
                if (!n.contact || score>best) { n.contact=id;best=score; }
            }
        return;
    }
    if (!usable(s,n.contact,n.faction,dealer)) n.contact=replacement(s,n.faction,dealer);
    if (dealer && n.contact) {
        n.territory=s.marketSimulation.dealers.at(n.contact).territory;
        n.district=s.world.territories.get(n.territory).district;
    }
}
bool worldEligible(const GameState& s,const StoryNode& n) {
    if (!active(s,n.faction) || (n.other && !active(s,n.other)) ||
        !usable(s,n.contact,n.faction,n.kind==StoryKind::DealerConflict)) return false;
    const auto& f=s.world.factions.get(n.faction);
    switch (n.kind) {
    case StoryKind::Contact: return true;
    case StoryKind::BossOperation: return f.leader && usable(s,f.leader,n.faction);
    case StoryKind::Succession:
        // Observe an actual organizational successor, never appoint one here.
        return n.originalContact && s.world.npcs.get(n.originalContact).lifecycle==world::NpcLifecycle::Dead &&
            f.leader!=n.originalContact;
    case StoryKind::Betrayal: {
        const auto& c=s.world.npcs.get(n.contact);
        const auto relation=std::find_if(s.world.npcRelationships.begin(),s.world.npcRelationships.end(),[&](const auto& r){
            return r.from==n.contact && r.to==f.leader;
        });
        const bool personalConflict=relation!=s.world.npcRelationships.end() &&
            (relation->hostility>=6000 || relation->personalLoyalty<3000);
        return n.contact!=f.leader && c.traits.ambition>c.loyalty && (f.cohesion<5000 || personalConflict);
    }
    case StoryKind::DealerConflict:
        return !controlsTerritory(s,n.faction,n.territory);
    case StoryKind::MarketChange: {
        const auto it=s.marketSimulation.districts.find(n.district);
        return it!=s.marketSimulation.districts.end() && it->second.demand>it->second.supply;
    }
    case StoryKind::TerritorialConflict:
    case StoryKind::Alliance: {
        const auto it=std::find_if(s.world.factionRelationships.begin(),s.world.factionRelationships.end(),[&](const auto& r){
            return r.from==n.faction && r.to==n.other;
        });
        if (it==s.world.factionRelationships.end()) return false;
        if (n.kind==StoryKind::Alliance) return it->trust>=6000 && it->hostility<4500;
        return it->hostility>=4500 && n.territory && controlsTerritory(s,n.other,n.territory);
    }
    }
    return false;
}
}
void initializeStoryGraph(GameState& s) {
    validateState(s);
    core::require(!s.story.version,"Story graph already initialized");
    core::require(s.history.version==currentHistoryVersion && s.history.months==historyMonths,
        "Story initialization requires completed strategic history");
    s.story.version=currentStoryGraphVersion; repairStoryGraph(s);
    validateStoryGraph(s);
}
void repairStoryGraph(GameState& s) {
    core::require(s.story.version==currentStoryGraphVersion,"Unsupported story graph version");
    // Append only missing semantic possibilities; never regenerate progressed nodes.
    const auto find=[&](StoryKind kind,FactionId f,FactionId other={}) {
        for (const auto& [id,n] : s.story.nodes.entries())
            if (n.kind==kind && n.faction==f && n.other==other) return id;
        return StoryNodeId{};
    };
    const auto add=[&](StoryKind kind,FactionId f,FactionId other,std::vector<StoryPrerequisite> prerequisites) {
        if (find(kind,f,other)) return;
        core::require(s.story.nodes.entries().size()<maximumStoryNodes,"Story graph resource limit");
        const auto& faction=s.world.factions.get(f);
        auto territory=other ? s.world.factions.get(other).homeTerritory : faction.homeTerritory;
        // Pick a real current opposing region, rather than assuming its old home is owned.
        if (other) for (const auto& [t,value] : s.world.territories.entries()) {
            (void)value; if (controlsTerritory(s,other,t)) { territory=t; break; }
        }
        const auto district=territory ? s.world.territories.get(territory).district : DistrictId{};
        StoryNode n{kind,f,other,faction.leader,faction.leader,territory,district,{},std::move(prerequisites)};
        for (std::size_t i=s.history.events.size();i>0;--i) {
            const auto& event=s.history.events[i-1];
            if (event.faction==f && (!other || event.other==other)) {
                n.historyRecord=static_cast<std::uint32_t>(i); break;
            }
        }
        bind(s,n);
        if (kind==StoryKind::DealerConflict) n.originalContact=n.contact;
        (void)s.story.nodes.create(std::move(n));
    };
    for (const auto& [f,value] : s.world.factions.entries()) {
        (void)value; add(StoryKind::Contact,f,{},{});
        const auto root=find(StoryKind::Contact,f);
        for (auto kind : {StoryKind::BossOperation,StoryKind::Betrayal,StoryKind::DealerConflict})
            add(kind,f,{},{{root,StoryOutcome::Succeeded}});
        add(StoryKind::MarketChange,f,{},{{root,StoryOutcome::Failed}});
        add(StoryKind::Succession,f,{},{});
        for (const auto& [other,v] : s.world.factions.entries()) if (other!=f) {
            (void)v;
            add(StoryKind::TerritorialConflict,f,other,{{root,StoryOutcome::Succeeded}});
            add(StoryKind::Alliance,f,other,{{root,StoryOutcome::Succeeded}});
        }
    }
    for (const auto& [id,value] : s.story.nodes.entries()) if (value.outcome==StoryOutcome::Unresolved) bind(s,s.story.nodes.get(id));
}
StoryAvailability storyAvailability(const GameState& s,StoryNodeId id) {
    const auto& n=s.story.nodes.get(id);
    if (n.outcome!=StoryOutcome::Unresolved) return StoryAvailability::Closed;
    // Check world first, even if an upstream prerequisite has not resolved yet.
    if (!worldEligible(s,n)) return StoryAvailability::Invalidated;
    bool locked=false;
    for (const auto& p : n.prerequisites) {
        const auto outcome=s.story.nodes.get(p.node).outcome;
        if (outcome!=StoryOutcome::Unresolved && outcome!=p.outcome) return StoryAvailability::Invalidated;
        if (outcome==StoryOutcome::Unresolved) locked=true;
    }
    return locked ? StoryAvailability::Locked : StoryAvailability::Eligible;
}
void resolveStoryNode(GameState& s,StoryNodeId id,StoryOutcome outcome) {
    core::require(outcome==StoryOutcome::Succeeded || outcome==StoryOutcome::Failed,"Invalid story resolution");
    core::require(storyAvailability(s,id)==StoryAvailability::Eligible,"Story possibility is not eligible");
    s.story.nodes.get(id).outcome=outcome;
}
void validateStoryGraph(const GameState& s) {
    if (!s.story.version) {
        core::require(s.story.nodes.entries().empty() && s.story.nodes.nextId()==1,"Unversioned story payload"); return;
    }
    core::require(s.story.version==currentStoryGraphVersion && s.story.nodes.entries().size()<=maximumStoryNodes &&
        !s.story.nodes.entries().empty() && s.history.version==currentHistoryVersion && s.history.months==historyMonths,
        "Invalid story graph header/context");
    std::set<std::tuple<StoryKind,FactionId,FactionId>> keys;
    for (const auto& [id,n] : s.story.nodes.entries()) {
        core::require(id && n.kind<=StoryKind::Alliance && n.outcome<=StoryOutcome::Failed &&
            s.world.factions.exists(n.faction) && (!n.other || (n.other!=n.faction && s.world.factions.exists(n.other))) &&
            (!n.originalContact || s.world.npcs.exists(n.originalContact)) && (!n.contact || s.world.npcs.exists(n.contact)) &&
            (!n.territory || s.world.territories.exists(n.territory)) && (!n.district || s.world.districts.exists(n.district)),"Invalid story node/reference");
        core::require(keys.emplace(n.kind,n.faction,n.other).second,"Duplicate story possibility");
        core::require((n.kind==StoryKind::Alliance || n.kind==StoryKind::TerritorialConflict)==static_cast<bool>(n.other),"Invalid story counterpart");
        core::require(!n.territory || s.world.territories.get(n.territory).district==n.district,"Incoherent story location");
        core::require(n.historyRecord<=s.history.events.size() && (!n.historyRecord ||
            s.history.events.at(n.historyRecord-1).faction==n.faction),"Invalid story history context");
        core::require(n.outcome!=StoryOutcome::Unresolved || !n.contact ||
            s.world.npcs.get(n.contact).faction==n.faction,"Invalid story contact affiliation");
        StoryNodeId previous;
        for (const auto& p : n.prerequisites) {
            // Strict backward edges establish a reachable DAG with no mandatory cycles.
            core::require(p.node>previous && p.node<id && s.story.nodes.exists(p.node) &&
                (p.outcome==StoryOutcome::Succeeded || p.outcome==StoryOutcome::Failed),"Invalid story prerequisite/cycle");
            previous=p.node;
        }
        core::require(n.kind==StoryKind::Contact || n.kind==StoryKind::Succession || !n.prerequisites.empty(),
            "Unreachable/unrooted story continuation");
    }
}
void appendStoryGraphHash(core::StateHash& h,const GameState& s) {
    h.u64(s.story.version); h.u64(s.story.nodes.nextId()); h.u64(s.story.nodes.entries().size());
    for (const auto& [id,n] : s.story.nodes.entries()) {
        for (auto v : {id.value(),static_cast<std::uint64_t>(n.kind),n.faction.value(),n.other.value(),n.originalContact.value(),
            n.contact.value(),n.territory.value(),n.district.value(),static_cast<std::uint64_t>(n.outcome)}) h.u64(v);
        h.u64(n.prerequisites.size());
        for (const auto& p : n.prerequisites) { h.u64(p.node.value()); h.u64(static_cast<std::uint64_t>(p.outcome)); }
        h.u64(n.historyRecord);
    }
}
}
