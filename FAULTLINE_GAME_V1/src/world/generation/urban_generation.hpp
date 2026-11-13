#pragma once
#include "world/generation/world_generator.hpp"
#include <span>

namespace faultline::world {
struct RoadReservation {
    RoadEdgeId road; Rect boundary;
    bool vertical{};
    std::int32_t axis{}, start{}, end{};
    RoadCategory category{};
};
void validateUrbanConfig(const UrbanGenerationConfig& config);
[[nodiscard]] std::vector<RoadReservation> roadReservations(const GeneratedWorld& world);
[[nodiscard]] bool roadBoundaryCovers(bool vertical, std::int64_t axis, std::int64_t start, std::int64_t end,
    std::span<const RoadReservation> roads);
[[nodiscard]] ParcelGenerationResult generateParcels(const GenerationContext& context, const GeneratedWorld& topology);
[[nodiscard]] BuildingGenerationResult generateBuildings(const GenerationContext& context,
    std::span<const GeneratedDistrict> districts, std::span<const Parcel> parcels);
[[nodiscard]] TerritoryGenerationResult generateTerritories(const GenerationContext& context,
    const GeneratedWorld& topology, std::span<const Parcel> parcels, std::span<const BuildingState> buildings);
[[nodiscard]] ValidationReport validateUrbanWorld(const GeneratedWorld& world);
void appendUrbanHash(core::StateHash& hash, const GeneratedWorld& world);
[[nodiscard]] std::uint64_t parcelLayoutHash(std::span<const Parcel> parcels);
[[nodiscard]] std::uint64_t buildingLayoutHash(std::span<const BuildingState> buildings);
[[nodiscard]] std::uint64_t territoryLayoutHash(std::span<const GeneratedTerritory> territories);
}
