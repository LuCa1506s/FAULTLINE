#pragma once
#include "core/time/simulation_clock.hpp"

namespace faultline {
// Application-owned elapsed time; backlog is retained rather than dropping ticks.
class FixedStepRunner {
public:
    void addTime(std::chrono::nanoseconds elapsed) {
        if (elapsed.count() < 0) throw std::invalid_argument("Negative elapsed time");
        if (elapsed.count() > std::numeric_limits<std::int64_t>::max() - accumulated_.count())
            throw std::overflow_error("Fixed step accumulator overflow");
        accumulated_ += elapsed;
    }
    [[nodiscard]] bool shouldTick() const { return accumulated_ >= core::SimulationClock::tickDuration; }
    void consumeTick() {
        if (!shouldTick()) throw std::logic_error("No tick available");
        accumulated_ -= core::SimulationClock::tickDuration;
    }
    [[nodiscard]] std::chrono::nanoseconds backlog() const { return accumulated_; }
private:
    std::chrono::nanoseconds accumulated_{};
};
}
