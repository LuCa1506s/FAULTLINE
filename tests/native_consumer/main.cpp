#include <faultline/simulation.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Exception, class Operation>
void rejects(Operation operation, const char* message) {
    try { operation(); }
    catch (const Exception&) { return; }
    throw std::runtime_error(message);
}

struct TestSave {
    std::filesystem::path path{"framework-native-consumer.flsave"};
    TestSave() { check(!std::filesystem::exists(path), "Test snapshot destination already exists"); }
    ~TestSave() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};
}

int main() {
    using namespace faultline::sdk;
    try {
        constexpr Config compact{WorldProfile::compact};
        Simulation sim{42, compact};
        Simulation independent{42, compact};
        const auto start = sim.observe();
        const auto initial_hash = sim.state_hash();
        check(independent.state_hash() == initial_hash, "Identical seeds must start identically");

        const auto map = sim.road_map();
        check(!map.edges.empty() && std::find(map.nodes.begin(), map.nodes.end(), start.position) != map.nodes.end(),
            "Generated road map must contain the spawn");
        for (const auto edge : map.edges)
            check(edge.first < map.nodes.size() && edge.second < map.nodes.size(), "Invalid projected edge");

        const auto moved = sim.step(Action::east, 16);
        check(moved.ticks == 16 && moved.observation.tick == 16 &&
            moved.observation.position.x == start.position.x + 4 && moved.observation.position.z == start.position.z,
            "East movement must produce four meters of authoritative displacement");
        check(independent.observe() == start && independent.state_hash() == initial_hash,
            "Sessions must have isolated state");
        independent.step(Action::east, 8);
        independent.step(Action::east, 8);
        const auto checkpoint = sim.state_hash();
        check(independent.state_hash() == checkpoint, "Equivalent ordered inputs must hash identically");

        rejects<std::invalid_argument>([&] { sim.step(static_cast<Action>(255)); }, "Unknown action accepted");
        rejects<std::invalid_argument>([&] { sim.advance(0); }, "Zero ticks accepted");
        rejects<std::invalid_argument>([&] { sim.advance(Simulation::max_step_ticks + 1); }, "Oversized tick batch accepted");
        rejects<std::invalid_argument>([&] { sim.reset(42, {static_cast<WorldProfile>(255)}); }, "Unknown profile accepted");
        auto corrupt = sim.snapshot();
        corrupt.pop_back();
        rejects<SnapshotError>([&] { sim.restore(corrupt); }, "Truncated snapshot accepted");
        corrupt.resize(Simulation::max_snapshot_bytes + 1);
        rejects<SnapshotError>([&] { sim.restore(corrupt); }, "Oversized snapshot accepted");
        check(sim.state_hash() == checkpoint, "Rejected requests must preserve the session");

        TestSave file;
        sim.save(file.path);
        sim.save(file.path); // Exercise replacement on this platform.
        sim.close();
        sim.close();
        rejects<std::logic_error>([&] { (void)sim.observe(); }, "Closed session remained usable");
        sim.load(file.path);
        check(sim.state_hash() == checkpoint, "Persisted checkpoint hash changed");
        sim.advance(100);
        independent.advance(100);
        sim.step(Action::south, 20);
        independent.step(Action::south, 20);
        check(sim.observe() == independent.observe() && sim.state_hash() == independent.state_hash() &&
            sim.snapshot() == independent.snapshot(), "Restored and uninterrupted continuations diverged");

        sim.reset(std::numeric_limits<std::uint64_t>::max(), compact);
        check(sim.observe().tick == 0 && sim.state_hash() != initial_hash, "Unsigned seed/reset contract failed");
        sim.reset(42, compact);
        for (int batch = 0; batch < 5; ++batch) sim.step(Action::east, Simulation::max_step_ticks);
        check(sim.observe().position.x == 256, "Authoritative world bound did not stop movement");

        Simulation standard{42};
        const auto standard_start = standard.observe();
        standard.advance(100);
        check(standard.observe().position == standard_start.position, "Idle must preserve player position");
        std::cout << "Native consumer PASS. Continued StateHash " << std::hex << independent.state_hash() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
