#pragma once
#include "persistence/schema_v10.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(world::CharacterBody, v.yaw, v.movementSpeed, v.stanceHeight)
FAULTLINE_SAVE_FIELDS(simulation::NpcResidency, v.npc, v.entity)
FAULTLINE_SAVE_FIELDS(simulation::PlayerState, v.version, v.controlledEntity, v.spawnPosition, v.yaw, v.residents)
#undef FAULTLINE_SAVE_FIELDS
}
