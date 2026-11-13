#include "world/spatial/streaming.hpp"
#include <algorithm>

namespace faultline::world {
WorldStreamingSystem::WorldStreamingSystem(StreamingConfig config) : config_(config) {
    core::require(std::isfinite(config.activationRadius) && std::isfinite(config.deactivationRadius) &&
        config.activationRadius >= 0 && config.deactivationRadius > config.activationRadius,
        "Streaming requires finite radii and hysteresis");
}
SimulationLevel WorldStreamingSystem::desiredLevel(bool active, double distance) const {
    core::require(std::isfinite(distance) && distance >= 0, "Invalid streaming distance");
    return distance <= (active ? config_.deactivationRadius : config_.activationRadius)
        ? SimulationLevel::Active : SimulationLevel::Abstract;
}
EntityId materializeNpc(WorldState& world, NpcId id) {
    auto& npc = world.npcs.get(id);
    core::require(!npc.activeEntity, "NPC is already materialized");
    core::require(!npc.semantic.travel,"Semantic travel must finish/cancel before materialization in this milestone");
    core::require(npc.lifecycle == NpcLifecycle::Alive || npc.lifecycle == NpcLifecycle::Injured,
                  "NPC lifecycle cannot materialize");
    core::require(!npc.faction || world.factions.exists(npc.faction), "Materializing NPC with missing faction");
    core::require(!npc.location.district || world.districts.exists(npc.location.district), "Materializing NPC with missing district");
    core::require(std::isfinite(npc.health.current) && std::isfinite(npc.health.maximum) &&
        npc.health.current > 0 && npc.health.maximum >= npc.health.current, "Materializing invalid NPC health");
    const auto position = normalize(npc.position, world.spatial.cellSize());
    const auto entity = world.entities.create();
    world.transforms.set(world.entities, entity, {position});
    world.health.set(world.entities, entity, npc.health);
    world.characterBodies.set(world.entities, entity,
        {0.0F, defaultNpcMovementSpeedMetersPerSec, defaultNpcStanceHeightMeters});
    world.npcLinks.set(world.entities, entity, {id});
    world.spatial.upsert(entity, position); npc.activeEntity = entity;
    return entity;
}
EntityId materializeVehicle(WorldState& world, VehiclePersistentId id, EntityId lease) {
    auto& vehicle = world.vehicles.get(id);
    core::require(!vehicle.activeEntity, "Vehicle is already materialized");
    core::require(validVehicleCategory(vehicle.category), "Vehicle category cannot materialize");
    core::require(validVehicleArchetype(vehicle.archetype), "Vehicle archetype cannot materialize");
    const auto& definition = vehicleDefinition(vehicle.archetype);
    core::require(vehicle.category == definition.category, "Vehicle category/archetype mismatch");
    core::require(std::isfinite(vehicle.condition.health.current) &&
        std::isfinite(vehicle.condition.health.maximum) && vehicle.condition.health.current > 0 &&
        vehicle.condition.health.maximum == definition.maxHealth, "Materializing invalid vehicle health");
    core::require(std::isfinite(vehicle.yaw) && vehicle.yaw >= -3.14159265F &&
        vehicle.yaw <= 3.14159265F, "Materializing invalid vehicle yaw");
    const auto position = normalize(vehicle.position, world.spatial.cellSize());
    EntityId entity;
    if (lease) {
        // Reuse the persisted residency lease instead of allocating a new handle.
        core::require(!world.entities.exists(lease) &&
            (world.entities.nextId() == 0 || lease.value() < world.entities.nextId()),
            "Invalid vehicle entity lease");
        auto live = world.entities.entries();
        live.insert(lease);
        world.entities = EntityRegistry::restore(world.entities.nextId(), std::move(live));
        entity = lease;
    } else {
        entity = world.entities.create();
    }
    world.transforms.set(world.entities, entity, {position});
    world.health.set(world.entities, entity, vehicle.condition.health);
    world.vehicleBodies.set(world.entities, entity,
        {vehicle.yaw, definition.halfLength, definition.halfWidth});
    world.vehiclePhysics.set(world.entities, entity,
        {vehicle.speed, definition.acceleration, definition.steeringRate});
    world.vehicleLinks.set(world.entities, entity, {id});
    world.spatial.upsert(entity, position); vehicle.activeEntity = entity;
    return entity;
}
void rebuildSpatialIndex(WorldState& world) {
    world.spatial.clear();
    for (const auto& [id, transform] : world.transforms.entries()) world.spatial.upsert(id, transform.position);
}
void WorldStreamingSystem::commit(WorldState& world, WorldPosition focus,
    std::optional<VehiclePersistentId> pinnedVehicle,
    std::span<const VehicleEntityLease> vehicleLeases) const {
    focus = normalize(focus, world.spatial.cellSize());
    for (std::size_t i = 1; i < vehicleLeases.size(); ++i)
        core::require(vehicleLeases[i - 1].vehicle < vehicleLeases[i].vehicle,
                      "Vehicle leases must be sorted by persistent id");
    for (const auto& [id, record] : world.npcs.entries()) {
        const auto active = record.activeEntity;
        const auto position = active ? world.transforms.get(active).position : record.position;
        const auto level = desiredLevel(static_cast<bool>(active), std::sqrt(distanceSquared(position, focus, world.spatial.cellSize())));
        const bool eligible = record.lifecycle == NpcLifecycle::Alive || record.lifecycle == NpcLifecycle::Injured;
        if (active && (level != SimulationLevel::Active || !eligible)) destroyEntity(world, active);
        else if (!active && level == SimulationLevel::Active && eligible && !record.semantic.travel) (void)materializeNpc(world, id);
    }
    for (const auto& [id, record] : world.vehicles.entries()) {
        const auto active = record.activeEntity;
        const auto position = active ? world.transforms.get(active).position : record.position;
        const auto level = desiredLevel(static_cast<bool>(active), std::sqrt(distanceSquared(position, focus, world.spatial.cellSize())));
        const bool pinned = pinnedVehicle.has_value() && *pinnedVehicle == id;
        const bool eligible = record.condition.health.current > 0 && validVehicleCategory(record.category) &&
            validVehicleArchetype(record.archetype);
        if (active && level != SimulationLevel::Active && !pinned) destroyEntity(world, active);
        else if (active && !eligible) destroyEntity(world, active);
        else if (!active && (level == SimulationLevel::Active || pinned) && eligible) {
            EntityId lease{};
            const auto found = std::lower_bound(vehicleLeases.begin(), vehicleLeases.end(), id,
                [](const VehicleEntityLease& value, VehiclePersistentId key) { return value.vehicle < key; });
            if (found != vehicleLeases.end() && found->vehicle == id) lease = found->entity;
            (void)materializeVehicle(world, id, lease);
        }
    }
    rebuildSpatialIndex(world);
}
}
