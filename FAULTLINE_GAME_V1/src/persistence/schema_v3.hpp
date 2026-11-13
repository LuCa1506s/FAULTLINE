#pragma once
#include "persistence/schema_v2.hpp"
namespace faultline::persistence::detail {
// Sections 1..11 retain their historical field order. Only section 12 adds NPC runtime.
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(NpcTravelState,v.origin,v.destination,v.departureTick,v.arrivalTick)
FAULTLINE_SAVE_FIELDS(NpcSemanticState,v.activity,v.home,v.travel,v.nextUpdateTick,v.operation)
FAULTLINE_SAVE_FIELDS(NpcSimulationConfig,v.updateIntervalTicks,v.minimumTravelTicks,v.metersPerGameMinute)
FAULTLINE_SAVE_FIELDS(NpcStatistics,v.updates,v.travelsStarted,v.travelsCompleted,v.travelsCancelled,v.participations,v.relationshipChanges,v.promotions)
#undef FAULTLINE_SAVE_FIELDS
}
