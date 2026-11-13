#pragma once
#include "world/state/actor_types.hpp"
#include <array>
#include <cstdint>

namespace faultline::world {
// V1 implements cars only. The category is explicit so motorcycles, boats and
// aircraft can be added without changing persistent vehicle identity or links.
enum class VehicleCategory : std::uint8_t { Car, Motorcycle, Boat, Aircraft };
enum class VehicleArchetype : std::uint8_t { Sedan, Hatchback, PoliceCruiser };
enum class VehicleOwnerKind : std::uint8_t { Unowned, Npc, Faction, Police };

struct VehicleOwnership {
    VehicleOwnerKind kind{};
    NpcId npc;
    FactionId faction;
    auto operator<=>(const VehicleOwnership&) const = default;
};
struct VehicleCondition {
    Health health{};
    auto operator<=>(const VehicleCondition&) const = default;
};
// Physical components exist only while streaming keeps the vehicle ACTIVE.
struct VehicleBody {
    float yaw{};
    float halfLength{2.2F};
    float halfWidth{0.9F};
    auto operator<=>(const VehicleBody&) const = default;
};
struct VehiclePhysics {
    float speed{};
    float acceleration{};
    float handling{};
    auto operator<=>(const VehiclePhysics&) const = default;
};
struct VehicleDefinition {
    VehicleCategory category{};
    VehicleArchetype archetype{};
    float halfLength{2.2F};
    float halfWidth{0.9F};
    float maxHealth{100.0F};
    float maxSpeed{22.0F};
    float reverseSpeed{6.0F};
    float acceleration{6.0F};
    float braking{10.0F};
    float rollingResistance{1.5F};
    float steeringRate{1.6F};
    float steeringFullSpeed{8.0F};
    float collisionRadius{1.0F};
    std::uint8_t passengerCapacity{4};
};
inline constexpr std::array<VehicleDefinition,3> vehicleDefinitions{{
    {VehicleCategory::Car, VehicleArchetype::Sedan, 2.3F, 0.9F, 100.0F, 22.0F, 6.0F, 6.0F, 10.0F, 1.5F, 1.6F, 8.0F, 1.05F, 4},
    {VehicleCategory::Car, VehicleArchetype::Hatchback, 1.9F, 0.85F, 85.0F, 20.0F, 5.5F, 5.5F, 9.0F, 1.6F, 1.8F, 7.5F, 0.95F, 4},
    {VehicleCategory::Car, VehicleArchetype::PoliceCruiser, 2.4F, 0.95F, 130.0F, 28.0F, 7.0F, 8.0F, 12.0F, 1.4F, 1.7F, 9.0F, 1.10F, 4},
}};
[[nodiscard]] inline bool validVehicleCategory(VehicleCategory category) {
    return static_cast<std::uint8_t>(category) <= static_cast<std::uint8_t>(VehicleCategory::Aircraft);
}
[[nodiscard]] inline bool validVehicleArchetype(VehicleArchetype archetype) {
    return static_cast<std::uint8_t>(archetype) < vehicleDefinitions.size();
}
[[nodiscard]] inline const VehicleDefinition& vehicleDefinition(VehicleArchetype archetype) {
    core::require(validVehicleArchetype(archetype), "Unknown vehicle archetype");
    return vehicleDefinitions[static_cast<std::size_t>(archetype)];
}
[[nodiscard]] inline bool validVehicleOwnerKind(VehicleOwnerKind kind) {
    return static_cast<std::uint8_t>(kind) <= static_cast<std::uint8_t>(VehicleOwnerKind::Police);
}

// Persistent identity is the VehiclePersistentId store key, not this entity.
// position/yaw/speed/condition are the abstract state used while no body exists.
struct VehicleState {
    EntityId activeEntity;
    VehicleCategory category{};
    VehicleArchetype archetype{};
    VehicleOwnership ownership;
    VehicleCondition condition;
    WorldLocation location;
    WorldPosition position;
    float yaw{};
    float speed{};
    std::uint8_t passengerCapacity{4};
    auto operator<=>(const VehicleState&) const = default;
};
}
