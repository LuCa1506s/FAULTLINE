#pragma once
#include "persistence/schema_v11.hpp"
#include "simulation/combat/combat_state.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(simulation::WeaponInstance, v.definition,v.ammoInMagazine,v.reserveAmmo)
FAULTLINE_SAVE_FIELDS(simulation::CombatantState, v.equipped,v.nextAttackTick,v.reloadCompleteTick,v.reloading)
FAULTLINE_SAVE_FIELDS(simulation::CombatStatistics, v.attempts,v.shots,v.hits,v.misses,v.damageEvents,v.deaths)
#undef FAULTLINE_SAVE_FIELDS
}
