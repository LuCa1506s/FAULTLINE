#pragma once
#include <compare>
#include <cstdint>
#include <functional>
#include <ostream>

namespace faultline {
struct OperationTag;
template<class Tag> class Id {
public:
    constexpr Id() = default;
    explicit constexpr Id(std::uint64_t value) : value_(value) {}
    [[nodiscard]] constexpr std::uint64_t value() const { return value_; }
    explicit constexpr operator bool() const { return value_ != 0; }
    auto operator<=>(const Id&) const = default;
    friend std::ostream& operator<<(std::ostream& out, Id id) { return out << id.value_; }
private:
    std::uint64_t value_{};
};
using OperationId = Id<OperationTag>;
struct StoryNodeTag; using StoryNodeId = Id<StoryNodeTag>;
struct EntityTag; using EntityId = Id<EntityTag>;
struct NpcTag; using NpcId = Id<NpcTag>;
struct FactionTag; using FactionId = Id<FactionTag>;
struct TerritoryTag; using TerritoryId = Id<TerritoryTag>;
struct DistrictTag; using DistrictId = Id<DistrictTag>;
struct MissionTag; using MissionId = Id<MissionTag>;
struct ObjectiveTag; using ObjectiveId = Id<ObjectiveTag>;
struct VehicleTag; using VehiclePersistentId = Id<VehicleTag>;
struct BuildingTag; using BuildingId = Id<BuildingTag>;
struct ParcelTag; using ParcelId = Id<ParcelTag>;
struct RoadEdgeTag; using RoadEdgeId = Id<RoadEdgeTag>;
struct MacroZoneTag; using MacroZoneId = Id<MacroZoneTag>;
}
template<class Tag> struct std::hash<faultline::Id<Tag>> {
    std::size_t operator()(faultline::Id<Tag> id) const noexcept {
        return static_cast<std::size_t>(id.value());
    }
};
