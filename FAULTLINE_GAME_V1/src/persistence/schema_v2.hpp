#pragma once
#include "persistence/schema_v1.hpp"

namespace faultline::persistence::detail {
using namespace simulation;
// Sections 1..10 keep their v1 encoding, including the frozen generated baseline.
// FactionStrategicState belongs only to the new runtime section, not worldgen.
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(FactionStrategicState, v.confidence,v.evaluations,v.lastEvaluationTick,v.nextAttackTick,v.lastTarget,v.lastUtility)
FAULTLINE_SAVE_FIELDS(FactionSimulationConfig, v.evaluationIntervalTicks,v.preparationTicks,v.activeDurationTicks,
    v.defensePreparationTicks,v.attackCooldownTicks,v.minForce,v.maxForce,v.maxConcurrentOperations,
    v.attackBaseCost,v.defenseBaseCost,v.costPerMember,v.minimumAttackUtility)
FAULTLINE_SAVE_FIELDS(ResourceCommitment, v.manpower,v.paidCost)
FAULTLINE_SAVE_FIELDS(OperationResult, v.ownLosses,v.opposingLosses,v.garrisonLosses,v.strategicPressure)
FAULTLINE_SAVE_FIELDS(FactionOperation, v.faction,v.opponent,v.type,v.status,v.target,v.threat,v.commitment,
    v.createdTick,v.activeTick,v.resolutionTick,v.finishedTick,v.result)
#undef FAULTLINE_SAVE_FIELDS
}
