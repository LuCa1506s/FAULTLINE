#pragma once
#include "core/time/simulation_clock.hpp"
#include <cassert>
#include <ostream>
#include <source_location>
#include <stdexcept>
#include <string>

namespace faultline::core {
struct DebugContext {
    std::uint64_t worldSeed{};
    std::uint32_t generatorVersion{1};
    std::uint64_t simulationTick{};
    GameTime gameTime{};
};
inline std::ostream& operator<<(std::ostream& out, const DebugContext& context) {
    return out << "WorldSeed: " << context.worldSeed << " GeneratorVersion: " << context.generatorVersion
        << " SimulationTick: " << context.simulationTick << " GameTime(ms): " << context.gameTime.milliseconds;
}
inline void require(bool condition, const char* message,
                    std::source_location location = std::source_location::current()) {
    if (!condition) throw std::logic_error(std::string{location.file_name()} + ":" +
        std::to_string(location.line()) + " " + message);
}
}
