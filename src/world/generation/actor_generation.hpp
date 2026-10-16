#pragma once
#include "world/generation/world_generator.hpp"
#include <map>
#include <span>

namespace faultline::world {
using TerritoryAdjacency = std::map<TerritoryId,std::vector<TerritoryId>>;
[[nodiscard]] TerritoryAdjacency territoryAdjacency(std::span<const GeneratedTerritory> territories);
[[nodiscard]] FactionGenerationResult generateFactions(const GenerationContext& context, const GeneratedWorld& world);
[[nodiscard]] MajorNpcGenerationResult generateMajorNpcs(const GenerationContext& context, const GeneratedWorld& world,
    std::span<const GeneratedFaction> factions);
[[nodiscard]] RelationshipGenerationResult generateInitialRelationships(const GenerationContext& context, const GeneratedWorld& world,
    std::span<const GeneratedFaction> factions, std::span<const TerritoryAssignment> assignments, std::span<const GeneratedNpc> npcs);
[[nodiscard]] ValidationReport validateActors(const GeneratedWorld& world);
[[nodiscard]] ValidationReport validateFactionViability(const GeneratedWorld& world, FactionId faction);
[[nodiscard]] std::uint64_t factionManpower(const GeneratedWorld& world, FactionId faction);
void appendNpcHash(core::StateHash& hash, const NpcState& npc);
void appendFactionHash(core::StateHash& hash, const FactionState& faction);
void appendRelationshipHash(core::StateHash& hash, std::span<const FactionRelationship> factions, std::span<const NpcRelationship> npcs);
void appendActorsHash(core::StateHash& hash, const GeneratedWorld& world);
}
