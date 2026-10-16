#pragma once
#include "core/diagnostics/diagnostics.hpp"
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>

namespace faultline::world {
struct CellCoord { std::int32_t x{}, z{}; auto operator<=>(const CellCoord&) const = default; };
struct LocalPosition { float x{}, y{}, z{}; auto operator<=>(const LocalPosition&) const = default; };
struct WorldPosition { CellCoord cell; LocalPosition local; auto operator<=>(const WorldPosition&) const = default; };
struct DoubleVec3 { double x{}, y{}, z{}; };
inline void validateCellSize(float size) { core::require(std::isfinite(size) && size > 0, "Invalid cell size"); }
[[nodiscard]] inline WorldPosition normalize(WorldPosition position, float size = 128.0F) {
    validateCellSize(size);
    core::require(std::isfinite(position.local.x) && std::isfinite(position.local.y) &&
                  std::isfinite(position.local.z), "Non-finite position");
    const auto axis = [size](std::int32_t& cell, float& local) {
        const double shift = std::floor(static_cast<double>(local) / size);
        double target = static_cast<double>(cell) + shift;
        local = static_cast<float>(static_cast<double>(local) - shift * size);
        // Conversion to float can round the residual to the cell size.
        if (local >= size) { local = 0; target += 1; }
        core::require(target >= std::numeric_limits<std::int32_t>::min() &&
                      target <= std::numeric_limits<std::int32_t>::max(), "Cell coordinate overflow");
        cell = static_cast<std::int32_t>(target);
    };
    axis(position.cell.x, position.local.x); axis(position.cell.z, position.local.z);
    return position;
}
[[nodiscard]] inline DoubleVec3 toGlobal(WorldPosition position, float size = 128.0F) {
    position = normalize(position, size);
    return {static_cast<double>(position.cell.x) * size + position.local.x, position.local.y,
            static_cast<double>(position.cell.z) * size + position.local.z};
}
[[nodiscard]] inline WorldPosition fromGlobal(DoubleVec3 point, float size = 128.0F) {
    validateCellSize(size);
    core::require(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                  std::abs(point.y) <= std::numeric_limits<float>::max(), "Invalid global position");
    const double cx = std::floor(point.x / size), cz = std::floor(point.z / size);
    core::require(cx >= std::numeric_limits<std::int32_t>::min() && cx <= std::numeric_limits<std::int32_t>::max() &&
                  cz >= std::numeric_limits<std::int32_t>::min() && cz <= std::numeric_limits<std::int32_t>::max(),
                  "Global position outside cell range");
    return normalize({{static_cast<std::int32_t>(cx), static_cast<std::int32_t>(cz)},
        {static_cast<float>(point.x - cx * size), static_cast<float>(point.y), static_cast<float>(point.z - cz * size)}}, size);
}
[[nodiscard]] inline double distanceSquared(WorldPosition a, WorldPosition b, float size = 128.0F) {
    validateCellSize(size);
    const double dx = (static_cast<double>(a.cell.x) - b.cell.x) * size + static_cast<double>(a.local.x) - b.local.x;
    const double dy = static_cast<double>(a.local.y) - b.local.y;
    const double dz = (static_cast<double>(a.cell.z) - b.cell.z) * size + static_cast<double>(a.local.z) - b.local.z;
    return dx * dx + dy * dy + dz * dz;
}
}
