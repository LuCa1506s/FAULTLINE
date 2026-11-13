#pragma once
#include <array>
#include <cstdint>

namespace faultline::core {
class DeterministicRng {
public:
    using State = std::array<std::uint64_t, 4>;
    explicit DeterministicRng(std::uint64_t seed);
    [[nodiscard]] std::uint64_t nextU64();
    [[nodiscard]] std::uint32_t nextU32() { return static_cast<std::uint32_t>(nextU64() >> 32); }
    // Half-open [0, bound); rejection sampling avoids modulo bias.
    [[nodiscard]] std::uint64_t uniform(std::uint64_t bound);
    [[nodiscard]] std::uint64_t uniform(std::uint64_t min, std::uint64_t max);
    [[nodiscard]] bool chance(std::uint64_t numerator, std::uint64_t denominator);
    [[nodiscard]] State state() const { return state_; }
    void restore(State state);
private:
    State state_;
};
}
