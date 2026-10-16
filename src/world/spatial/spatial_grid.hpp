#pragma once
#include "world/spatial/world_position.hpp"
#include "core/id/id.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace faultline::world {
class SpatialGrid {
public:
    explicit SpatialGrid(float cellSize = 128.0F) : cellSize_(cellSize) { validateCellSize(cellSize); }
    [[nodiscard]] float cellSize() const { return cellSize_; }
    void upsert(EntityId id, WorldPosition position) {
        core::require(static_cast<bool>(id), "Cannot index null entity");
        position = normalize(position, cellSize_); remove(id);
        cells_[position.cell].insert(id); membership_.insert_or_assign(id, position.cell);
    }
    void remove(EntityId id) {
        const auto found = membership_.find(id);
        if (found == membership_.end()) return;
        const auto cell = cells_.find(found->second); cell->second.erase(id);
        if (cell->second.empty()) cells_.erase(cell);
        membership_.erase(found);
    }
    void clear() { cells_.clear(); membership_.clear(); }
    [[nodiscard]] std::vector<EntityId> queryCell(CellCoord cell) const {
        const auto found = cells_.find(cell);
        if (found == cells_.end()) return {};
        return {found->second.begin(), found->second.end()};
    }
    // Resolver reads authoritative transforms. Index stores no duplicate positions.
    template<class Resolve> [[nodiscard]] std::vector<EntityId> queryRadius(
        WorldPosition center, double radius, Resolve resolve) const {
        core::require(std::isfinite(radius) && radius >= 0 && radius <= std::sqrt(std::numeric_limits<double>::max()),
                      "Invalid query radius");
        center = normalize(center, cellSize_);
        const auto point = toGlobal(center, cellSize_);
        const auto clampCell = [](double coordinate) {
            return static_cast<std::int32_t>(std::clamp(coordinate,
                static_cast<double>(std::numeric_limits<std::int32_t>::min()),
                static_cast<double>(std::numeric_limits<std::int32_t>::max())));
        };
        const auto minX = clampCell(std::floor((point.x - radius) / cellSize_));
        const auto maxX = clampCell(std::floor((point.x + radius) / cellSize_));
        const auto minZ = clampCell(std::floor((point.z - radius) / cellSize_));
        const auto maxZ = clampCell(std::floor((point.z + radius) / cellSize_));
        std::vector<EntityId> result;
        for (auto it = cells_.lower_bound({minX, std::numeric_limits<std::int32_t>::min()});
             it != cells_.end() && it->first.x <= maxX; ++it) {
            if (it->first.z < minZ || it->first.z > maxZ) continue;
            for (const auto id : it->second)
                if (distanceSquared(center, resolve(id), cellSize_) <= radius * radius) result.push_back(id);
        }
        std::sort(result.begin(), result.end()); return result;
    }
private:
    float cellSize_;
    std::map<CellCoord, std::set<EntityId>> cells_;
    std::map<EntityId, CellCoord> membership_;
};
}
