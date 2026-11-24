#include <faultline/simulation.hpp>

#include "persistence/save.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/runtime/game.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <string>
#include <utility>

namespace faultline::sdk {
namespace {
world::WorldGenerationConfig generation_config(Config config) {
    world::WorldGenerationConfig result;
    switch (config.profile) {
    case WorldProfile::standard:
        return result;
    case WorldProfile::compact:
        result.width = result.depth = 512;
        result.originX = result.originZ = -256;
        result.waterPercent = 0;
        return result;
    }
    throw std::invalid_argument("Unknown world profile");
}

WorldPosition spawn_position(const GameState& state) {
    const auto& land = state.world.base->buildableLand;
    // Generation always inserts the buildable-land center in the road graph.
    return world::fromGlobal({static_cast<double>(land.x + land.width / 2), 0,
        static_cast<double>(land.z + land.depth / 2)}, state.world.spatial.cellSize());
}

PlayerInputFrame player_input(Action action) {
    PlayerInputFrame frame;
    switch (action) {
    case Action::idle: break;
    case Action::east: frame.moveX = 1; break;
    case Action::west: frame.moveX = -1; break;
    case Action::south: frame.moveZ = 1; break;
    case Action::north: frame.moveZ = -1; break;
    default: throw std::invalid_argument("Unknown action");
    }
    return frame;
}

Observation observation(const GameState& state) {
    const auto player = state.player.controlledEntity;
    const auto position = world::toGlobal(state.world.transforms.get(player).position,
        state.world.spatial.cellSize());
    const auto health = state.world.health.get(player).current;
    auto status = Status::running;
    if (health <= 0) status = Status::incapacitated;
    else if (state.clock.tick() >= Simulation::max_session_ticks) status = Status::tick_limit;
    return {state.clock.tick(), {position.x, position.z}, health, status};
}

void validate_snapshot(const GameState& state) {
    if (!state.world.base || !state.player.controlledEntity || state.vehicleSimulation.drivenVehicle ||
        state.clock.tick() > Simulation::max_session_ticks ||
        (state.history.version && state.history.months != simulation::historyMonths) ||
        state.prologue.phase == simulation::ProloguePhase::DeliveryComplete)
        throw SnapshotError("Snapshot requires a generated world, a player on foot and a complete tick boundary within the SDK limit");

    const auto& config = state.world.base->config;
    const auto matches_profile = [&config](WorldProfile profile) {
        const auto expected = generation_config({profile});
        return config.width == expected.width && config.depth == expected.depth &&
            config.originX == expected.originX && config.originZ == expected.originZ &&
            config.waterPercent == expected.waterPercent;
    };
    if (!matches_profile(WorldProfile::standard) && !matches_profile(WorldProfile::compact))
        throw SnapshotError("Snapshot world dimensions, origin or coastline profile exceed the SDK contract");
}

void append_event(StepResult& result, Event event) {
    if (result.events.size() < Simulation::max_events) result.events.push_back(event);
    else result.events_truncated = true;
}

void append_events(StepResult& result, const faultline::Simulation& engine) {
    constexpr std::array kinds{EventKind::operation_created, EventKind::operation_started,
        EventKind::operation_resolved, EventKind::operation_failed, EventKind::operation_cancelled};
    for (const auto& event : engine.factionEvents().values())
        append_event(result, {event.tick, kinds.at(static_cast<std::size_t>(event.kind)), event.operation.value()});
    for (const auto& event : engine.territoryEvents().values())
        append_event(result, {event.tick, EventKind::territory_changed, event.territory.value()});
}

// Reserve a unique staging directory beside the destination. A completed write
// can be renamed on the same filesystem, keeping the previous save on failure.
class SaveStaging {
public:
    explicit SaveStaging(const std::filesystem::path& parent) {
        std::random_device random;
        for (int attempt = 0; attempt < 8; ++attempt) {
            auto candidate = parent / (".faultline-" + std::to_string(random()) + "-" + std::to_string(random()));
            auto candidate_file = candidate / "snapshot";
            if (std::filesystem::create_directory(candidate)) {
                directory_ = std::move(candidate);
                file_ = std::move(candidate_file);
                return;
            }
        }
        throw SnapshotError("Cannot allocate a staging directory for the snapshot");
    }
    ~SaveStaging() {
        std::error_code ignored;
        std::filesystem::remove(file_, ignored);
        std::filesystem::remove(directory_, ignored);
    }
    [[nodiscard]] const std::filesystem::path& file() const { return file_; }
private:
    std::filesystem::path directory_;
    std::filesystem::path file_;
};
}

struct Simulation::Impl {
    GameState state;
    faultline::Simulation engine;
};

Simulation::Simulation(std::uint64_t seed, Config config) { reset(seed, config); }
Simulation::~Simulation() = default;
Simulation::Simulation(Simulation&&) noexcept = default;
Simulation& Simulation::operator=(Simulation&&) noexcept = default;

Simulation::Impl& Simulation::session() const {
    if (!impl_) throw std::logic_error("Simulation is closed");
    return *impl_;
}

void Simulation::reset(std::uint64_t seed, Config config) {
    GameState state{WorldSeed{seed}, generation_config(config)};
    simulation::createPlayerEntity(state, spawn_position(state));
    impl_.reset(new Impl{std::move(state), {}});
}

Observation Simulation::observe() const { return observation(session().state); }

RoadMap Simulation::road_map() const {
    const auto& roads = session().state.world.base->roads;
    RoadMap map;
    map.nodes.reserve(roads.nodes.size());
    map.edges.reserve(roads.edges.size());
    for (const auto& node : roads.nodes)
        map.nodes.push_back({static_cast<double>(node.x), static_cast<double>(node.z)});
    const auto index = [&roads](RoadNodeId id) {
        const auto node = std::lower_bound(roads.nodes.begin(), roads.nodes.end(), id,
            [](const auto& entry, RoadNodeId key) { return entry.id < key; });
        return static_cast<std::size_t>(node - roads.nodes.begin());
    };
    for (const auto& edge : roads.edges)
        map.edges.push_back({index(edge.a), index(edge.b)});
    return map;
}

StepResult Simulation::step(Action action, std::uint32_t ticks) {
    auto& active = session();
    const auto input = player_input(action);
    StepResult result{observe(), 0, {}, false};
    if (ticks == 0 || ticks > max_step_ticks)
        throw std::invalid_argument("Ticks must be between 1 and max_step_ticks");
    if (result.observation.status != Status::running)
        throw std::logic_error("Simulation reached a terminal state");
    if (ticks > max_session_ticks - result.observation.tick)
        throw std::invalid_argument("Requested ticks exceed the session limit");
    try {
        while (result.ticks < ticks && result.observation.status == Status::running) {
            active.engine.tick(active.state, {active.state.clock.tick(), input, {}});
            append_events(result, active.engine);
            ++result.ticks;
            result.observation = observe();
        }
    } catch (...) {
        close(); // A failed engine tick may have mutated authority before throwing.
        throw;
    }
    return result;
}

StepResult Simulation::advance(std::uint32_t ticks) { return step(Action::idle, ticks); }

std::uint64_t Simulation::state_hash() const { return faultline::stateHash(session().state); }

std::vector<std::uint8_t> Simulation::snapshot() const {
    try {
        auto bytes = persistence::saveGameState(session().state);
        if (bytes.size() > max_snapshot_bytes) throw SnapshotError("Snapshot exceeds the SDK byte limit");
        return bytes;
    } catch (const persistence::SaveError& error) {
        throw SnapshotError(error.what());
    }
}

void Simulation::restore(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > max_snapshot_bytes) throw SnapshotError("Snapshot exceeds the SDK byte limit");
    try {
        auto state = persistence::loadGameState(bytes);
        validate_snapshot(state);
        impl_.reset(new Impl{std::move(state), {}});
    } catch (const persistence::SaveError& error) {
        throw SnapshotError(error.what());
    }
}

void Simulation::save(const std::filesystem::path& path) const {
    const auto bytes = snapshot();
    try {
        SaveStaging staging{path.parent_path()};
        const auto& file = staging.file();
        std::ofstream output{file, std::ios::binary};
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) throw SnapshotError("Cannot write snapshot");
        std::filesystem::rename(file, path);
    } catch (const std::filesystem::filesystem_error& error) {
        throw SnapshotError(error.what());
    }
}

void Simulation::load(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) throw SnapshotError("Cannot open snapshot");
    const auto size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(max_snapshot_bytes))
        throw SnapshotError("Invalid snapshot size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) throw SnapshotError("Cannot read snapshot");
    restore(bytes);
}

void Simulation::close() noexcept { impl_.reset(); }
}
