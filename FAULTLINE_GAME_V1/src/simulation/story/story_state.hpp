#pragma once
#include "world/state/stores.hpp"
#include <vector>

namespace faultline::simulation {
inline constexpr std::uint32_t currentStoryGraphVersion=1;
enum class StoryKind : std::uint8_t { Contact, BossOperation, Succession, Betrayal, DealerConflict, MarketChange, TerritorialConflict, Alliance };
enum class StoryOutcome : std::uint8_t { Unresolved, Succeeded, Failed };
enum class StoryAvailability : std::uint8_t { Locked, Eligible, Invalidated, Closed };
struct StoryPrerequisite {
    StoryNodeId node;
    StoryOutcome outcome{StoryOutcome::Succeeded};
    auto operator<=>(const StoryPrerequisite&) const = default;
};
// High-level possibilities, never per-mission objectives or ECS entities.
struct StoryNode {
    StoryKind kind{};
    FactionId faction, other;
    NpcId originalContact, contact;
    TerritoryId territory;
    DistrictId district;
    StoryOutcome outcome{};
    std::vector<StoryPrerequisite> prerequisites;
    // One-based index into immutable compressed history; zero means no source.
    std::uint32_t historyRecord{};
    auto operator<=>(const StoryNode&) const = default;
};
struct StoryGraph {
    // Zero means not initialized. Migration never invents a prologue/story.
    std::uint32_t version{};
    world::DomainStore<StoryNodeId,StoryNode> nodes;
};
}
