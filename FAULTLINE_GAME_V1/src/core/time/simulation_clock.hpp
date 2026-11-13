#pragma once
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace faultline::core {
struct GameTime {
    std::uint64_t milliseconds{};
    auto operator<=>(const GameTime&) const = default;
};
class SimulationClock {
public:
    static constexpr auto tickDuration = std::chrono::milliseconds{50};
    static constexpr std::uint64_t gameMillisecondsPerTick = 2250;
    explicit SimulationClock(std::uint64_t tick = 0) : tick_(tick) {
        if (tick > maxTick) throw std::overflow_error("Simulation clock overflow");
    }
    void advance() {
        if (tick_ == maxTick) throw std::overflow_error("Simulation clock overflow");
        ++tick_;
    }
    [[nodiscard]] std::uint64_t tick() const { return tick_; }
    // GDD: 32 real minutes per game day => 45 game seconds per real second.
    [[nodiscard]] GameTime gameTime() const { return {tick_ * gameMillisecondsPerTick}; }
    static constexpr auto maxTick = std::numeric_limits<std::uint64_t>::max() / gameMillisecondsPerTick;
private:
    std::uint64_t tick_{};
};
}
