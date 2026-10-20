#pragma once
#include "persistence/schema_v12.hpp"
#include "simulation/police/police_state.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(simulation::CrimeActorRef,v.kind,v.npc)
FAULTLINE_SAVE_FIELDS(simulation::CrimeEvent,v.id,v.type,v.actor,v.victim,v.position,v.tick,v.severityBps,v.observability)
FAULTLINE_SAVE_FIELDS(simulation::WitnessObservation,v.crime,v.witness,v.tick)
FAULTLINE_SAVE_FIELDS(simulation::CrimeReport,v.id,v.crime,v.witness,v.observedTick,v.completionTick,v.status)
FAULTLINE_SAVE_FIELDS(simulation::PoliceKnowledge,v.crime,v.suspectIdentified,v.suspect,v.reportedPosition,v.knownTick,v.directObservation,v.deliveredReports)
FAULTLINE_SAVE_FIELDS(simulation::WantedContext,v.state,v.heatBps,v.suspectIdentified,v.lastKnownPosition,v.lastConfirmedTick,v.deadlineTick,v.confidenceRadiusMeters)
FAULTLINE_SAVE_FIELDS(simulation::PoliceResponder,v.officer,v.status,v.dispatchTick,v.arrivalTick,v.target)
FAULTLINE_SAVE_FIELDS(simulation::PoliceRules,v.reportDelayTicks,v.reportedTicks,v.lostContactTicks,v.escapedPendingTicks,v.searchTicks,v.cooldownTicks,v.suspicionTicks,v.responseTravelTicks,v.arrestAdvanceTicks,v.visualRangeMeters,v.audibleRangeMeters,v.initialSearchRadiusMeters,v.searchExpansionMeters,v.arrestHealthThreshold)
FAULTLINE_SAVE_FIELDS(simulation::ProcessedCombatFact,v.tick,v.eventIndex)
#undef FAULTLINE_SAVE_FIELDS
}
