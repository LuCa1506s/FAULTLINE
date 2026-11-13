#include "world/generation/actor_generation.hpp"
#include <bit>

namespace faultline::world {
void appendNpcHash(core::StateHash& h,const NpcState& n) {
    h.u64(static_cast<std::uint64_t>(n.lifecycle)); h.u64(n.faction.value());
    h.u64(n.location.district.value()); h.u64(n.location.territory.value()); h.u64(n.location.building.value()); h.u64(n.activeEntity.value());
    const auto number=[&](float value){h.u64(std::bit_cast<std::uint32_t>(value==0 ? 0.0F : value));};
    h.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(n.position.cell.x)));
    h.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(n.position.cell.z)));
    number(n.position.local.x); number(n.position.local.y); number(n.position.local.z); number(n.health.current); number(n.health.maximum);
    h.string(n.identity.name); h.u64(static_cast<std::uint64_t>(n.identity.age)); h.u64(n.identity.appearanceSeed);
    h.u64(n.roles.bits); h.u64(static_cast<std::uint64_t>(n.authority)); h.u64(n.respect); h.u64(n.loyalty);
    for (auto value : {n.traits.aggression,n.traits.ambition,n.traits.riskTolerance,n.traits.loyaltyBias,n.traits.greed,n.traits.courage,
        n.capabilities.combat,n.capabilities.driving,n.capabilities.leadership,n.capabilities.streetKnowledge,n.capabilities.business}) h.u64(value);
    h.u64(n.major);
}
void appendFactionHash(core::StateHash& h,const FactionState& f) {
    h.u64(static_cast<std::uint64_t>(f.lifecycle)); h.u64(f.leader.value()); h.u64(f.homeTerritory.value());
    h.string(f.name); h.u64(static_cast<std::uint64_t>(f.family));
    for (auto value : {f.personality.aggression,f.personality.riskTolerance,f.personality.marketFocus,f.personality.territorialFocus,f.cohesion}) h.u64(value);
    h.u64(static_cast<std::uint64_t>(f.treasury)); h.u64(f.reserveMembers);
}
void appendRelationshipHash(core::StateHash& h,std::span<const FactionRelationship> factions,std::span<const NpcRelationship> npcs) {
    h.u64(factions.size());
    for (const auto& r : factions) {
        h.u64(r.from.value()); h.u64(r.to.value());
        for (auto value : {r.trust,r.hostility,r.fear,r.sharedInterest,r.historicalGrievance}) h.u64(value);
        h.u64(r.context);
    }
    h.u64(npcs.size());
    for (const auto& r : npcs) {
        h.u64(r.from.value()); h.u64(r.to.value());
        for (auto value : {r.trust,r.fear,r.hostility,r.personalLoyalty,r.debt}) h.u64(value);
        h.u64(static_cast<std::uint64_t>(r.context));
    }
}
void appendActorsHash(core::StateHash& h,const GeneratedWorld& w) {
    h.u64(w.factions.size()); for (const auto& f : w.factions) {h.u64(f.id.value()); appendFactionHash(h,f.state);}
    h.u64(w.majorNpcs.size()); for (const auto& n : w.majorNpcs) {h.u64(n.id.value()); appendNpcHash(h,n.state);}
    h.u64(w.territoryAssignments.size()); for (const auto& a : w.territoryAssignments) {h.u64(a.territory.value()); h.u64(a.owner.value()); h.u64(a.residentMembers);}
    appendRelationshipHash(h,w.factionRelationships,w.npcRelationships);
}
}
