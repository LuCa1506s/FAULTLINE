#include "world/spatial/character_movement.hpp"

namespace faultline::world {
namespace {
// Swept point against an expanded footprint. Touching a wall is allowed; entering
// its interior is blocked. Axis-separated clipping supplies deterministic sliding.
double clipAxis(double from, double to, double other, double low, double high, double sideLow, double sideHigh) {
    if (other <= sideLow || other >= sideHigh) return to;
    if (from <= low && to > low) return low;
    if (from >= high && to < high) return high;
    return to;
}
}
WorldPosition moveCharacter(const WorldState& world, WorldPosition origin, float dx, float dz) {
    const auto start=toGlobal(origin,world.spatial.cellSize());
    auto end=start; end.x+=dx; end.z+=dz;
    if (world.base) {
        const auto& base=*world.base;
        const auto obstacle=[&](Rect r) {
            const double x0=r.x-characterCollisionRadius, x1=static_cast<double>(right(r))+characterCollisionRadius;
            const double z0=r.z-characterCollisionRadius, z1=static_cast<double>(bottom(r))+characterCollisionRadius;
            end.x=clipAxis(start.x,end.x,start.z,x0,x1,z0,z1);
            end.z=clipAxis(start.z,end.z,end.x,z0,z1,x0,x1);
        };
        for (const auto& b : base.buildings) obstacle(b.footprint);
        for (const auto& w : base.water) obstacle(w.boundary);
        if (start.x>=base.bounds.x && start.x<=static_cast<double>(right(base.bounds)) && start.z>=base.bounds.z && start.z<=static_cast<double>(bottom(base.bounds))) {
            end.x=std::clamp(end.x,static_cast<double>(base.bounds.x),static_cast<double>(right(base.bounds)));
            end.z=std::clamp(end.z,static_cast<double>(base.bounds.z),static_cast<double>(bottom(base.bounds)));
        }
    }
    // Preserve cell/local arithmetic, including its established rounding contract.
    origin.local.x+=static_cast<float>(end.x-start.x);
    origin.local.z+=static_cast<float>(end.z-start.z);
    return normalize(origin,world.spatial.cellSize());
}
}
