#pragma once
#include "persistence/schema_v3.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(TerritoryRules,v.maximumPresence,v.minimumControlPresence,v.contestThreshold,v.gainMargin,v.holdMargin,
    v.garrisonUnit,v.initialEntrenchment,v.homeEntrenchment,v.attackPressureDivisor,v.defenseSurvivorUnit)
FAULTLINE_SAVE_FIELDS(TerritoryStatistics,v.transitions,v.controlChanges,v.contestedTransitions,v.uncontrolledTransitions)
#undef FAULTLINE_SAVE_FIELDS
}
