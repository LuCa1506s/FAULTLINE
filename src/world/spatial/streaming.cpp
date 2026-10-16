#include "world/spatial/streaming.hpp"

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
    world.npcLinks.set(world.entities, entity, {id});
    world.spatial.upsert(entity, position); npc.activeEntity = entity;
    return entity;
}
void rebuildSpatialIndex(WorldState& world) {
    world.spatial.clear();
    for (const auto& [id, transform] : world.transforms.entries()) world.spatial.upsert(id, transform.position);
}
void WorldStreamingSystem::commit(WorldState& world, WorldPosition focus) const {
    focus = normalize(focus, world.spatial.cellSize());
    for (const auto& [id, record] : world.npcs.entries()) {
        const auto active = record.activeEntity;
        const auto position = active ? world.transforms.get(active).position : record.position;
        const auto level = desiredLevel(static_cast<bool>(active), std::sqrt(distanceSquared(position, focus, world.spatial.cellSize())));
        const bool eligible = record.lifecycle == NpcLifecycle::Alive || record.lifecycle == NpcLifecycle::Injured;
        if (active && (level != SimulationLevel::Active || !eligible)) destroyEntity(world, active);
        else if (!active && level == SimulationLevel::Active && eligible && !record.semantic.travel) (void)materializeNpc(world, id);
    }
    rebuildSpatialIndex(world);
}
}
