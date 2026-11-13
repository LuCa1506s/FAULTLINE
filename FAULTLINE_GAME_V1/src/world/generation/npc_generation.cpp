#include "world/generation/actor_generation.hpp"
#include "world/generation/actor_names.hpp"
#include <algorithm>
#include <set>

namespace faultline::world {
MajorNpcGenerationResult generateMajorNpcs(const GenerationContext& context,const GeneratedWorld& world,
                                          std::span<const GeneratedFaction> factions) {
    std::map<TerritoryId,const GeneratedTerritory*> territories;
    for (const auto& t : world.territories) territories.emplace(t.id,&t);
    std::vector<GeneratedFaction> ordered(factions.begin(),factions.end());
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    MajorNpcGenerationResult result; std::set<std::string> names;
    for (const auto& faction : ordered) {
        const auto home=territories.at(faction.state.homeTerritory);
        core::DeterministicRng slots{core::deriveSeed(context.stageSeed("npc.slots"),"faction",faction.id)};
        const auto count=slots.uniform(3,6); std::set<BuildingId> usedBuildings;
        for (std::uint64_t i=0; i<count; ++i) {
            const NpcId id{result.npcs.size()+1};
            const auto localSeed=actor_detail::pairSeed(context.stageSeed("npc"),faction.id.value(),i);
            core::DeterministicRng rng{localSeed};
            core::DeterministicRng nameRng{actor_detail::pairSeed(context.stageSeed("npc.names"),faction.id.value(),i)};
            NpcState npc; npc.major=true; npc.faction=faction.id;
            npc.identity={actor_detail::uniqueName(faction.state.family,true,nameRng,id.value(),names),
                i==0 ? static_cast<AgeBand>(rng.uniform(2,4)) : static_cast<AgeBand>(rng.uniform(0,3)),rng.nextU64()};
            npc.roles.add(NpcRole::FactionMember);
            npc.authority=i==0 ? AuthorityTier::Leader : i==1 ? AuthorityTier::Command : i==2 ? AuthorityTier::Member : AuthorityTier::Senior;
            if (i==0) npc.roles.add(NpcRole::FactionLeader);
            if (i<2) npc.roles.add(NpcRole::MissionIssuer);
            if (i==2) npc.roles.add(NpcRole::Dealer);
            npc.respect=static_cast<std::uint8_t>(i==0 ? rng.uniform(70,96) : i==1 ? rng.uniform(50,81) : rng.uniform(30,71));
            const auto value=[&] {return static_cast<std::uint16_t>(rng.uniform(1500,9001));};
            npc.traits={actor_detail::bps((static_cast<std::int64_t>(faction.state.personality.aggression)+value())/2),value(),value(),value(),value(),value()};
            npc.loyalty=actor_detail::bps((static_cast<std::int64_t>(faction.state.cohesion)+npc.traits.loyaltyBias)/2);
            npc.capabilities={value(),value(),i==0 ? static_cast<std::uint16_t>(rng.uniform(6500,9501)) : value(),value(),value()};
            std::vector<const BuildingState*> candidates;
            for (const auto& building : world.buildings) if (building.district==home->district && inside(building.footprint,home->boundary) && !usedBuildings.contains(building.id))
                candidates.push_back(&building);
            std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a->id<b->id;});
            npc.location={home->district,home->id,{}};
            DoubleVec3 position{static_cast<double>(home->boundary.x)+home->boundary.width/2.0,0,
                static_cast<double>(home->boundary.z)+home->boundary.depth/2.0};
            if (!candidates.empty()) {
                const auto preference=[&](const BuildingState* b) {
                    return i==2 ? (b->archetype==BuildingArchetype::Store || b->archetype==BuildingArchetype::Garage || b->archetype==BuildingArchetype::Warehouse)
                        : (b->archetype==BuildingArchetype::Office || b->archetype==BuildingArchetype::Apartment || b->archetype==BuildingArchetype::House);
                };
                const bool preferred=std::any_of(candidates.begin(),candidates.end(),preference);
                if (preferred) std::erase_if(candidates,[&](auto b){return !preference(b);});
                const auto building=candidates[static_cast<std::size_t>(rng.uniform(candidates.size()))];
                npc.location.building=building->id; usedBuildings.insert(building->id);
                position={static_cast<double>(building->transform.centerX2)/2.0,0,static_cast<double>(building->transform.centerZ2)/2.0};
            }
            npc.position=fromGlobal(position); result.npcs.push_back({id,std::move(npc)});
        }
    }
    return result;
}
}
