#include "simulation/runtime/game.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/market/market_system.hpp"
#include "simulation/history/history_system.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace faultline {
GameState::GameState(WorldSeed seed, world::WorldGenerationConfig config, GeneratorVersion version)
    : metadata{seed, version}, randomness(seed) {
    world.base = world::generateWorld({seed, version, config});
    for (const auto& district : world.base->districts) {
        const auto id = world.districts.create({district.macroZone});
        core::require(id == district.id, "Generated district ID order mismatch");
    }
    for (const auto& territory : world.base->territories) {
        const auto id = world.territories.create({territory.district});
        core::require(id == territory.id, "Generated territory ID order mismatch");
    }
    for (const auto& faction : world.base->factions) {
        const auto id=world.factions.create(faction.state); core::require(id==faction.id,"Generated faction ID order mismatch");
    }
    for (const auto& npc : world.base->majorNpcs) {
        const auto id=world.npcs.create(npc.state); core::require(id==npc.id,"Generated NPC ID order mismatch");
    }
    for (const auto& assignment : world.base->territoryAssignments) {
        auto& territory=world.territories.get(assignment.territory);
        territory.owner=assignment.owner; territory.residentMembers=assignment.residentMembers;
    }
    world.factionRelationships=world.base->factionRelationships; world.npcRelationships=world.base->npcRelationships;
    simulation::initializeFactionSimulation(*this);
    simulation::initializeNpcSimulation(*this);
    simulation::initializeTerritorySimulation(*this);
    simulation::initializeMarketSimulation(*this);
}
GameState::GameState(WorldMetadata meta, core::SimulationClock time, RuntimeRandomState random, world::WorldState restored)
    : metadata(meta), clock(time), randomness(std::move(random)), world(std::move(restored)) {
    simulation::initializeFactionSimulation(*this);
    simulation::initializeNpcSimulation(*this);
    simulation::initializeTerritorySimulation(*this);
    simulation::initializeMarketSimulation(*this);
}
void validateState(const GameState& state) {
    world::validateWorld(state.world);
    simulation::validateTerritorySimulation(state);
    simulation::validateFactionSimulation(state);
    simulation::validateNpcSimulation(state);
    simulation::validateMarketSimulation(state);
    simulation::validateStrategicHistory(state);
    if (state.world.base) core::require(state.world.base->metadata.seed == state.metadata.seed &&
        state.world.base->metadata.generatorVersion == state.metadata.generatorVersion, "Base world metadata mismatch");
    core::require(state.metadata.generatorVersion.value >= 1 && state.metadata.generatorVersion.value <= 3, "Unsupported generator version");
    for (const auto& rng : state.randomness.streams) {
        const auto words = rng.state();
        core::require((words[0] | words[1] | words[2] | words[3]) != 0, "Invalid runtime RNG");
    }
}
RuntimeRandomState::RuntimeRandomState(WorldSeed seed) : streams{
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.combat")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.npc")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.missions")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.factions")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.market")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.police")},
    core::DeterministicRng{core::deriveSeed(seed.value, "runtime.story")}} {}

std::uint64_t stateHash(const GameState& state) {
    core::StateHash hash;
    hash.string("FAULTLINE.state.v9"); hash.u64(state.metadata.seed.value);
    hash.u64(state.metadata.generatorVersion.value); hash.u64(state.clock.tick());
    hash.u64(state.clock.gameTime().milliseconds);
    for (const auto& stream : state.randomness.streams)
        for (auto word : stream.state()) hash.u64(word);
    world::appendStateHash(hash, world::persistentSnapshot(state.world));
    simulation::appendFactionSimulationHash(hash,state);
    simulation::appendNpcSimulationHash(hash,state);
    simulation::appendTerritorySimulationHash(hash,state);
    simulation::appendMarketSimulationHash(hash,state);
    simulation::appendStrategicHistoryHash(hash,state);
    return hash.value();
}
void CommandQueue::submit(std::uint64_t targetTick, CommandSource source) {
    if (nextSequence_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Command sequence exhausted");
    pending_.push_back({targetTick, nextSequence_++, source});
}
std::vector<GameCommand> CommandQueue::take(std::uint64_t tick) {
    for (const auto& command : pending_)
        core::require(command.targetTick >= tick, "Stale command in queue");
    std::vector<GameCommand> result;
    for (const auto& command : pending_) if (command.targetTick == tick) result.push_back(command);
    std::erase_if(pending_, [tick](const auto& command) { return command.targetTick == tick; });
    return result;
}
void Simulation::tick(GameState& state, const TickInput& input) {
    core::require(!state.history.version || state.history.months==simulation::historyMonths,"Finish strategic history before runtime simulation");
    core::require(input.tick == state.clock.tick(), "Tick input mismatch");
    core::require(state.clock.tick() < core::SimulationClock::maxTick, "Tick counter exhausted");
    core::require(std::isfinite(input.player.moveX) && std::isfinite(input.player.moveZ) &&
        std::isfinite(input.player.aimX) && std::isfinite(input.player.aimZ), "Non-finite player input");
    for (std::size_t i = 0; i < input.commands.size(); ++i) {
        core::require(input.commands[i].targetTick == input.tick, "Command target mismatch");
        core::require(i == 0 || input.commands[i-1].sequence < input.commands[i].sequence,
                      "Command sequence must be strictly increasing");
    }
    events_.clear(); structuralChanges_.clear(); factionEvents_.clear(); npcEvents_.clear(); territoryEvents_.clear();
    SimulationContext context{input.tick, events_, structuralChanges_, state.randomness};
    // Ingest diagnostic commands. Domain systems enter in the canonical pipeline order.
    for (const auto& command : input.commands) {
        validateState(state);
        context.events.push({context.tick, command.sequence});
    }
    simulation::updateFactionOperations(state,factionEvents_);
    simulation::applyTerritoryConsequences(state,factionEvents_.values(),territoryEvents_);
    simulation::reconcileFactionOperations(state,factionEvents_);
    simulation::updateMarkets(state);
    simulation::evaluateFactions(state,factionEvents_);
    simulation::updateNpcs(state,npcEvents_,!factionEvents_.empty());
    context.structuralChanges.commit([&state](const auto& command) { world::applyStructuralCommand(state.world, command); });
    state.clock.advance();
}
void Simulation::advanceHeadless(GameState& state, std::uint64_t ticks,
    const std::function<void(const simulation::FactionOperationEvent&)>& observe) {
    core::require(!state.history.version || state.history.months==simulation::historyMonths,"Finish strategic history before runtime simulation");
    core::require(state.world.entities.entries().empty(),"Coarse headless advancement requires no physical entities");
    core::require(ticks<=core::SimulationClock::maxTick-state.clock.tick(),"Headless duration overflow");
    const auto end=state.clock.tick()+ticks;
    while (state.clock.tick()<end) {
        const auto due=std::min({simulation::nextFactionDueTick(state),simulation::nextNpcDueTick(state),state.marketSimulation.nextUpdateTick});
        if (state.clock.tick()<due) {
            // Only idle fixed ticks are skipped. Every scheduled mutation still uses tick().
            state.clock=core::SimulationClock{std::min(end,due)};
            events_.clear(); factionEvents_.clear(); npcEvents_.clear(); territoryEvents_.clear();
        }
        if (state.clock.tick()<end) {
            tick(state,{state.clock.tick(),{},{}});
            if (observe) for (const auto& event : factionEvents_.values()) observe(event);
        }
    }
}
void Game::requestValidation(std::uint64_t targetTick, CommandSource source) {
    core::require(targetTick >= state_.clock.tick(), "Cannot submit intent in the past");
    commands_.submit(targetTick, source);
}
void Game::tick() {
    simulation_.tick(state_, {state_.clock.tick(), input_, commands_.take(state_.clock.tick())});
    input_.shootPressed = false; input_.interactPressed = false; input_.enterVehiclePressed = false;
}
}
