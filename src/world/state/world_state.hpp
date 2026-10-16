#pragma once
#include "world/state/stores.hpp"
#include "core/rng/stable_hash.hpp"
#include "world/spatial/spatial_grid.hpp"
#include "world/generation/world_generator.hpp"
#include <optional>
#include <variant>

namespace faultline::world {
enum class MissionResolution : std::uint8_t { Available, Accepted, Active, Completed, Failed, Expired, Invalidated, ResolvedByNpc };
struct Transform { WorldPosition position; };
struct VehicleState { EntityId activeEntity; };
struct MissionState { MissionResolution status{}; NpcId issuer; };
struct DistrictState { MacroZoneId macroZone; };
struct NpcLink { NpcId npc; };
struct VehicleLink { VehiclePersistentId vehicle; };
using NpcStore = DomainStore<NpcId, NpcState>;
using FactionStore = DomainStore<FactionId, FactionState>;
using TerritoryStore = DomainStore<TerritoryId, TerritoryState>;
using VehicleStore = DomainStore<VehiclePersistentId, VehicleState>;
using MissionStore = DomainStore<MissionId, MissionState>;
using DistrictStore = DomainStore<DistrictId, DistrictState>;
struct WorldState {
    std::optional<GeneratedWorld> base;
    EntityRegistry entities;
    ComponentStore<NpcLink> npcLinks;
    ComponentStore<VehicleLink> vehicleLinks;
    ComponentStore<Transform> transforms;
    ComponentStore<Health> health;
    SpatialGrid spatial;
    NpcStore npcs;
    FactionStore factions;
    TerritoryStore territories;
    VehicleStore vehicles;
    MissionStore missions;
    DistrictStore districts;
    std::vector<FactionRelationship> factionRelationships;
    std::vector<NpcRelationship> npcRelationships;
};
void destroyEntity(WorldState& world, EntityId id);
// Completed-tick semantic view: synchronize NPC bodies, then discard their representation.
// Allocator high-water marks survive abstraction. Other data-only components remain authoritative.
[[nodiscard]] WorldState persistentSnapshot(const WorldState& world);
void validateWorld(const WorldState& world);
void appendStateHash(core::StateHash& hash, const WorldState& world);
struct CreateEntityCommand {};
struct DestroyEntityCommand { EntityId entity; };
using StructuralCommand = std::variant<CreateEntityCommand, DestroyEntityCommand>;
void applyStructuralCommand(WorldState& world, const StructuralCommand& command);
}
