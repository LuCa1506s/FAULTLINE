#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace faultline::sdk {

enum class WorldProfile : std::uint8_t { standard, compact };
struct Config { WorldProfile profile{WorldProfile::standard}; };
enum class Action : std::uint8_t { idle, east, west, south, north };
enum class Status : std::uint8_t { running, incapacitated, tick_limit };

struct Point {
    double x{}, z{};
    auto operator<=>(const Point&) const = default;
};

struct Observation {
    std::uint64_t tick{};
    Point position;
    float health{};
    Status status{Status::running};
    auto operator<=>(const Observation&) const = default;
};

struct RoadEdge { std::size_t first{}, second{}; };
struct RoadMap {
    std::vector<Point> nodes;
    std::vector<RoadEdge> edges; // Endpoints are zero-based indices into nodes.
};

enum class EventKind : std::uint8_t {
    operation_created, operation_started, operation_resolved,
    operation_failed, operation_cancelled, territory_changed
};
struct Event {
    std::uint64_t tick{}; // Tick being processed when the domain event was emitted.
    EventKind kind{};
    std::uint64_t subject{}; // Operation or territory ID according to kind.
};
struct StepResult {
    Observation observation;
    std::uint32_t ticks{};
    std::vector<Event> events;
    bool events_truncated{};
};

class SnapshotError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A single-threaded, owning session. Observations and maps are detached values.
// Applications own the session, file paths and diagnostics. Policies consume
// observations and return Action values.
class Simulation {
public:
    static constexpr std::uint32_t max_step_ticks = 256;
    static constexpr std::uint64_t max_session_ticks = 100'000;
    static constexpr std::size_t max_events = 256;
    static constexpr std::size_t max_snapshot_bytes = 8 * 1024 * 1024;

    explicit Simulation(std::uint64_t seed = 0, Config config = {});
    ~Simulation();
    Simulation(Simulation&&) noexcept;
    Simulation& operator=(Simulation&&) noexcept;
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void reset(std::uint64_t seed, Config config = {});
    [[nodiscard]] Observation observe() const;
    [[nodiscard]] RoadMap road_map() const;
    StepResult step(Action action, std::uint32_t ticks = 1);
    StepResult advance(std::uint32_t ticks);

    [[nodiscard]] std::uint64_t state_hash() const;
    [[nodiscard]] std::vector<std::uint8_t> snapshot() const;
    void restore(std::span<const std::uint8_t> bytes);
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
    void close() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    [[nodiscard]] Impl& session() const;
};
}
