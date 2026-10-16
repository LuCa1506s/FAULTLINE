#pragma once
#include "core/diagnostics/diagnostics.hpp"
#include "core/events/ordered_buffer.hpp"
#include "simulation/runtime/game_state.hpp"
#include <vector>
#include <functional>

namespace faultline {
enum class CommandSource : std::uint8_t { LocalPlayer, Tool };
// A real diagnostic request; gameplay command variants will arrive with their systems.
struct GameCommand {
    std::uint64_t targetTick{};
    std::uint64_t sequence{};
    CommandSource source{};
};
struct PlayerInputFrame {
    float moveX{}, moveZ{}, aimX{}, aimZ{};
    bool shootHeld{}, shootPressed{}, interactPressed{}, enterVehiclePressed{};
};
struct TickInput {
    std::uint64_t tick{};
    PlayerInputFrame player;
    std::vector<GameCommand> commands;
};
class CommandQueue {
public:
    void submit(std::uint64_t targetTick, CommandSource source);
    [[nodiscard]] std::vector<GameCommand> take(std::uint64_t tick);
private:
    std::uint64_t nextSequence_{};
    std::vector<GameCommand> pending_;
};
struct ValidationCompleted { std::uint64_t tick{}, commandSequence{}; };
using DomainEventBuffer = core::OrderedBuffer<ValidationCompleted>;
using StructuralCommandBuffer = core::OrderedBuffer<world::StructuralCommand>;
struct SimulationContext {
    std::uint64_t tick;
    DomainEventBuffer& events;
    StructuralCommandBuffer& structuralChanges;
    RuntimeRandomState& random;
};
class Simulation {
public:
    void tick(GameState& state, const TickInput& input);
    void advanceHeadless(GameState& state, std::uint64_t ticks,
        const std::function<void(const simulation::FactionOperationEvent&)>& observe = {});
    [[nodiscard]] const DomainEventBuffer& events() const { return events_; }
    [[nodiscard]] const simulation::FactionEventBuffer& factionEvents() const { return factionEvents_; }
    [[nodiscard]] const simulation::TerritoryEventBuffer& territoryEvents() const { return territoryEvents_; }
    [[nodiscard]] const simulation::NpcEventBuffer& npcEvents() const { return npcEvents_; }
private:
    simulation::NpcEventBuffer npcEvents_;
    simulation::TerritoryEventBuffer territoryEvents_;
    DomainEventBuffer events_;
    StructuralCommandBuffer structuralChanges_;
    simulation::FactionEventBuffer factionEvents_;
};
class Game {
public:
    explicit Game(WorldSeed seed) : state_(seed) {}
    explicit Game(GameState restored) : state_(std::move(restored)) { validateState(state_); }
    void submit(PlayerInputFrame input) { input_ = input; }
    void requestValidation(std::uint64_t targetTick, CommandSource source = CommandSource::Tool);
    void tick();
    [[nodiscard]] const GameState& state() const { return state_; }
    [[nodiscard]] const DomainEventBuffer& events() const { return simulation_.events(); }
private:
    GameState state_;
    Simulation simulation_;
    CommandQueue commands_;
    PlayerInputFrame input_;
};
}
