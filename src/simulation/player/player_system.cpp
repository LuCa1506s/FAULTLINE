#include "simulation/player/player_system.hpp"
#include "world/spatial/streaming.hpp"
#include "world/spatial/character_movement.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace faultline::simulation {
using namespace world;

void initializePlayerState(GameState& state) {
    state.player = PlayerState{};
}

void restorePlayerResidency(GameState& state) {
    auto& w=state.world;
    for (const auto& r : state.player.residents) {
        auto& n=w.npcs.get(r.npc);
        if (n.activeEntity) {
            core::require(n.activeEntity==r.entity,"Streaming lease/body mismatch");
            continue;
        }
        core::require(!w.entities.exists(r.entity) && (w.entities.nextId()==0 || r.entity.value()<w.entities.nextId()),
                      "Invalid streaming allocation lease");
        core::require(!n.semantic.travel && (n.lifecycle==NpcLifecycle::Alive || n.lifecycle==NpcLifecycle::Injured),
                      "Ineligible streaming resident");
        auto live=w.entities.entries(); live.insert(r.entity);
        w.entities=EntityRegistry::restore(w.entities.nextId(),std::move(live));
        w.transforms.set(w.entities,r.entity,{n.position});
        w.health.set(w.entities,r.entity,n.health);
        w.npcLinks.set(w.entities,r.entity,{r.npc});
        w.spatial.upsert(r.entity,n.position); n.activeEntity=r.entity;
    }
}

EntityId createPlayerEntity(GameState& state, WorldPosition spawnPosition) {
    core::require(!state.player.controlledEntity, "Player entity already exists");
    spawnPosition = normalize(spawnPosition, state.world.spatial.cellSize());
    const auto entity = state.world.entities.create();
    state.world.transforms.set(state.world.entities, entity, {spawnPosition});
    state.world.health.set(state.world.entities, entity, {100, 100});
    state.world.characterBodies.set(state.world.entities, entity, {0, playerMovementSpeedMetersPerSec, playerStanceHeightMeters});
    state.world.spatial.upsert(entity, spawnPosition);
    state.player.controlledEntity = entity;
    state.player.spawnPosition = spawnPosition;
    state.player.yaw = 0;
    return entity;
}

void updatePlayerMovement(GameState& state, const PlayerInputFrame& input, WorldStreamingSystem& streaming) {
    const auto entity = state.player.controlledEntity;
    if (!entity || !state.world.entities.exists(entity)) return;
    auto& transform = state.world.transforms.get(entity);
    auto& body = state.world.characterBodies.get(entity);
    if (std::isfinite(input.aimX) && std::isfinite(input.aimZ) &&
        (std::fabs(input.aimX) > 0 || std::fabs(input.aimZ) > 0)) {
        body.yaw = std::atan2(input.aimX, input.aimZ);
        state.player.yaw = body.yaw;
    }
    const double magnitude = std::hypot(static_cast<double>(input.moveX), static_cast<double>(input.moveZ));
    if (magnitude > 0 && std::isfinite(magnitude)) {
        if (std::fabs(input.aimX) <= 0 && std::fabs(input.aimZ) <= 0) {
            body.yaw = std::atan2(input.moveX, input.moveZ);
            state.player.yaw = body.yaw;
        }
    }
    const float nx = magnitude>0 ? static_cast<float>(input.moveX / std::max(1.0,magnitude)) : 0;
    const float nz = magnitude>0 ? static_cast<float>(input.moveZ / std::max(1.0,magnitude)) : 0;
    const float tickSeconds = static_cast<float>(core::SimulationClock::tickDuration.count()) / 1000.0F;
    const float shiftX = nx * body.movementSpeed * tickSeconds;
    const float shiftZ = nz * body.movementSpeed * tickSeconds;
    transform.position = moveCharacter(state.world,transform.position,shiftX,shiftZ);
    state.world.spatial.upsert(entity, transform.position);
    streaming.commit(state.world, transform.position);
    state.player.residents.clear();
    for (const auto& [id,n] : state.world.npcs.entries())
        if (n.activeEntity) state.player.residents.push_back({id,n.activeEntity});
}

void validatePlayerState(const GameState& state) {
    core::require(state.player.version == currentPlayerStateVersion, "Unsupported player state version");
    const auto& w = state.world;
    if (state.player.controlledEntity) {
        core::require(w.entities.exists(state.player.controlledEntity), "Player controlled entity missing");
        core::require(w.transforms.contains(state.player.controlledEntity), "Player entity has no transform");
        core::require(w.health.contains(state.player.controlledEntity), "Player entity has no health");
        core::require(w.characterBodies.contains(state.player.controlledEntity), "Player entity has no character body");
        core::require(!w.npcLinks.contains(state.player.controlledEntity) && !w.vehicleLinks.contains(state.player.controlledEntity),
                      "Player cannot also represent an NPC or vehicle");
        core::require(state.player.yaw==w.characterBodies.get(state.player.controlledEntity).yaw,"Player facing cache/body mismatch");
        const auto& hp = w.health.get(state.player.controlledEntity);
        core::require(std::isfinite(hp.current) && hp.current > 0 && hp.maximum >= hp.current, "Player health invalid");
        core::require(std::isfinite(state.player.yaw) && state.player.yaw >= -3.14159265F && state.player.yaw <= 3.14159265F, "Player yaw out of range");
        core::require(normalize(state.player.spawnPosition, w.spatial.cellSize()) == state.player.spawnPosition, "Player spawn position not normalized");
    } else {
        core::require(state.player.yaw == 0.0F, "Player yaw non-zero without controlled entity");
        core::require(state.player.residents.empty(),"Streaming residents without player");
    }
    NpcId previous; std::set<EntityId> leased;
    for (const auto& r : state.player.residents) {
        core::require(r.npc>previous && w.npcs.exists(r.npc) && r.entity && r.entity!=state.player.controlledEntity &&
            leased.insert(r.entity).second && (w.entities.nextId()==0 || r.entity.value()<w.entities.nextId()),"Invalid streaming lease");
        const auto& n=w.npcs.get(r.npc);
        core::require(!n.semantic.travel && (n.lifecycle==NpcLifecycle::Alive || n.lifecycle==NpcLifecycle::Injured),"Invalid streaming resident");
        core::require(n.activeEntity ? n.activeEntity==r.entity : !w.entities.exists(r.entity),"Streaming lease collision");
        previous=r.npc;
    }
    if (state.player.controlledEntity)
        for (const auto& [id,n] : w.npcs.entries()) if (n.activeEntity) {
            core::require(std::binary_search(state.player.residents.begin(),state.player.residents.end(),NpcResidency{id,n.activeEntity}),
                          "Active NPC missing streaming lease");
        }
}

void appendPlayerStateHash(core::StateHash& hash, const GameState& state) {
    hash.u64(state.player.version);
    hash.u64(state.player.controlledEntity.value());
    hash.u64(static_cast<std::uint64_t>(state.player.spawnPosition.cell.x));
    hash.u64(static_cast<std::uint64_t>(state.player.spawnPosition.cell.z));
    const auto appendFloat = [&](float value) {
        hash.u64(std::bit_cast<std::uint32_t>(value == 0 ? 0.0F : value));
    };
    appendFloat(state.player.spawnPosition.local.x);
    appendFloat(state.player.spawnPosition.local.y);
    appendFloat(state.player.spawnPosition.local.z);
    appendFloat(state.player.yaw);
    hash.u64(state.player.residents.size());
    for (const auto& r : state.player.residents) { hash.u64(r.npc.value()); hash.u64(r.entity.value()); }
}

}
