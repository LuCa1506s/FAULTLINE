#pragma once
#include "core/id/id.hpp"
#include "world/geometry/rect.hpp"
#include <array>
#include <vector>

namespace faultline::world {
enum class FrontageSide : std::uint8_t { North, East, South, West }; // North is -Z.
enum class BuildingArchetype : std::uint8_t { House, Apartment, Store, Office, Warehouse, Industrial, Garage };
using BuildingWeights = std::array<std::uint32_t, 7>;
struct UrbanGenerationConfig {
    std::array<std::int32_t, 4> roadHalfWidths{12, 8, 5, 3}; // Arterial, major, local, service.
    std::int32_t minParcelSpan{12}, maxParcelDepth{64}, targetParcelFrontage{48}, buildingSetback{2};
    std::uint64_t minTerritoryArea{64000}, targetTerritoryArea{1200000};
    std::int32_t minTerritorySpan{64};
    std::uint32_t maxTerritoriesPerDistrict{6}, maxParcels{100000};
    BuildingWeights residential{45, 35, 10, 0, 0, 0, 10};
    BuildingWeights commercial{5, 20, 30, 35, 0, 0, 10};
    BuildingWeights industrial{0, 0, 10, 0, 55, 30, 5};
    BuildingWeights park{30, 0, 40, 0, 0, 0, 30};
};
struct RoadFrontage {
    RoadEdgeId road;
    FrontageSide side{};
    std::int32_t start{}, length{}; // Interval on the road's longitudinal axis.
    auto operator<=>(const RoadFrontage&) const = default;
};
struct Parcel {
    ParcelId id; DistrictId district; Rect boundary;
    std::uint64_t areaSquareMeters{};
    RoadFrontage frontage;
    bool buildable{true};
    auto operator<=>(const Parcel&) const = default;
};
enum class BuildingFlags : std::uint8_t { NoInterior = 1 };
struct BuildingTransform {
    std::int64_t centerX2{}, centerZ2{}; // Half-meter units, exact even for odd footprints.
    FrontageSide front{};
    auto operator<=>(const BuildingTransform&) const = default;
};
struct BuildingState {
    BuildingId id; DistrictId district; ParcelId parcel;
    BuildingArchetype archetype{};
    Rect footprint;
    BuildingTransform transform;
    BuildingFlags flags{BuildingFlags::NoInterior};
    auto operator<=>(const BuildingState&) const = default;
};
struct GeneratedTerritory {
    TerritoryId id; DistrictId district; Rect boundary;
    std::uint32_t buildingCount{};
    std::uint64_t buildableArea{};
    auto operator<=>(const GeneratedTerritory&) const = default;
};
struct ParcelGenerationResult { std::vector<Parcel> parcels; };
struct BuildingGenerationResult { std::vector<BuildingState> buildings; };
struct TerritoryGenerationResult { std::vector<GeneratedTerritory> territories; };
}
