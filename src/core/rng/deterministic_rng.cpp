#include "core/rng/deterministic_rng.hpp"
#include <bit>
#include <stdexcept>

namespace faultline::core {
DeterministicRng::DeterministicRng(std::uint64_t seed) {
    // SplitMix64 seed expansion, then xoshiro256** 1.0 (Blackman/Vigna).
    for (auto& word : state_) {
        seed += 0x9e3779b97f4a7c15ULL;
        auto z = seed;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        word = z ^ (z >> 31);
    }
}
std::uint64_t DeterministicRng::nextU64() {
    const auto result = std::rotl(state_[1] * 5, 7) * 9;
    const auto t = state_[1] << 17;
    state_[2] ^= state_[0]; state_[3] ^= state_[1];
    state_[1] ^= state_[2]; state_[0] ^= state_[3];
    state_[2] ^= t; state_[3] = std::rotl(state_[3], 45);
    return result;
}
std::uint64_t DeterministicRng::uniform(std::uint64_t bound) {
    if (bound == 0) throw std::invalid_argument("RNG bound must be positive");
    const auto threshold = (std::uint64_t{0} - bound) % bound;
    for (;;) { const auto value = nextU64(); if (value >= threshold) return value % bound; }
}
std::uint64_t DeterministicRng::uniform(std::uint64_t min, std::uint64_t max) {
    if (max <= min) throw std::invalid_argument("Empty RNG interval");
    return min + uniform(max - min);
}
bool DeterministicRng::chance(std::uint64_t numerator, std::uint64_t denominator) {
    if (denominator == 0 || numerator > denominator) throw std::invalid_argument("Invalid probability");
    return uniform(denominator) < numerator;
}
void DeterministicRng::restore(State state) {
    if ((state[0] | state[1] | state[2] | state[3]) == 0) throw std::invalid_argument("Zero xoshiro state");
    state_ = state;
}
}
