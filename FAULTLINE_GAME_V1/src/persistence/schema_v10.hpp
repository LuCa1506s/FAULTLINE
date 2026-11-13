#pragma once
#include "persistence/schema_v9.hpp"
#include "simulation/prologue/prologue_state.hpp"

namespace faultline::persistence::detail {
template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::PrologueState>
void fields(A& a, T& v) {
    a(v.version, v.phase, v.firstDelivery, v.origin, v.giver, v.recipient, v.evidence);
}
}
