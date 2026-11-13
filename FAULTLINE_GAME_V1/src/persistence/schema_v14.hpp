#pragma once
#include "persistence/schema_v13.hpp"
#include "simulation/vehicles/vehicle_state.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
// VehicleState itself keeps its historical v1 wire fields (v.activeEntity) in the
// Vehicles section. V14 semantic vehicle records live in VehicleSimulation and are
// encoded explicitly in save.cpp so old sections remain byte-identical.
FAULTLINE_SAVE_FIELDS(world::VehicleOwnership, v.kind,v.npc,v.faction)
FAULTLINE_SAVE_FIELDS(world::VehicleCondition, v.health)
FAULTLINE_SAVE_FIELDS(simulation::VehicleRules, v.civilianCarsPerDistrict,
    v.policeCarsPerDistrict,v.enterRangeMeters,v.exitOffsetMeters,v.laneOffsetMeters,
    v.collisionDamagePerSpeed,v.collisionMinimumDamage)
FAULTLINE_SAVE_FIELDS(simulation::VehicleResidency, v.vehicle,v.entity)
#undef FAULTLINE_SAVE_FIELDS
}
