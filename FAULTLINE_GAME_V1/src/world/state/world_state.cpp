#include "world/state/world_state.hpp"
#include <bit>
#include <type_traits>
#include "world/generation/actor_generation.hpp"

namespace faultline::world {
void applyStructuralCommand(WorldState& world, const StructuralCommand& command) {
    std::visit([&world](const auto& action) {
        using T = std::decay_t<decltype(action)>;
        if constexpr (std::is_same_v<T, CreateEntityCommand>) (void)world.entities.create();
        else destroyEntity(world, action.entity);
    }, command);
}
void destroyEntity(WorldState& world, EntityId id) {
    core::require(world.entities.exists(id), "Destroying unknown entity");
    if (world.npcLinks.contains(id)) {
        auto& npc = world.npcs.get(world.npcLinks.get(id).npc);
        if (world.transforms.contains(id)) npc.position = normalize(world.transforms.get(id).position, world.spatial.cellSize());
        if (world.health.contains(id)) npc.health = world.health.get(id);
        npc.activeEntity = {};
    }
    if (world.vehicleLinks.contains(id)) {
        auto& vehicle = world.vehicles.get(world.vehicleLinks.get(id).vehicle);
        if (world.transforms.contains(id))
            vehicle.position = normalize(world.transforms.get(id).position, world.spatial.cellSize());
        if (world.vehicleBodies.contains(id)) vehicle.yaw = world.vehicleBodies.get(id).yaw;
        if (world.vehiclePhysics.contains(id)) vehicle.speed = world.vehiclePhysics.get(id).speed;
        if (world.health.contains(id)) vehicle.condition.health = world.health.get(id);
        vehicle.activeEntity = {};
    }
    world.npcLinks.remove(id); world.vehicleLinks.remove(id);
    world.transforms.remove(id); world.health.remove(id); world.characterBodies.remove(id);
    world.vehicleBodies.remove(id); world.vehiclePhysics.remove(id);
    world.spatial.remove(id); world.entities.destroy(id);
}
WorldState persistentSnapshot(const WorldState& world) {
    WorldState result = world;
    for (const auto& [id, npc] : world.npcs.entries()) {
        (void)id;
        if (npc.activeEntity) destroyEntity(result, npc.activeEntity);
    }
    for (const auto& [id, vehicle] : world.vehicles.entries()) {
        (void)id;
        if (vehicle.activeEntity) destroyEntity(result, vehicle.activeEntity);
    }
    result.spatial.clear();
    // Relationship vectors are semantic sets of directed edges, not insertion histories.
    std::sort(result.factionRelationships.begin(), result.factionRelationships.end());
    std::sort(result.npcRelationships.begin(), result.npcRelationships.end());
    return result;
}
void validateWorld(const WorldState& world) {
    if (world.base) {
        const auto report = validateGeneratedWorld(*world.base);
        core::require(report.passed(), "Invalid generated base world");
        for (const auto& district : world.base->districts) {
            core::require(world.districts.exists(district.id) &&
                world.districts.get(district.id).macroZone == district.macroZone, "Generated district reference mismatch");
        }
        for (const auto& territory : world.base->territories) {
            core::require(world.territories.exists(territory.id) &&
                world.territories.get(territory.id).district == territory.district, "Generated territory reference mismatch");
        }
        for (const auto& f : world.base->factions) core::require(world.factions.exists(f.id),"Generated faction missing from store");
        for (const auto& n : world.base->majorNpcs) core::require(world.npcs.exists(n.id),"Generated NPC missing from store");
    }
    for (const auto& [id, transform] : world.transforms.entries()) {
        core::require(world.entities.exists(id), "Orphan transform");
        core::require(normalize(transform.position, world.spatial.cellSize()) == transform.position, "Unnormalized transform");
    }
    const auto validHealth = [](Health health) {
        return std::isfinite(health.current) && std::isfinite(health.maximum) && health.maximum > 0 &&
               health.current >= 0 && health.current <= health.maximum;
    };
    for (const auto& [id, health] : world.health.entries())
        core::require(world.entities.exists(id) && validHealth(health), "Invalid physical health");
    const auto validCharacterBody = [](const CharacterBody& body) {
        return std::isfinite(body.yaw) && body.yaw >= -3.14159265F && body.yaw <= 3.14159265F &&
               std::isfinite(body.movementSpeed) && body.movementSpeed > 0 &&
               std::isfinite(body.stanceHeight) && body.stanceHeight > 0;
    };
    for (const auto& [id, body] : world.characterBodies.entries())
        core::require(world.entities.exists(id) && validCharacterBody(body), "Invalid character body");
    const auto validVehicleBody = [](const VehicleBody& body) {
        return std::isfinite(body.yaw) && body.yaw >= -3.14159265F && body.yaw <= 3.14159265F &&
               std::isfinite(body.halfLength) && body.halfLength > 0 &&
               std::isfinite(body.halfWidth) && body.halfWidth > 0;
    };
    for (const auto& [id, body] : world.vehicleBodies.entries())
        core::require(world.entities.exists(id) && validVehicleBody(body), "Invalid vehicle body");
    for (const auto& [id, physics] : world.vehiclePhysics.entries())
        core::require(world.entities.exists(id) && std::isfinite(physics.speed) &&
            std::isfinite(physics.acceleration) && physics.acceleration > 0 &&
            std::isfinite(physics.handling) && physics.handling > 0, "Invalid vehicle physics");
    for (const auto& [id, npc] : world.npcs.entries()) {
        core::require(npc.identity.age <= AgeBand::OlderAdult, "Invalid NPC age band");
        core::require(npc.lifecycle <= NpcLifecycle::Dead, "Invalid NPC lifecycle");
        core::require(normalize(npc.position, world.spatial.cellSize()) == npc.position && validHealth(npc.health), "Invalid semantic NPC state");
        core::require(!npc.faction || world.factions.exists(npc.faction), "NPC faction missing");
        core::require(!npc.location.district || world.districts.exists(npc.location.district), "NPC district missing");
        core::require(!npc.location.territory || (world.territories.exists(npc.location.territory) &&
            world.territories.get(npc.location.territory).district==npc.location.district),"NPC territory missing/incoherent");
        if (npc.location.building) {
            core::require(world.base.has_value(),"NPC building requires generated geography");
            const auto& buildings=world.base->buildings;
            const auto found=std::find_if(buildings.begin(),buildings.end(),[&](const auto& b){return b.id==npc.location.building;});
            core::require(found!=buildings.end() && found->district==npc.location.district,"NPC building missing/incoherent");
        }
        core::require(npc.authority<=AuthorityTier::Leader && npc.respect<=100 && npc.loyalty<=10000 && (npc.roles.bits & ~63U)==0,"Invalid NPC authority/social state");
        for (auto v : {npc.traits.aggression,npc.traits.ambition,npc.traits.riskTolerance,npc.traits.loyaltyBias,npc.traits.greed,npc.traits.courage,
            npc.capabilities.combat,npc.capabilities.driving,npc.capabilities.leadership,npc.capabilities.streetKnowledge,npc.capabilities.business})
            core::require(v<=10000,"Invalid NPC normalized state");
        if (npc.activeEntity) {
            core::require(npc.lifecycle == NpcLifecycle::Alive || npc.lifecycle == NpcLifecycle::Injured,
                          "Non-physical NPC lifecycle has a body");
            core::require(world.entities.exists(npc.activeEntity), "NPC entity missing");
            core::require(world.transforms.contains(npc.activeEntity) && world.health.contains(npc.activeEntity),
                          "NPC physical components missing");
            core::require(world.characterBodies.contains(npc.activeEntity),
                          "NPC physical character body missing");
            core::require(world.npcLinks.contains(npc.activeEntity) &&
                world.npcLinks.get(npc.activeEntity).npc == id, "NPC back-link mismatch");
        }
    }
    for (const auto& [entity, link] : world.npcLinks.entries()) {
        core::require(world.entities.exists(entity) && world.npcs.exists(link.npc), "Orphan NPC link");
        core::require(world.npcs.get(link.npc).activeEntity == entity, "Duplicate or stale NPC body");
        core::require(!world.vehicleLinks.contains(entity), "Entity represents both NPC and vehicle");
    }
    for (const auto& [id, vehicle] : world.vehicles.entries()) {
        core::require(validVehicleCategory(vehicle.category) && validVehicleArchetype(vehicle.archetype),
                      "Invalid vehicle category/archetype");
        const auto& definition = vehicleDefinition(vehicle.archetype);
        core::require(vehicle.category == definition.category, "Vehicle category/archetype mismatch");
        core::require(validVehicleOwnerKind(vehicle.ownership.kind), "Invalid vehicle owner kind");
        core::require(vehicle.passengerCapacity == definition.passengerCapacity,
                      "Vehicle passenger capacity mismatch");
        core::require(std::isfinite(vehicle.yaw) && vehicle.yaw >= -3.14159265F && vehicle.yaw <= 3.14159265F &&
            std::isfinite(vehicle.speed) && std::abs(vehicle.speed) <= definition.maxSpeed + 1e-3F,
            "Invalid vehicle motion state");
        core::require(normalize(vehicle.position, world.spatial.cellSize()) == vehicle.position,
                      "Unnormalized vehicle position");
        core::require(validHealth(vehicle.condition.health) &&
            vehicle.condition.health.maximum == definition.maxHealth, "Invalid vehicle condition");
        switch (vehicle.ownership.kind) {
        case VehicleOwnerKind::Unowned:
            core::require(!vehicle.ownership.npc && !vehicle.ownership.faction, "Unowned vehicle references an owner");
            break;
        case VehicleOwnerKind::Npc:
            core::require(vehicle.ownership.npc && world.npcs.exists(vehicle.ownership.npc) &&
                !vehicle.ownership.faction, "Vehicle NPC owner missing/incoherent");
            break;
        case VehicleOwnerKind::Faction:
            core::require(vehicle.ownership.faction && world.factions.exists(vehicle.ownership.faction) &&
                !vehicle.ownership.npc, "Vehicle faction owner missing/incoherent");
            break;
        case VehicleOwnerKind::Police:
            core::require(!vehicle.ownership.npc && (!vehicle.ownership.faction ||
                world.factions.exists(vehicle.ownership.faction)), "Vehicle police owner missing/incoherent");
            break;
        }
        core::require(!vehicle.location.district || world.districts.exists(vehicle.location.district),
                      "Vehicle district missing");
        core::require(!vehicle.location.territory || (world.territories.exists(vehicle.location.territory) &&
            world.territories.get(vehicle.location.territory).district == vehicle.location.district),
            "Vehicle territory missing/incoherent");
        if (vehicle.location.building) {
            core::require(world.base.has_value(), "Vehicle building requires generated geography");
            const auto& buildings = world.base->buildings;
            const auto found = std::find_if(buildings.begin(), buildings.end(),
                [&](const auto& b) { return b.id == vehicle.location.building; });
            core::require(found != buildings.end() && found->district == vehicle.location.district,
                          "Vehicle building missing/incoherent");
        }
        if (vehicle.activeEntity) {
            const auto entity = vehicle.activeEntity;
            core::require(vehicle.condition.health.current > 0, "Wrecked vehicle has a body");
            core::require(world.entities.exists(entity) && world.vehicleLinks.contains(entity),
                          "Vehicle entity missing");
            core::require(world.vehicleLinks.get(entity).vehicle == id, "Vehicle back-link mismatch");
            core::require(world.transforms.contains(entity) && world.health.contains(entity) &&
                world.vehicleBodies.contains(entity) && world.vehiclePhysics.contains(entity),
                "Vehicle physical components missing");
            core::require(normalize(world.transforms.get(entity).position, world.spatial.cellSize()) ==
                world.transforms.get(entity).position, "Unnormalized vehicle transform");
            core::require(world.vehicleBodies.get(entity).halfLength == definition.halfLength &&
                world.vehicleBodies.get(entity).halfWidth == definition.halfWidth,
                "Vehicle body dimensions mismatch");
        }
    }
    for (const auto& [entity, link] : world.vehicleLinks.entries()) {
        core::require(world.entities.exists(entity) && world.vehicles.exists(link.vehicle), "Orphan vehicle link");
        core::require(world.vehicles.get(link.vehicle).activeEntity == entity, "Duplicate vehicle body");
        core::require(!world.npcLinks.contains(entity), "Entity represents both NPC and vehicle");
    }
    for (const auto& [id, faction] : world.factions.entries()) {
        core::require(faction.lifecycle <= FactionLifecycle::Reformed, "Invalid faction lifecycle");
        core::require(faction.family<=FactionFamily::Cartel && faction.treasury>=0 && faction.cohesion<=10000,"Invalid faction identity/resources");
        for (auto v : {faction.personality.aggression,faction.personality.riskTolerance,faction.personality.marketFocus,faction.personality.territorialFocus})
            core::require(v<=10000,"Invalid faction personality");
        if (faction.leader) {
            core::require(world.npcs.exists(faction.leader), "Faction leader missing");
            core::require(world.npcs.get(faction.leader).lifecycle == NpcLifecycle::Alive, "Dead faction leader");
            core::require(world.npcs.get(faction.leader).faction==id,"Faction leader membership mismatch");
        }
        core::require(!faction.homeTerritory || world.territories.exists(faction.homeTerritory), "Faction home missing");
    }
    for (const auto& [id, territory] : world.territories.entries()) {
        (void)id; core::require(world.districts.exists(territory.district), "Territory district missing");
        core::require(!territory.owner || world.factions.exists(territory.owner),"Territory owner missing");
        // Runtime territory validation separately checks control and garrison membership.
        // Generated initial-state validation retains its original stricter contract.
        // Phase 11 validates separate garrison membership; contested control has no owner.
    }
    for (const auto& [id, district] : world.districts.entries()) {
        (void)id;
        if (district.macroZone) {
            core::require(world.base.has_value(), "District macro-zone requires generated geography");
            const auto& zones=world.base->macroZones;
            core::require(std::any_of(zones.begin(),zones.end(),[&](const auto& zone){return zone.id==district.macroZone;}),
                          "District macro-zone missing");
        }
    }
    for (const auto& [id, mission] : world.missions.entries()) {
        (void)id;
        core::require(mission.status <= MissionResolution::ResolvedByNpc, "Invalid mission state");
        core::require(!mission.issuer || world.npcs.exists(mission.issuer), "Mission issuer missing");
    }
    std::set<std::pair<FactionId,FactionId>> political;
    for (const auto& r : world.factionRelationships) {
        core::require((r.context & ~15U) == 0, "Invalid faction relationship context");
        core::require(world.factions.exists(r.from) && world.factions.exists(r.to) && r.from!=r.to && political.emplace(r.from,r.to).second,
                      "Invalid faction relationship references");
        for (auto v : {r.trust,r.hostility,r.fear,r.sharedInterest,r.historicalGrievance}) core::require(v<=10000,"Invalid faction relationship value");
    }
    std::set<std::pair<NpcId,NpcId>> personal;
    for (const auto& r : world.npcRelationships) {
        core::require(r.context <= NpcRelationContext::MeaningfulInteraction, "Invalid NPC relationship context");
        core::require(world.npcs.exists(r.from) && world.npcs.exists(r.to) && r.from!=r.to && personal.emplace(r.from,r.to).second,
                      "Invalid NPC relationship references");
        for (auto v : {r.trust,r.fear,r.hostility,r.personalLoyalty,r.debt}) core::require(v<=10000,"Invalid NPC relationship value");
    }
}
template<class Store, class Append> void appendStore(core::StateHash& hash, const Store& store, Append append) {
    hash.u64(store.nextId()); hash.u64(store.entries().size());
    for (const auto& [id, record] : store.entries()) { hash.u64(id.value()); append(record); }
}
void appendStateHash(core::StateHash& hash, const WorldState& world) {
    hash.u64(world.base.has_value());
    if (world.base) hash.u64(baseWorldHash(*world.base));
    const auto appendFloat = [&](float value) {
        // Canonicalize signed zero; reject non-finite state via validation.
        hash.u64(std::bit_cast<std::uint32_t>(value == 0 ? 0.0F : value));
    };
    const auto appendPosition = [&](WorldPosition position) {
        hash.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(position.cell.x)));
        hash.u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(position.cell.z)));
        appendFloat(position.local.x); appendFloat(position.local.y); appendFloat(position.local.z);
    };
    appendFloat(world.spatial.cellSize());
    hash.u64(world.transforms.entries().size());
    for (const auto& [id, transform] : world.transforms.entries()) { hash.u64(id.value()); appendPosition(transform.position); }
    hash.u64(world.health.entries().size());
    for (const auto& [id, health] : world.health.entries()) { hash.u64(id.value()); appendFloat(health.current); appendFloat(health.maximum); }
    hash.u64(world.characterBodies.entries().size());
    for (const auto& [id, body] : world.characterBodies.entries()) { hash.u64(id.value()); appendFloat(body.yaw); appendFloat(body.movementSpeed); appendFloat(body.stanceHeight); }
    hash.u64(world.entities.nextId()); hash.u64(world.entities.entries().size());
    for (auto id : world.entities.entries()) hash.u64(id.value());
    hash.u64(world.npcLinks.entries().size());
    for (const auto& [id, link] : world.npcLinks.entries()) { hash.u64(id.value()); hash.u64(link.npc.value()); }
    hash.u64(world.vehicleLinks.entries().size());
    for (const auto& [id, link] : world.vehicleLinks.entries()) { hash.u64(id.value()); hash.u64(link.vehicle.value()); }
    appendStore(hash, world.npcs, [&](const auto& npc) { appendNpcHash(hash,npc); });
    appendStore(hash, world.factions, [&](const auto& faction) { appendFactionHash(hash,faction); });
    appendStore(hash, world.territories, [&](const auto& territory) {
        hash.u64(territory.district.value()); hash.u64(territory.owner.value()); hash.u64(territory.residentMembers);
    });
    appendStore(hash, world.vehicles, [&](const auto& vehicle) {
        hash.u64(vehicle.activeEntity.value());
        hash.u64(static_cast<std::uint64_t>(vehicle.category));
        hash.u64(static_cast<std::uint64_t>(vehicle.archetype));
        hash.u64(static_cast<std::uint64_t>(vehicle.ownership.kind));
        hash.u64(vehicle.ownership.npc.value()); hash.u64(vehicle.ownership.faction.value());
        appendFloat(vehicle.condition.health.current); appendFloat(vehicle.condition.health.maximum);
        hash.u64(vehicle.location.district.value()); hash.u64(vehicle.location.territory.value());
        hash.u64(vehicle.location.building.value());
        appendPosition(vehicle.position); appendFloat(vehicle.yaw); appendFloat(vehicle.speed);
        hash.u64(vehicle.passengerCapacity);
    });
    appendStore(hash, world.missions, [&](const auto& mission) {
        hash.u64(static_cast<std::uint64_t>(mission.status)); hash.u64(mission.issuer.value());
    });
    appendStore(hash, world.districts, [&](const auto& district) { hash.u64(district.macroZone.value()); });
    appendRelationshipHash(hash,world.factionRelationships,world.npcRelationships);
}
}
