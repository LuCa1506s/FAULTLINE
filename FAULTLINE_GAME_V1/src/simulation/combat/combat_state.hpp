#pragma once

#include "world/state/stores.hpp"
#include <cstdint>
#include <map>

namespace faultline::simulation {

inline constexpr std::uint32_t currentCombatStateVersion = 1;

enum class WeaponArchetype : std::uint8_t { Pistol, Shotgun, Smg };

struct WeaponDefinition {
    WeaponDefinitionId id;
    WeaponArchetype archetype{};
    std::uint16_t damage{};
    float rangeMeters{};
    std::uint16_t magazineCapacity{};
    std::uint64_t cooldownTicks{};
    std::uint64_t reloadTicks{};
    bool automatic{};
};

struct WeaponInstance {
    WeaponDefinitionId definition;
    std::uint16_t ammoInMagazine{};
    std::uint16_t reserveAmmo{};
    auto operator<=>(const WeaponInstance&) const = default;
};

struct CombatantState {
    WeaponInstanceId equipped;
    std::uint64_t nextAttackTick{};
    std::uint64_t reloadCompleteTick{};
    bool reloading{};
    auto operator<=>(const CombatantState&) const = default;
};

struct CombatStatistics {
    std::uint64_t attempts{}, shots{}, hits{}, misses{}, damageEvents{}, deaths{};
    auto operator<=>(const CombatStatistics&) const = default;
};

struct CombatSimulationState {
    std::uint32_t version{currentCombatStateVersion};
    world::DomainStore<WeaponInstanceId, WeaponInstance> weapons;
    CombatantState player;
    std::map<NpcId, CombatantState> npcs;
    CombatStatistics statistics;
};

} // namespace faultline::simulation
