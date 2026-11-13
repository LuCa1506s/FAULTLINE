#include "world/spatial/character_movement.hpp"

namespace faultline::world {
namespace {
bool segmentRect(DoubleVec3 a, DoubleVec3 b, Rect r) {
    double lo=0,hi=1; const double dx=b.x-a.x,dz=b.z-a.z;
    const auto clip=[&](double p,double q) {
        if (std::abs(p)<1e-12) return q>=0;
        const double t=q/p;
        if (p<0) lo=std::max(lo,t); else hi=std::min(hi,t);
        return lo<=hi;
    };
    return clip(-dx,a.x-static_cast<double>(r.x)) &&
        clip(dx,static_cast<double>(right(r))-a.x) &&
        clip(-dz,a.z-static_cast<double>(r.z)) &&
        clip(dz,static_cast<double>(bottom(r))-a.z) &&
        lo>1e-6 && lo<1.0-1e-6;
}
// Swept point against an expanded footprint. Touching a wall is allowed; entering
// its interior is blocked. Axis-separated clipping supplies deterministic sliding.
double clipAxis(double from, double to, double other, double low, double high, double sideLow, double sideHigh) {
    if (other <= sideLow || other >= sideHigh) return to;
    if (from <= low && to > low) return low;
    if (from >= high && to < high) return high;
    return to;
}
}
bool hasStaticLineOfSight(const WorldState& world, WorldPosition from, WorldPosition to) {
    if (!world.base) return true;
    const auto a=toGlobal(from,world.spatial.cellSize());
    const auto b=toGlobal(to,world.spatial.cellSize());
    for (const auto& building:world.base->buildings)
        if (segmentRect(a,b,building.footprint)) return false;
    for (const auto& water:world.base->water)
        if (segmentRect(a,b,water.boundary)) return false;
    return true;
}
MovementResult moveWithRadius(const WorldState& world, WorldPosition origin, float dx, float dz, double radius) {
    core::require(std::isfinite(dx) && std::isfinite(dz) && std::isfinite(radius) && radius >= 0,
                  "Invalid movement input");
    const auto start=toGlobal(origin,world.spatial.cellSize());
    auto end=start; end.x+=dx; end.z+=dz;
    const auto desired=end;
    if (world.base) {
        const auto& base=*world.base;
        const auto obstacle=[&](Rect r) {
            const double x0=r.x-radius, x1=static_cast<double>(right(r))+radius;
            const double z0=r.z-radius, z1=static_cast<double>(bottom(r))+radius;
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
    const bool collided=std::abs(end.x-desired.x)>1e-9 || std::abs(end.z-desired.z)>1e-9;
    return {normalize(origin,world.spatial.cellSize()),collided};
}
WorldPosition moveCharacter(const WorldState& world, WorldPosition origin, float dx, float dz) {
    return moveWithRadius(world,origin,dx,dz,characterCollisionRadius).position;
}
}
