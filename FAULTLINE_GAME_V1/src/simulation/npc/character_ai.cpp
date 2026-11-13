#include "simulation/npc/character_ai.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/factions/faction_state.hpp"
#include "world/spatial/character_movement.hpp"
#include <algorithm>
#include <cmath>

namespace faultline::simulation {
namespace {

world::WorldPosition locationPosition(const GameState& state, world::WorldLocation location) {
    core::require(validNpcLocation(state.world, location) && state.world.base.has_value(),
                  "Physical AI target requires generated semantic geography");
    const auto& base = *state.world.base;
    if (location.building) {
        const auto found = std::lower_bound(base.buildings.begin(), base.buildings.end(), location.building,
            [](const auto& value, BuildingId id) { return value.id < id; });
        core::require(found != base.buildings.end() && found->id == location.building,
                      "Physical AI building target missing");
        return world::fromGlobal({static_cast<double>(found->transform.centerX2) / 2.0, 0,
                                  static_cast<double>(found->transform.centerZ2) / 2.0},
                                 state.world.spatial.cellSize());
    }
    world::Rect boundary;
    if (location.territory) {
        const auto found = std::lower_bound(base.territories.begin(), base.territories.end(), location.territory,
            [](const auto& value, TerritoryId id) { return value.id < id; });
        core::require(found != base.territories.end() && found->id == location.territory,
                      "Physical AI territory target missing");
        boundary = found->boundary;
    } else {
        const auto found = std::lower_bound(base.districts.begin(), base.districts.end(), location.district,
            [](const auto& value, DistrictId id) { return value.id < id; });
        core::require(found != base.districts.end() && found->id == location.district,
                      "Physical AI district target missing");
        boundary = found->boundary;
    }
    return world::fromGlobal({static_cast<double>(boundary.x) + boundary.width / 2.0, 0,
                              static_cast<double>(boundary.z) + boundary.depth / 2.0},
                             state.world.spatial.cellSize());
}

world::WorldPosition territoryPosition(const GameState& state, TerritoryId territory) {
    const auto& value = state.world.territories.get(territory);
    return locationPosition(state, {value.district, territory, {}});
}

bool physicalNpcValid(const GameState& state, NpcId id) {
    if (!state.world.npcs.exists(id)) return false;
    const auto& npc = state.world.npcs.get(id);
    const auto entity = npc.activeEntity;
    return entity && state.world.entities.exists(entity) && state.world.npcLinks.contains(entity) &&
        state.world.npcLinks.get(entity).npc == id && state.world.transforms.contains(entity) &&
        state.world.health.contains(entity) && state.world.characterBodies.contains(entity);
}

CharacterAiIntent targeted(const world::NpcState& npc, NpcId id,
                           CharacterAiBehavior behavior, world::WorldPosition target,
                           NpcId targetNpc = {}) {
    return {id, npc.activeEntity, behavior, true, target, targetNpc};
}

} // namespace

std::vector<EntityId> perceiveCharacters(const GameState& state, NpcId observer, double radius) {
    core::require(physicalNpcValid(state, observer), "Physical perception requires a valid active NPC");
    const auto entity = state.world.npcs.get(observer).activeEntity;
    const auto origin = state.world.transforms.get(entity).position;
    auto result = state.world.spatial.queryRadius(origin, radius, [&](EntityId id) {
        return state.world.transforms.get(id).position;
    });
    std::erase_if(result, [&](EntityId id) {
        return id == entity || !state.world.characterBodies.contains(id);
    });
    return result;
}

CharacterAiIntent decideCharacterIntent(const GameState& state, NpcId id) {
    core::require(physicalNpcValid(state, id), "Physical AI requires a valid active NPC body");
    const auto& npc = state.world.npcs.get(id);
    core::require(!npc.semantic.travel, "Semantic travel and physical AI cannot run together");

    // Survival overrides assignments. Hiding is an explicit semantic order; an
    // injured low-courage actor also retreats to its existing home fact.
    if ((npc.semantic.activity == world::NpcActivity::Hiding ||
         (npc.lifecycle == world::NpcLifecycle::Injured && npc.traits.courage < 5000)) &&
        npc.semantic.home.district) {
        return targeted(npc, id, CharacterAiBehavior::Flee,
                        locationPosition(state, npc.semantic.home));
    }

    if (npc.semantic.operation && state.factionSimulation.operations.exists(npc.semantic.operation)) {
        const auto& operation = state.factionSimulation.operations.get(npc.semantic.operation);
        if (pending(operation.status)) {
            return targeted(npc, id,
                operation.type == OperationType::AttackTerritory
                    ? CharacterAiBehavior::AttackIntent
                    : CharacterAiBehavior::Protect,
                territoryPosition(state, operation.target));
        }
    }

    // Meeting is the minimal follow foundation. The target must be locally
    // perceived; semantic activity does not grant omniscient physical tracking.
    if (npc.semantic.activity == world::NpcActivity::Meeting && npc.faction &&
        state.world.factions.exists(npc.faction)) {
        const auto leader = state.world.factions.get(npc.faction).leader;
        if (leader && leader != id && physicalNpcValid(state, leader)) {
            const auto visible = perceiveCharacters(state, id);
            const auto targetEntity = state.world.npcs.get(leader).activeEntity;
            if (std::binary_search(visible.begin(), visible.end(), targetEntity))
                return targeted(npc, id, CharacterAiBehavior::Follow,
                                state.world.transforms.get(targetEntity).position, leader);
        }
    }

    // Role-driven local work returns to the already authoritative semantic
    // location. Idle actors do not invent per-frame wandering.
    if (npc.semantic.activity == world::NpcActivity::Working && npc.location.district)
        return targeted(npc, id, CharacterAiBehavior::MoveTo,
                        locationPosition(state, npc.location));

    return {id, npc.activeEntity, CharacterAiBehavior::Idle, false, {}, {}};
}

CharacterAiEvent executeCharacterIntent(GameState& state, const CharacterAiIntent& intent) {
    core::require(physicalNpcValid(state, intent.npc) &&
                  state.world.npcs.get(intent.npc).activeEntity == intent.entity,
                  "Character intent actor mismatch");
    auto& transform = state.world.transforms.get(intent.entity);
    auto& body = state.world.characterBodies.get(intent.entity);
    bool moved = false;
    bool reached = !intent.hasTarget;
    if (!intent.hasTarget) {
        core::require(intent.behavior == CharacterAiBehavior::Idle,
                      "Non-idle character intent requires target");
        // Facing is reconstructible physical state. Canonical idle facing makes
        // abstraction/save/load independent from discarded body caches.
        body.yaw = 0.0F;
    } else {
        core::require(intent.behavior != CharacterAiBehavior::Idle,
                      "Idle character intent cannot have target");
        const auto from = world::toGlobal(transform.position, state.world.spatial.cellSize());
        const auto to = world::toGlobal(intent.target, state.world.spatial.cellSize());
        const double dx = to.x - from.x;
        const double dz = to.z - from.z;
        const double distance = std::hypot(dx, dz);
        core::require(std::isfinite(distance), "Non-finite character intent target");
        if (distance > 0.0) body.yaw = static_cast<float>(std::atan2(dx, dz));
        reached = distance <= characterArrivalRadiusMeters;
        if (!reached) {
            const double seconds = static_cast<double>(core::SimulationClock::tickDuration.count()) / 1000.0;
            const double step = std::min(distance, static_cast<double>(body.movementSpeed) * seconds);
            const auto before = transform.position;
            transform.position = world::moveCharacter(state.world, before,
                static_cast<float>(dx / distance * step),
                static_cast<float>(dz / distance * step));
            moved = transform.position != before;
            state.world.spatial.upsert(intent.entity, transform.position);
            const auto remaining = std::sqrt(world::distanceSquared(
                transform.position, intent.target, state.world.spatial.cellSize()));
            reached = remaining <= characterArrivalRadiusMeters;
        }
    }
    return {intent, moved, reached};
}

void updateCharacterAi(GameState& state, CharacterAiEventBuffer& events) {
    events.clear();
    // Player residency is kept in NpcId order and is the bounded ACTIVE set.
    for (const auto& resident : state.player.residents) {
        core::require(physicalNpcValid(state, resident.npc) &&
                      state.world.npcs.get(resident.npc).activeEntity == resident.entity,
                      "Character AI residency/body mismatch");
        const auto intent = decideCharacterIntent(state, resident.npc);
        events.push(executeCharacterIntent(state, intent));
    }
}

} // namespace faultline::simulation
