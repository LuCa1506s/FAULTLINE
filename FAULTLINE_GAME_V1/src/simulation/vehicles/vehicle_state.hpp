#pragma once
#include "world/state/vehicle_types.hpp"
#include <cstdint>
#include <vector>

namespace faultline::simulation {
inline constexpr std::uint32_t currentVehicleSimulationVersion = 1;

// V1 deliberately keeps one shared car profile family. The semantic category,
// archetype and definition table are explicit so future categories do not change
// persistent identity, links, or save ownership records.
struct VehicleRules {
    std::uint32_t civilianCarsPerDistrict{2};
    std::uint32_t policeCarsPerDistrict{1};
    float enterRangeMeters{3.0F};
    float exitOffsetMeters{2.4F};
    float laneOffsetMeters{1.5F};
    float collisionDamagePerSpeed{2.0F};
    float collisionMinimumDamage{1.0F};
    auto operator<=>(const VehicleRules&) const = default;
};

// Logical residency preserves the already allocated physical handle across saves.
// Bodies/components/spatial buckets are rebuilt from VehicleState.
struct VehicleResidency {
    VehiclePersistentId vehicle;
    EntityId entity;
    auto operator<=>(const VehicleResidency&) const = default;
};

struct VehicleStatistics {
    std::uint64_t spawned{}, materialized{}, abstracted{};
    std::uint64_t entered{}, exited{}, collisions{};
    auto operator<=>(const VehicleStatistics&) const = default;
};

struct VehicleSimulationState {
    std::uint32_t version{currentVehicleSimulationVersion};
    VehicleRules rules;
    VehiclePersistentId drivenVehicle;
    std::vector<VehicleResidency> residents;
    // Non-authoritative process diagnostics: deliberately excluded from save/hash.
    VehicleStatistics statistics;
};

} // namespace faultline::simulation
