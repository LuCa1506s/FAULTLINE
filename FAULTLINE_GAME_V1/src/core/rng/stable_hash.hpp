#pragma once
#include "core/id/id.hpp"
#include <string_view>

namespace faultline::core {
// FNV-1a 64; integers use exactly eight little-endian bytes, never object memory.
class StateHash {
public:
    constexpr void byte(std::uint8_t value) { value_ = (value_ ^ value) * 1099511628211ULL; }
    constexpr void u64(std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) { byte(static_cast<std::uint8_t>(value)); value >>= 8; }
    }
    constexpr void string(std::string_view value) {
        u64(value.size());
        for (char ch : value) byte(static_cast<std::uint8_t>(ch));
    }
    [[nodiscard]] constexpr std::uint64_t value() const { return value_; }
private:
    std::uint64_t value_ = 14695981039346656037ULL;
};
[[nodiscard]] constexpr std::uint64_t stableHash(std::string_view text) {
    StateHash hash;
    for (char ch : text) hash.byte(static_cast<std::uint8_t>(ch));
    return hash.value();
}
[[nodiscard]] constexpr std::uint64_t deriveSeed(std::uint64_t seed, std::string_view tag) {
    StateHash hash; hash.string("FAULTLINE.seed.v1"); hash.u64(seed); hash.string(tag);
    return hash.value();
}
template<class Tag> [[nodiscard]] constexpr std::uint64_t deriveSeed(
    std::uint64_t seed, std::string_view tag, Id<Tag> id) {
    StateHash hash; hash.u64(deriveSeed(seed, tag)); hash.u64(id.value()); return hash.value();
}
}
