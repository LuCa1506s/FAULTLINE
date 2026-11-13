#include "simulation/runtime/game.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/npc/character_ai.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/market/market_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/story/story_system.hpp"
#include "simulation/missions/mission_system.hpp"
#include "simulation/prologue/prologue_system.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/combat/combat_system.hpp"
#include "simulation/police/police_system.hpp"
#include "simulation/vehicles/vehicle_system.hpp"
#include "world/spatial/streaming.hpp"
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
    simulation::initializeMissionSimulation(*this);
    simulation::initializePrologueState(*this);
    simulation::initializePlayerState(*this);
    simulation::initializeCombatSimulation(*this);
    simulation::initializePoliceSimulation(*this);
    simulation::initializeVehicleSimulation(*this);
    // Only fresh games own a generated fleet. Restored/legacy saves never invent
    // vehicles and never consume the vehicle spawn RNG stream.
    simulation::seedVehicleFleet(*this);
}
GameState::GameState(WorldMetadata meta, core::SimulationClock time, RuntimeRandomState random, world::WorldState restored)
    : metadata(meta), clock(time), randomness(std::move(random)), world(std::move(restored)) {
    simulation::initializeFactionSimulation(*this);
    simulation::initializeNpcSimulation(*this);
    simulation::initializeTerritorySimulation(*this);
    simulation::initializeMarketSimulation(*this);
    simulation::initializeMissionSimulation(*this);
    simulation::initializePrologueState(*this);
    simulation::initializePlayerState(*this);
    simulation::initializeCombatSimulation(*this);
    simulation::initializePoliceSimulation(*this);
    simulation::initializeVehicleSimulation(*this);
}
void validateState(const GameState& state) {
    world::validateWorld(state.world);
    simulation::validateTerritorySimulation(state);
    simulation::validateFactionSimulation(state);
    simulation::validateNpcSimulation(state);
    simulation::validateMarketSimulation(state);
    simulation::validateStrategicHistory(state);
    simulation::validateStoryGraph(state);
    simulation::validateMissionSimulation(state);
    simulation::validatePrologueState(state);
    simulation::validatePlayerState(state);
    simulation::validateCombatSimulation(state);
    simulation::validatePoliceSimulation(state);
    simulation::validateVehicleSimulation(state);
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
    hash.string("FAULTLINE.state.v16"); hash.u64(state.metadata.seed.value);
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
    simulation::appendStoryGraphHash(hash,state);
    simulation::appendMissionSimulationHash(hash,state);
    simulation::appendPrologueStateHash(hash,state);
    simulation::appendPlayerStateHash(hash,state);
    simulation::appendCombatSimulationHash(hash,state);
    simulation::appendPoliceSimulationHash(hash,state);
    simulation::appendVehicleSimulationHash(hash,state);
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
    core::require(state.prologue.phase!=simulation::ProloguePhase::DeliveryComplete,"Commit prologue delivery before runtime simulation");
    core::require(input.tick == state.clock.tick(), "Tick input mismatch");
    core::require(state.clock.tick() < core::SimulationClock::maxTick, "Tick counter exhausted");
    core::require(std::isfinite(input.player.moveX) && std::isfinite(input.player.moveZ) &&
        std::isfinite(input.player.aimX) && std::isfinite(input.player.aimZ), "Non-finite player input");
    for (std::size_t i = 0; i < input.commands.size(); ++i) {
        core::require(input.commands[i].targetTick == input.tick, "Command target mismatch");
        core::require(i == 0 || input.commands[i-1].sequence < input.commands[i].sequence,
                      "Command sequence must be strictly increasing");
    }
    // Restore logical streaming residency before NPC rules inspect materialization.
    // Reusing an existing allocation lease must not consume another EntityId.
    simulation::restorePlayerResidency(state);
    simulation::restoreVehicleResidency(state);
    events_.clear(); structuralChanges_.clear(); factionEvents_.clear(); npcEvents_.clear(); territoryEvents_.clear(); missionEvents_.clear(); policeEvents_.clear();
    SimulationContext context{input.tick, events_, structuralChanges_, state.randomness};
    // Ingest diagnostic commands. Domain systems enter in the canonical pipeline order.
    for (const auto& command : input.commands) {
        validateState(state);
        context.events.push({context.tick, command.sequence});
    }
    simulation::updateFactionOperations(state,factionEvents_);
    simulation::applyTerritoryConsequences(state,factionEvents_.values(),territoryEvents_);
    simulation::reconcileFactionOperations(state,factionEvents_);
    const auto previousMarketUpdate=state.marketSimulation.updates;
    simulation::updateMarkets(state);
    simulation::evaluateFactions(state,factionEvents_);
    simulation::updateNpcs(state,npcEvents_,!factionEvents_.empty());
    // Observe domain events for missions
    for (const auto& ev : territoryEvents_.values()) {
        simulation::onTerritoryControlChanged(state, ev.territory, missionEvents_);
    }
    for (const auto& ev : factionEvents_.values()) {
        if (ev.kind == simulation::OperationEventKind::Resolved) {
            simulation::onFactionOperationResolved(state, ev.operation, missionEvents_);
        }
    }
    for (const auto& ev : npcEvents_.values()) {
        if (ev.kind == simulation::NpcEventKind::Died) {
            simulation::onNpcDied(state, ev.npc, missionEvents_);
        }
    }
    simulation::updateMissions(state, missionEvents_);
    // Observe only scheduled semantic mutations; never drive foreign world rules.
    if (state.story.version && (!npcEvents_.empty() || !factionEvents_.empty() ||
        !territoryEvents_.empty() || previousMarketUpdate!=state.marketSimulation.updates)) simulation::repairStoryGraph(state);
    context.structuralChanges.commit([&state](const auto& command) { world::applyStructuralCommand(state.world, command); });
    simulation::updatePlayerMovement(state, input.player, streaming_);
    // Streaming has established the bounded physical residency set. Character AI
    // now executes local intent before the completed-tick clock boundary.
    simulation::updateCharacterAi(state, characterAiEvents_);
    // Existing report deadlines and police knowledge act before this tick's combat.
    // Response movement uses only persisted last-known facts or fresh officer LoS.
    simulation::updateReportsAndWanted(state,policeEvents_);
    simulation::updatePoliceResponse(state,characterAiEvents_,policeEvents_);
    const auto npcEventsBeforeCombat=npcEvents_.values().size();
    if (!simulation::arrestPending(state)) {
        simulation::updateCombat(state,input.player,characterAiEvents_,combatEvents_,npcEvents_);
    } else {
        combatEvents_.clear();
    }
    // Combat emits facts; crime/witness/report processing follows without recursive
    // writes back into CombatSystem. A newly reported response acts next tick.
    simulation::ingestCombatCrimes(state,combatEvents_,policeEvents_);
    simulation::observeNewCrimes(state,policeEvents_);
    simulation::updateReportsAndWanted(state,policeEvents_);
    // Physical combat consequences are observed after resolution. Downstream
    // systems consume facts; Combat never edits missions or StoryGraph directly.
    bool combatDeathObserved=false;
    for (std::size_t i=npcEventsBeforeCombat;i<npcEvents_.values().size();++i) {
        const auto& ev=npcEvents_.values()[i];
        if (ev.kind==simulation::NpcEventKind::Died) {
            combatDeathObserved=true; simulation::onNpcDied(state,ev.npc,missionEvents_);
        }
    }
    if (combatDeathObserved) {
        simulation::updateMissions(state,missionEvents_);
        if (state.story.version) simulation::repairStoryGraph(state);
    }
    playerIntents_.clear();
    if (input.player.interactPressed && state.player.controlledEntity)
        playerIntents_.push({state.player.controlledEntity,state.world.transforms.get(state.player.controlledEntity).position,
                            state.player.yaw,state.clock.tick()});
    state.clock.advance();
    if (simulation::arrestPending(state)) simulation::resolvePlayerArrest(state);
}
void Simulation::advanceHeadless(GameState& state, std::uint64_t ticks,
    const std::function<void(const simulation::FactionOperationEvent&)>& observe) {
    core::require(!state.history.version || state.history.months==simulation::historyMonths,"Finish strategic history before runtime simulation");
    core::require(state.world.entities.entries().empty(),"Coarse headless advancement requires no physical entities");
    core::require(ticks<=core::SimulationClock::maxTick-state.clock.tick(),"Headless duration overflow");
    const auto end=state.clock.tick()+ticks;
    while (state.clock.tick()<end) {
        const auto due=std::min({simulation::nextFactionDueTick(state),simulation::nextNpcDueTick(state),
            state.marketSimulation.nextUpdateTick,simulation::nextPoliceDueTick(state)});
        if (state.clock.tick()<due) {
            // Only idle fixed ticks are skipped. Every scheduled mutation still uses tick().
            state.clock=core::SimulationClock{std::min(end,due)};
            events_.clear(); factionEvents_.clear(); npcEvents_.clear(); territoryEvents_.clear(); missionEvents_.clear(); policeEvents_.clear();
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
    input_.shootPressed = false; input_.interactPressed = false; input_.enterVehiclePressed = false; input_.reloadPressed=false;
}
}
