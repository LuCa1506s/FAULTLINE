#pragma once

#include "simulation/runtime/game_state.hpp"
#include "core/events/ordered_buffer.hpp"
#include <vector>

namespace faultline::simulation {

// Transient physical execution intent. Persistent NpcState remains the authority
// for identity, activity, assignments and personality.
enum class CharacterAiBehavior : std::uint8_t {
    Idle,
    MoveTo,
    Follow,
    Flee,
    AttackIntent,
    Protect,
    PoliceSearch,
    PolicePursuit
};

struct CharacterAiIntent {
    NpcId npc;
    EntityId entity;
    CharacterAiBehavior behavior{};
    bool hasTarget{};
    world::WorldPosition target;
    NpcId targetNpc;
};

struct CharacterAiEvent {
    CharacterAiIntent intent;
    bool moved{};
    bool reached{};
};

using CharacterAiEventBuffer = core::OrderedBuffer<CharacterAiEvent>;

inline constexpr float npcMovementSpeedMetersPerSec = world::defaultNpcMovementSpeedMetersPerSec;
inline constexpr float npcStanceHeightMeters = world::defaultNpcStanceHeightMeters;
inline constexpr double characterPerceptionRadiusMeters = 30.0;
inline constexpr double characterArrivalRadiusMeters = 0.10;

[[nodiscard]] std::vector<EntityId> perceiveCharacters(
    const GameState& state, NpcId observer,
    double radius = characterPerceptionRadiusMeters);

[[nodiscard]] CharacterAiIntent decideCharacterIntent(
    const GameState& state, NpcId npc);

[[nodiscard]] CharacterAiEvent executeCharacterIntent(
    GameState& state, const CharacterAiIntent& intent);

// Executes only the ordered logical residency set. Streaming owns that set and
// physical materialization; this system never creates or destroys bodies.
void updateCharacterAi(GameState& state, CharacterAiEventBuffer& events);

} // namespace faultline::simulation
