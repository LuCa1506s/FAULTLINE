#pragma once
#include "persistence/binary.hpp"

namespace faultline::persistence::detail {
using namespace world;
// Explicit field lists are the wire schema, independent of C++ padding/layout.
// The local macro only removes identical const/mutable visitor boilerplate.
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(WorldSeed, v.value)
FAULTLINE_SAVE_FIELDS(GeneratorVersion, v.value)
FAULTLINE_SAVE_FIELDS(SaveFormatVersion, v.value)
FAULTLINE_SAVE_FIELDS(GameVersion, v.major,v.minor,v.patch)
FAULTLINE_SAVE_FIELDS(SaveHeader, v.saveFormatVersion,v.gameVersion,v.generatorVersion,v.worldSeed)
FAULTLINE_SAVE_FIELDS(WorldMetadata, v.seed,v.generatorVersion)
FAULTLINE_SAVE_FIELDS(Rect, v.x,v.z,v.width,v.depth)
FAULTLINE_SAVE_FIELDS(CellCoord, v.x,v.z)
FAULTLINE_SAVE_FIELDS(LocalPosition, v.x,v.y,v.z)
FAULTLINE_SAVE_FIELDS(WorldPosition, v.cell,v.local)
FAULTLINE_SAVE_FIELDS(Health, v.current,v.maximum)
FAULTLINE_SAVE_FIELDS(Transform, v.position)
FAULTLINE_SAVE_FIELDS(NpcLink, v.npc)
FAULTLINE_SAVE_FIELDS(VehicleLink, v.vehicle)
FAULTLINE_SAVE_FIELDS(VehicleState, v.activeEntity)
FAULTLINE_SAVE_FIELDS(MissionState, v.status,v.issuer)
FAULTLINE_SAVE_FIELDS(DistrictState, v.macroZone)
FAULTLINE_SAVE_FIELDS(TerritoryState, v.district,v.owner,v.residentMembers)
FAULTLINE_SAVE_FIELDS(NpcRoles, v.bits)
FAULTLINE_SAVE_FIELDS(NpcTraits, v.aggression,v.ambition,v.riskTolerance,v.loyaltyBias,v.greed,v.courage)
FAULTLINE_SAVE_FIELDS(NpcCapabilities, v.combat,v.driving,v.leadership,v.streetKnowledge,v.business)
FAULTLINE_SAVE_FIELDS(NpcIdentity, v.name,v.age,v.appearanceSeed)
FAULTLINE_SAVE_FIELDS(WorldLocation, v.district,v.territory,v.building)
FAULTLINE_SAVE_FIELDS(NpcState, v.lifecycle,v.faction,v.location,v.activeEntity,v.position,v.health,v.identity,
    v.roles,v.authority,v.respect,v.loyalty,v.traits,v.capabilities,v.major)
FAULTLINE_SAVE_FIELDS(FactionPersonality, v.aggression,v.riskTolerance,v.marketFocus,v.territorialFocus)
FAULTLINE_SAVE_FIELDS(FactionState, v.lifecycle,v.leader,v.homeTerritory,v.name,v.family,v.personality,
    v.treasury,v.reserveMembers,v.cohesion)
FAULTLINE_SAVE_FIELDS(GeneratedFaction, v.id,v.state)
FAULTLINE_SAVE_FIELDS(GeneratedNpc, v.id,v.state)
FAULTLINE_SAVE_FIELDS(TerritoryAssignment, v.territory,v.owner,v.residentMembers)
FAULTLINE_SAVE_FIELDS(FactionRelationship, v.from,v.to,v.trust,v.hostility,v.fear,v.sharedInterest,v.historicalGrievance,v.context)
FAULTLINE_SAVE_FIELDS(NpcRelationship, v.from,v.to,v.trust,v.fear,v.hostility,v.personalLoyalty,v.debt,v.context)
FAULTLINE_SAVE_FIELDS(UrbanGenerationConfig, v.roadHalfWidths,v.minParcelSpan,v.maxParcelDepth,v.targetParcelFrontage,
    v.buildingSetback,v.minTerritoryArea,v.targetTerritoryArea,v.minTerritorySpan,v.maxTerritoriesPerDistrict,
    v.maxParcels,v.residential,v.commercial,v.industrial,v.park)
FAULTLINE_SAVE_FIELDS(WorldGenerationConfig, v.width,v.depth,v.originX,v.originZ,v.minMacroZones,v.maxMacroZones,v.waterPercent,v.urban)
FAULTLINE_SAVE_FIELDS(WaterRegion, v.id,v.boundary)
FAULTLINE_SAVE_FIELDS(MacroZone, v.id,v.boundary,v.archetype)
FAULTLINE_SAVE_FIELDS(ZoneAdjacency, v.a,v.b)
FAULTLINE_SAVE_FIELDS(RoadNode, v.id,v.x,v.z)
FAULTLINE_SAVE_FIELDS(RoadEdge, v.id,v.a,v.b,v.category)
FAULTLINE_SAVE_FIELDS(RoadGraph, v.nodes,v.edges)
FAULTLINE_SAVE_FIELDS(GeneratedDistrict, v.id,v.macroZone,v.boundary,v.archetype,v.population,v.density,v.wealth)
FAULTLINE_SAVE_FIELDS(RoadFrontage, v.road,v.side,v.start,v.length)
FAULTLINE_SAVE_FIELDS(Parcel, v.id,v.district,v.boundary,v.areaSquareMeters,v.frontage,v.buildable)
FAULTLINE_SAVE_FIELDS(BuildingTransform, v.centerX2,v.centerZ2,v.front)
FAULTLINE_SAVE_FIELDS(BuildingState, v.id,v.district,v.parcel,v.archetype,v.footprint,v.transform,v.flags)
FAULTLINE_SAVE_FIELDS(GeneratedTerritory, v.id,v.district,v.boundary,v.buildingCount,v.buildableArea)
FAULTLINE_SAVE_FIELDS(GeneratedWorld, v.metadata,v.config,v.bounds,v.buildableLand,v.water,v.macroZones,v.adjacency,
    v.roads,v.districts,v.parcels,v.buildings,v.territories,v.factions,v.majorNpcs,v.territoryAssignments,
    v.factionRelationships,v.npcRelationships)
#undef FAULTLINE_SAVE_FIELDS
}
