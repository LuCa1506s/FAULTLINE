#pragma once
#include "core/diagnostics/diagnostics.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace faultline::world {
// An implicit four-vertex axis-aligned polygon, in exact integer meters.
// Interiors are disjoint when edges merely touch; containment includes boundaries.
struct Rect {
    std::int32_t x{}, z{}, width{}, depth{};
    auto operator<=>(const Rect&) const = default;
};
inline std::int64_t right(Rect r) { return static_cast<std::int64_t>(r.x) + r.width; }
inline std::int64_t bottom(Rect r) { return static_cast<std::int64_t>(r.z) + r.depth; }
inline std::int64_t area(Rect r) { return static_cast<std::int64_t>(r.width) * r.depth; }
inline bool valid(Rect r) {
    return r.width > 0 && r.depth > 0 && right(r) <= std::numeric_limits<std::int32_t>::max() &&
        bottom(r) <= std::numeric_limits<std::int32_t>::max();
}
inline bool inside(Rect inner, Rect outer) {
    return valid(inner) && valid(outer) && inner.x >= outer.x && inner.z >= outer.z &&
        right(inner) <= right(outer) && bottom(inner) <= bottom(outer);
}
inline bool overlap(Rect a, Rect b) {
    return a.x < right(b) && b.x < right(a) && a.z < bottom(b) && b.z < bottom(a);
}
inline bool adjacent(Rect a, Rect b) {
    return ((right(a) == b.x || right(b) == a.x) && std::max(a.z, b.z) < std::min(bottom(a), bottom(b))) ||
        ((bottom(a) == b.z || bottom(b) == a.z) && std::max(a.x, b.x) < std::min(right(a), right(b)));
}
inline bool contains(Rect r, std::int32_t x, std::int32_t z) {
    return x >= r.x && x <= right(r) && z >= r.z && z <= bottom(r);
}
inline std::optional<Rect> intersection(Rect a, Rect b) {
    const auto x = std::max(a.x, b.x), z = std::max(a.z, b.z);
    const auto endX = std::min(right(a), right(b)), endZ = std::min(bottom(a), bottom(b));
    if (endX <= x || endZ <= z) return std::nullopt;
    return Rect{x, z, static_cast<std::int32_t>(endX - x), static_cast<std::int32_t>(endZ - z)};
}
inline Rect inset(Rect r, std::int32_t margin) {
    core::require(valid(r) && margin >= 0 && static_cast<std::int64_t>(margin) * 2 < std::min(r.width, r.depth),
                  "Invalid rectangle inset");
    return {r.x + margin, r.z + margin, r.width - margin * 2, r.depth - margin * 2};
}
}
