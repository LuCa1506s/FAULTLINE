#pragma once
#include "persistence/schema_v6.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(PrologueEvidence,v.origin,v.rival,v.witness,v.location,v.incident,v.deliveryTick,v.discoveryTick,v.firstDeliveryCompleted)
FAULTLINE_SAVE_FIELDS(HistoryEvent,v.kind,v.month,v.faction,v.other,v.npc,v.territory,v.cost)
FAULTLINE_SAVE_FIELDS(StrategicHistory,v.version,v.months,v.startTick,v.prologue,v.informed,v.events)
#undef FAULTLINE_SAVE_FIELDS
}
