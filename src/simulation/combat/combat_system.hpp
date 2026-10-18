#pragma once

#include "simulation/runtime/game_state.hpp"
#include "simulation/npc/character_ai.hpp"
#include "simulation/npc/npc_system.hpp"
#include "core/events/ordered_buffer.hpp"
#include <optional>

namespace faultline { struct PlayerInputFrame; }
namespace faultline::simulation {

enum class CombatActorKind : std::uint8_t { Player, Npc };
struct CombatActorRef {
    CombatActorKind kind{};
    EntityId entity;
    NpcId npc;
    auto operator<=>(const CombatActorRef&) const = default;
};
struct CombatIntent { CombatActorRef attacker, target; };

enum class CombatResult : std::uint8_t {
    Hit, MissOutOfRange, MissBlocked, InvalidAttacker, InvalidTarget,
    SelfTarget, DeadTarget, Cooldown, Reloading, EmptyMagazine
};
struct CombatEvent {
    CombatIntent intent;
    CombatResult result{};
    WeaponInstanceId weapon;
    std::uint16_t damage{};
    bool death{};
    std::uint64_t tick{};
};
using CombatEventBuffer = core::OrderedBuffer<CombatEvent>;

[[nodiscard]] const WeaponDefinition& weaponDefinition(WeaponDefinitionId id);
[[nodiscard]] WeaponDefinitionId weaponDefinitionId(WeaponArchetype archetype);
void initializeCombatSimulation(GameState& state);
void updateCombat(GameState& state, const PlayerInputFrame& input,
                  const CharacterAiEventBuffer& aiEvents, CombatEventBuffer& events,
                  NpcEventBuffer& npcEvents);
[[nodiscard]] CombatEvent resolveCombatIntent(GameState& state, const CombatIntent& intent,
                                              NpcEventBuffer& npcEvents);
void requestReload(GameState& state, CombatActorRef actor);
void validateCombatSimulation(const GameState& state);
void appendCombatSimulationHash(core::StateHash& hash, const GameState& state);

} // namespace faultline::simulation
