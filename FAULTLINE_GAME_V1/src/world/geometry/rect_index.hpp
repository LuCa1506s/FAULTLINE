#pragma once
#include "world/geometry/rect.hpp"
#include <numeric>
#include <span>
#include <vector>

namespace faultline::world {
// Rebuildable static AABB tree; indices refer to the caller's canonical input order.
// It indexes geometry, independently of entity IDs and streaming cell size.
class RectIndex {
public:
    explicit RectIndex(std::span<const Rect> rectangles) : rectangles_(rectangles.begin(), rectangles.end()) {
        std::vector<std::size_t> order(rectangles.size()); std::iota(order.begin(), order.end(), 0);
        for (const auto r : rectangles_) core::require(valid(r), "Cannot index invalid rectangle");
        if (!order.empty()) build(order, 0, order.size());
    }
    [[nodiscard]] std::vector<std::size_t> query(Rect rectangle) const {
        std::vector<std::size_t> result;
        if (nodes_.empty() || !valid(rectangle)) return result;
        visit(0, rectangle, result); std::sort(result.begin(), result.end()); return result;
    }
private:
    struct Node { Rect bounds; std::size_t left{}, right{}, item{}; bool leaf{}; };
    std::vector<Rect> rectangles_;
    std::vector<Node> nodes_;
    std::size_t build(std::vector<std::size_t>& order, std::size_t begin, std::size_t end) {
        const auto index = nodes_.size(); Rect bounds = rectangles_[order[begin]];
        for (std::size_t i = begin + 1; i < end; ++i) {
            const auto r = rectangles_[order[i]];
            const auto x = std::min(bounds.x, r.x), z = std::min(bounds.z, r.z);
            const auto w = std::max(world::right(bounds), world::right(r)) - x;
            const auto d = std::max(bottom(bounds), bottom(r)) - z;
            core::require(w <= std::numeric_limits<std::int32_t>::max() && d <= std::numeric_limits<std::int32_t>::max(),
                          "Geometry index extent overflow");
            bounds = {x, z, static_cast<std::int32_t>(w), static_cast<std::int32_t>(d)};
        }
        nodes_.push_back({bounds, 0, 0, order[begin], end - begin == 1});
        if (end - begin > 1) {
            const bool xAxis = bounds.width >= bounds.depth;
            std::sort(order.begin() + static_cast<std::ptrdiff_t>(begin), order.begin() + static_cast<std::ptrdiff_t>(end),
                [&](auto a, auto b) {
                    const auto ra = rectangles_[a], rb = rectangles_[b];
                    const auto ca = xAxis ? static_cast<std::int64_t>(ra.x) * 2 + ra.width : static_cast<std::int64_t>(ra.z) * 2 + ra.depth;
                    const auto cb = xAxis ? static_cast<std::int64_t>(rb.x) * 2 + rb.width : static_cast<std::int64_t>(rb.z) * 2 + rb.depth;
                    return ca == cb ? a < b : ca < cb;
                });
            const auto middle = begin + (end - begin) / 2;
            const auto left = build(order, begin, middle), rightChild = build(order, middle, end);
            nodes_[index].left = left; nodes_[index].right = rightChild;
        }
        return index;
    }
    void visit(std::size_t index, Rect rectangle, std::vector<std::size_t>& result) const {
        const auto& node = nodes_[index]; if (!overlap(node.bounds, rectangle)) return;
        if (node.leaf) result.push_back(node.item);
        else { visit(node.left, rectangle, result); visit(node.right, rectangle, result); }
    }
};
}
