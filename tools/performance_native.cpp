#include <faultline/simulation.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

#ifdef __linux__
#include <sys/resource.h>
#endif

using namespace faultline::sdk;
using Clock = std::chrono::steady_clock;
constexpr Config compact{WorldProfile::compact};

namespace {
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

long peak_rss() {
#ifdef __linux__
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) return usage.ru_maxrss;
#endif
    return -1;
}

void measure() {
    constexpr int repeats = 64;
    constexpr int decisions = 4096;
    constexpr int serializations = 128;
    constexpr std::array actions{Action::east, Action::west, Action::south, Action::north};
    double initialization = 0;
    for (int sample = 0; sample < repeats + 4; ++sample) {
        const auto start = Clock::now();
        Simulation instance(42, compact);
        if (sample >= 4) initialization += elapsed(start);
    }
    Simulation simulation(42, compact);
    auto start = Clock::now();
    for (int decision = 0; decision < decisions; ++decision)
        simulation.step(actions[static_cast<std::size_t>(decision) % actions.size()], 8);
    const auto steps = elapsed(start);
    const auto hash = simulation.state_hash();
    const auto observation = simulation.observe();
    const auto snapshot = simulation.snapshot();
    std::size_t encoded_bytes = 0;
    start = Clock::now();
    for (int sample = 0; sample < serializations; ++sample) encoded_bytes += simulation.snapshot().size();
    const auto save = elapsed(start);
    start = Clock::now();
    for (int sample = 0; sample < serializations; ++sample) simulation.restore(snapshot);
    const auto restore = elapsed(start);
    if (simulation.state_hash() != hash || encoded_bytes != serializations * snapshot.size())
        throw std::runtime_error("Serialization changed the measured state");
    std::cout << std::setprecision(12) << "{\"init_seconds\":" << initialization / repeats
              << ",\"step_seconds\":" << steps / decisions << ",\"ticks_per_second\":" << observation.tick / steps
              << ",\"ticks\":" << observation.tick << ",\"decisions\":" << decisions
              << ",\"snapshot_bytes\":" << snapshot.size() << ",\"native_observation_bytes\":" << sizeof(Observation)
              << ",\"snapshot_seconds\":" << save / serializations << ",\"restore_seconds\":" << restore / serializations
              << ",\"peak_rss_kib\":" << peak_rss() << ",\"state_hash\":\"" << std::hex << hash << "\"}\n";
}

void fuzz() {
    Simulation simulation(42, compact);
    simulation.step(Action::east, 64);
    const auto valid = simulation.snapshot();
    const auto baseline = simulation.state_hash();
    std::mt19937 random(40679);
    int accepted = 0;
    int rejected = 0;
    for (int test = 0; test < 1024; ++test) {
        std::vector<std::uint8_t> candidate;
        if (test < 256) {
            candidate.resize(random() % 4096);
            for (auto& byte : candidate) byte = static_cast<std::uint8_t>(random());
        } else {
            candidate = valid;
            if (test < 512) candidate.resize(random() % valid.size());
            else candidate[random() % valid.size()] ^= static_cast<std::uint8_t>(1U << (random() % 8));
        }
        try {
            simulation.restore(candidate);
            ++accepted;
            const auto hash = simulation.state_hash();
            simulation.restore(candidate);
            if (simulation.state_hash() != hash) throw std::runtime_error("Valid mutation restored inconsistently");
            simulation.restore(valid);
        } catch (const SnapshotError&) {
            ++rejected;
            if (simulation.state_hash() != baseline) throw std::runtime_error("Rejected restore changed the live state");
        }
    }
    if (accepted + rejected != 1024) throw std::runtime_error("A repeated restore of valid input failed");
    std::cout << "{\"cases\":1024,\"accepted_mutations\":" << accepted << ",\"rejected\":" << rejected << "}\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "fuzz") fuzz();
        else if (argc == 1) measure();
        else throw std::invalid_argument("Use performance_native [fuzz]");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
