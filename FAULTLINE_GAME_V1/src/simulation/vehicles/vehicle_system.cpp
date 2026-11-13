#include "simulation/vehicles/vehicle_system.hpp"
#include "world/spatial/streaming.hpp"
#include "world/spatial/character_movement.hpp"
#include "core/rng/deterministic_rng.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <set>

namespace faultline::simulation {
using namespace world;
namespace {
constexpr float pi = 3.14159265358979323846F;
constexpr float tickSeconds =
    static_cast<float>(core::SimulationClock::tickDuration.count()) / 1000.0F;

bool pointInside(Rect rect, double x, double z) {
    return x >= static_cast<double>(rect.x) && x <= static_cast<double>(right(rect)) &&
        z >= static_cast<double>(rect.z) && z <= static_cast<double>(bottom(rect));
}

float wrapYaw(float yaw) {
    return static_cast<float>(std::remainder(static_cast<double>(yaw), 2.0 * pi));
}

std::vector<RoadEdge> districtRoads(const GeneratedWorld& base, DistrictId district,
    const std::map<RoadNodeId, RoadNode>& nodes) {
    const auto districtData = std::find_if(base.districts.begin(), base.districts.end(),
        [&](const auto& value) { return value.id == district; });
    if (districtData == base.districts.end()) return {};
    std::vector<RoadEdge> result;
    for (const auto& edge : base.roads.edges) {
        const auto a = nodes.find(edge.a);
        const auto b = nodes.find(edge.b);
        if (a == nodes.end() || b == nodes.end()) continue;
        const double x = (static_cast<double>(a->second.x) + b->second.x) / 2.0;
        const double z = (static_cast<double>(a->second.z) + b->second.z) / 2.0;
        if (pointInside(districtData->boundary, x, z)) result.push_back(edge);
    }
    if (result.empty()) {
        // Tiny/incomplete districts still spawn on the nearest plausible road
        // rather than fabricating off-road geometry.
        for (const auto& edge : base.roads.edges) {
            const auto a = nodes.find(edge.a);
            const auto b = nodes.find(edge.b);
            if (a == nodes.end() || b == nodes.end()) continue;
            const double x = (static_cast<double>(a->second.x) + b->second.x) / 2.0;
            const double z = (static_cast<double>(a->second.z) + b->second.z) / 2.0;
            const double cx = static_cast<double>(districtData->boundary.x) + districtData->boundary.width / 2.0;
            const double cz = static_cast<double>(districtData->boundary.z) + districtData->boundary.depth / 2.0;
            if ((x - cx) * (x - cx) + (z - cz) * (z - cz) <=
                static_cast<double>(districtData->boundary.width) * districtData->boundary.width +
                static_cast<double>(districtData->boundary.depth) * districtData->boundary.depth)
                result.push_back(edge);
        }
    }
    if (result.empty()) result = base.roads.edges;
    return result;
}
} // namespace

void initializeVehicleSimulation(GameState& state) {
    state.vehicleSimulation = VehicleSimulationState{};
}

void seedVehicleFleet(GameState& state) {
    core::require(state.world.base.has_value(), "Vehicle fleet requires generated geography");
    core::require(state.world.vehicles.entries().empty(), "Vehicle fleet already seeded");
    const auto& base = *state.world.base;
    if (base.roads.edges.empty()) return;
    std::map<RoadNodeId, RoadNode> nodes;
    for (const auto& node : base.roads.nodes) nodes.emplace(node.id, node);
    core::DeterministicRng rng{core::deriveSeed(state.metadata.seed.value, "runtime.vehicles.spawn")};
    const auto spawn = [&](DistrictId district, const std::vector<RoadEdge>& roads,
                           VehicleArchetype archetype, bool police) {
        const auto& edge = roads[rng.uniform(roads.size())];
        const auto a = nodes.find(edge.a);
        const auto b = nodes.find(edge.b);
        core::require(a != nodes.end() && b != nodes.end(), "Vehicle spawn road is incomplete");
        const double t = static_cast<double>(rng.uniform(200, 801)) / 1000.0;
        const double dx = static_cast<double>(b->second.x) - a->second.x;
        const double dz = static_cast<double>(b->second.z) - a->second.z;
        const double length = std::hypot(dx, dz);
        core::require(length > 0, "Vehicle spawn road has zero length");
        const double ux = dx / length, uz = dz / length;
        const double side = rng.chance(1, 2) ? 1.0 : -1.0;
        const double lane = static_cast<double>(state.vehicleSimulation.rules.laneOffsetMeters) * side;
        const double x = static_cast<double>(a->second.x) + dx * t + uz * lane;
        const double z = static_cast<double>(a->second.z) + dz * t - ux * lane;
        const auto position = fromGlobal({x, 0, z}, state.world.spatial.cellSize());
        const auto yaw = static_cast<float>(std::atan2(ux, uz));

        world::WorldLocation location;
        location.district = district;
        FactionId owner;
        for (const auto& territory : base.territories) {
            if (territory.district != district || !pointInside(territory.boundary, x, z)) continue;
            location.territory = territory.id;
            owner = state.world.territories.get(territory.id).owner;
            break;
        }
        const auto& definition = vehicleDefinition(archetype);
        VehicleState vehicle;
        vehicle.category = definition.category;
        vehicle.archetype = archetype;
        vehicle.passengerCapacity = definition.passengerCapacity;
        vehicle.condition.health = {definition.maxHealth, definition.maxHealth};
        vehicle.location = location;
        vehicle.position = position;
        vehicle.yaw = yaw;
        vehicle.speed = 0;
        if (police) vehicle.ownership.kind = VehicleOwnerKind::Police;
        else if (owner) { vehicle.ownership.kind = VehicleOwnerKind::Faction; vehicle.ownership.faction = owner; }
        (void)state.world.vehicles.create(vehicle);
        ++state.vehicleSimulation.statistics.spawned;
    };
    for (const auto& district : base.districts) {
        const auto roads = districtRoads(base, district.id, nodes);
        if (roads.empty()) continue;
        for (std::uint32_t i = 0; i < state.vehicleSimulation.rules.civilianCarsPerDistrict; ++i) {
            const auto archetype = rng.chance(1, 2) ? VehicleArchetype::Sedan : VehicleArchetype::Hatchback;
            spawn(district.id, roads, archetype, false);
        }
        for (std::uint32_t i = 0; i < state.vehicleSimulation.rules.policeCarsPerDistrict; ++i)
            spawn(district.id, roads, VehicleArchetype::PoliceCruiser, true);
    }
}

void restoreVehicleResidency(GameState& state) {
    auto& worldState = state.world;
    for (const auto& resident : state.vehicleSimulation.residents) {
        auto& vehicle = worldState.vehicles.get(resident.vehicle);
        if (vehicle.activeEntity) {
            core::require(vehicle.activeEntity == resident.entity, "Vehicle streaming lease/body mismatch");
            continue;
        }
        core::require(resident.entity && !worldState.entities.exists(resident.entity) &&
            (worldState.entities.nextId() == 0 || resident.entity.value() < worldState.entities.nextId()),
            "Invalid vehicle streaming allocation lease");
        core::require(vehicle.condition.health.current > 0, "Ineligible vehicle streaming resident");
        (void)materializeVehicle(worldState, resident.vehicle, resident.entity);
    }
}

void collectVehicleResidency(GameState& state) {
    std::set<VehiclePersistentId> previous;
    for (const auto& resident : state.vehicleSimulation.residents) previous.insert(resident.vehicle);
    state.vehicleSimulation.residents.clear();
    for (const auto& [id, vehicle] : state.world.vehicles.entries()) {
        if (!vehicle.activeEntity) continue;
        state.vehicleSimulation.residents.push_back({id, vehicle.activeEntity});
        if (!previous.contains(id)) ++state.vehicleSimulation.statistics.materialized;
    }
    for (const auto id : previous) {
        const auto& vehicle = state.world.vehicles.get(id);
        if (!vehicle.activeEntity) ++state.vehicleSimulation.statistics.abstracted;
    }
}

VehiclePersistentId nearestActiveVehicle(const GameState& state, WorldPosition position,
    double rangeMeters) {
    core::require(std::isfinite(rangeMeters) && rangeMeters >= 0, "Invalid vehicle search range");
    VehiclePersistentId result;
    double best = rangeMeters * rangeMeters;
    const auto size = state.world.spatial.cellSize();
    for (const auto& [id, vehicle] : state.world.vehicles.entries()) {
        if (!vehicle.activeEntity || vehicle.condition.health.current <= 0) continue;
        const auto distance = distanceSquared(position,
            state.world.transforms.get(vehicle.activeEntity).position, size);
        if (distance <= best && (!result || distance < best ||
            (distance == best && id < result))) {
            result = id;
            best = distance;
        }
    }
    return result;
}

namespace {
void syncPlayerToVehicle(GameState& state, EntityId player, EntityId vehicleEntity) {
    auto& playerTransform = state.world.transforms.get(player);
    auto& vehicleTransform = state.world.transforms.get(vehicleEntity);
    playerTransform.position = vehicleTransform.position;
    auto& playerBody = state.world.characterBodies.get(player);
    playerBody.yaw = state.world.vehicleBodies.get(vehicleEntity).yaw;
    state.player.yaw = playerBody.yaw;
    state.world.spatial.upsert(player, playerTransform.position);
}
void exitDrivenVehicle(GameState& state, EntityId player) {
    auto& vehicles = state.vehicleSimulation;
    const auto id = vehicles.drivenVehicle;
    core::require(id && state.world.vehicles.exists(id), "Exiting an unknown vehicle");
    auto& vehicle = state.world.vehicles.get(id);
    const auto entity = vehicle.activeEntity;
    core::require(entity && state.world.entities.exists(entity), "Exiting an abstract vehicle");
    auto& transform = state.world.transforms.get(entity);
    auto& body = state.world.vehicleBodies.get(entity);
    auto& physics = state.world.vehiclePhysics.get(entity);
    physics.speed = 0;
    vehicle.position = transform.position;
    vehicle.yaw = body.yaw;
    vehicle.speed = 0;
    // Deterministic exit: try right-side first, fall back to left-side if the
    // right-side swept move would place the player inside solid geometry.
    const float offset = vehicles.rules.exitOffsetMeters;
    const double yaw = static_cast<double>(body.yaw);
    const double rightX = std::cos(yaw);
    const double rightZ = -std::sin(yaw);
    const double leftX = -rightX;
    const double leftZ = -rightZ;
    // Check both exit positions with radius-swept collision.
    const auto rightResult = world::moveWithRadius(state.world, transform.position,
        static_cast<float>(rightX * offset), static_cast<float>(rightZ * offset),
        world::characterCollisionRadius);
    const auto leftResult = world::moveWithRadius(state.world, transform.position,
        static_cast<float>(leftX * offset), static_cast<float>(leftZ * offset),
        world::characterCollisionRadius);
    WorldPosition exitPos;
    if (!rightResult.collided) {
        exitPos = rightResult.position;
    } else if (!leftResult.collided) {
        exitPos = leftResult.position;
    } else {
        // Both sides blocked; use right-side as V1 limitation and advance anyway.
        exitPos = rightResult.position;
    }
    auto& playerTransform = state.world.transforms.get(player);
    playerTransform.position = exitPos;
    auto& playerBody = state.world.characterBodies.get(player);
    playerBody.yaw = body.yaw;
    state.player.yaw = playerBody.yaw;
    state.world.spatial.upsert(player, playerTransform.position);
    vehicles.drivenVehicle = {};
    ++vehicles.statistics.exited;
}
} // namespace

bool updateVehicleControl(GameState& state, const PlayerInputFrame& input) {
    auto& vehicles = state.vehicleSimulation;
    const auto player = state.player.controlledEntity;
    if (!player || !state.world.entities.exists(player)) return false;
    if (vehicles.drivenVehicle) {
        auto& vehicle = state.world.vehicles.get(vehicles.drivenVehicle);
        if (!vehicle.activeEntity && vehicle.condition.health.current > 0)
            (void)materializeVehicle(state.world, vehicles.drivenVehicle);
        if (vehicle.condition.health.current <= 0) {
            // Wrecked vehicle: clear drivenVehicle so the player is not stuck
            // referencing an undrivable entity until the next valid driving state.
            vehicles.drivenVehicle = {};
        }
    }
    const bool driving = vehicles.drivenVehicle &&
        state.world.vehicles.get(vehicles.drivenVehicle).activeEntity &&
        state.world.vehicles.get(vehicles.drivenVehicle).condition.health.current > 0;
    if (input.enterVehiclePressed) {
        if (driving) {
            exitDrivenVehicle(state, player);
            return true;
        }
        auto id = nearestActiveVehicle(state,
            state.world.transforms.get(player).position, vehicles.rules.enterRangeMeters);
        if (!id) {
            // Enter may be requested before the first streaming commit. Select and
            // materialize the nearest eligible semantic vehicle without consuming
            // random numbers or changing its persistent identity.
            const auto playerPosition = state.world.transforms.get(player).position;
            double best = vehicles.rules.enterRangeMeters * vehicles.rules.enterRangeMeters;
            for (const auto& [candidate, record] : state.world.vehicles.entries()) {
                if (record.activeEntity || record.condition.health.current <= 0 ||
                    !validVehicleCategory(record.category) ||
                    !validVehicleArchetype(record.archetype)) continue;
                const auto distance = distanceSquared(playerPosition, record.position,
                    state.world.spatial.cellSize());
                if (distance <= best && (!id || distance < best ||
                    (distance == best && candidate < id))) {
                    id = candidate;
                    best = distance;
                }
            }
            if (id) (void)materializeVehicle(state.world, id);
        }
        if (!id) return false;
        auto& vehicle = state.world.vehicles.get(id);
        const auto entity = vehicle.activeEntity;
        core::require(vehicle.condition.health.current > 0 && entity &&
            state.world.vehicleBodies.contains(entity), "Entering an invalid vehicle");
        vehicles.drivenVehicle = id;
        ++vehicles.statistics.entered;
        syncPlayerToVehicle(state, player, entity);
        return true;
    }
    if (!driving) return false;

    const auto id = vehicles.drivenVehicle;
    auto& vehicle = state.world.vehicles.get(id);
    const auto entity = vehicle.activeEntity;
    auto& transform = state.world.transforms.get(entity);
    auto& body = state.world.vehicleBodies.get(entity);
    auto& physics = state.world.vehiclePhysics.get(entity);
    auto& health = state.world.health.get(entity);
    const auto& definition = vehicleDefinition(vehicle.archetype);
    const float throttle = std::clamp(input.moveZ, -1.0F, 1.0F);
    const float steer = std::clamp(input.moveX, -1.0F, 1.0F);
    if (throttle > 0) physics.speed += definition.acceleration * throttle * tickSeconds;
    else if (throttle < 0) {
        if (physics.speed > 0) physics.speed = std::max(0.0F,
            physics.speed - definition.braking * -throttle * tickSeconds);
        else physics.speed -= definition.acceleration * 0.5F * -throttle * tickSeconds;
    } else if (physics.speed > 0) {
        physics.speed = std::max(0.0F, physics.speed - definition.rollingResistance * tickSeconds);
    } else if (physics.speed < 0) {
        physics.speed = std::min(0.0F, physics.speed + definition.rollingResistance * tickSeconds);
    }
    physics.speed = std::clamp(physics.speed, -definition.reverseSpeed, definition.maxSpeed);
    if (std::abs(physics.speed) > 0.05F) {
        const float direction = physics.speed >= 0 ? 1.0F : -1.0F;
        const float speedFactor = std::min(1.0F, std::abs(physics.speed) / definition.steeringFullSpeed);
        body.yaw = wrapYaw(body.yaw + steer * physics.handling * speedFactor *
            direction * tickSeconds);
    }
    const float distance = physics.speed * tickSeconds;
    const auto moved = moveWithRadius(state.world, transform.position,
        std::sin(body.yaw) * distance, std::cos(body.yaw) * distance, definition.collisionRadius);
    if (moved.collided && std::abs(physics.speed) > 0.05F) {
        const float impactSpeed = std::abs(physics.speed);
        health.current = std::max(0.0F, health.current - std::max(
            vehicles.rules.collisionMinimumDamage,
            impactSpeed * vehicles.rules.collisionDamagePerSpeed));
        physics.speed = 0;
        ++vehicles.statistics.collisions;
    }
    transform.position = moved.position;
    state.world.spatial.upsert(entity, transform.position);
    vehicle.position = transform.position;
    vehicle.yaw = body.yaw;
    vehicle.speed = physics.speed;
    vehicle.condition.health = health;
    syncPlayerToVehicle(state, player, entity);
    if (health.current <= 0) {
        vehicles.drivenVehicle = {};
        ++vehicles.statistics.exited;
    }
    return true;
}

void validateVehicleSimulation(const GameState& state) {
    const auto& vehicles = state.vehicleSimulation;
    core::require(vehicles.version == currentVehicleSimulationVersion,
                  "Unsupported vehicle simulation version");
    core::require(vehicles.rules.civilianCarsPerDistrict <= 32 &&
        vehicles.rules.policeCarsPerDistrict <= 32, "Invalid vehicle spawn rules");
    core::require(std::isfinite(vehicles.rules.enterRangeMeters) && vehicles.rules.enterRangeMeters > 0 &&
        std::isfinite(vehicles.rules.exitOffsetMeters) && vehicles.rules.exitOffsetMeters > 0 &&
        std::isfinite(vehicles.rules.laneOffsetMeters) && vehicles.rules.laneOffsetMeters >= 0 &&
        std::isfinite(vehicles.rules.collisionDamagePerSpeed) &&
        vehicles.rules.collisionDamagePerSpeed >= 0 &&
        std::isfinite(vehicles.rules.collisionMinimumDamage) &&
        vehicles.rules.collisionMinimumDamage >= 0, "Invalid vehicle rules");
    if (vehicles.drivenVehicle) {
        core::require(state.world.vehicles.exists(vehicles.drivenVehicle),
                      "Driven vehicle missing");
        const auto& vehicle = state.world.vehicles.get(vehicles.drivenVehicle);
        core::require(vehicle.condition.health.current > 0,
                      "Driven vehicle must be healthy");
        if (vehicle.activeEntity) {
            core::require(state.world.entities.exists(vehicle.activeEntity) &&
                state.world.vehicleBodies.contains(vehicle.activeEntity),
                "Driven vehicle must have a body");
        } else {
            // A completed-tick snapshot stores the logical lease, not the transient
            // body. It is restored at the next tick start.
            const auto lease = std::lower_bound(vehicles.residents.begin(),
                vehicles.residents.end(), vehicles.drivenVehicle,
                [](const VehicleResidency& resident, VehiclePersistentId id) {
                    return resident.vehicle < id;
                });
            core::require(lease != vehicles.residents.end() &&
                lease->vehicle == vehicles.drivenVehicle,
                "Driven vehicle has no streaming lease");
        }
    }
    VehiclePersistentId previous;
    std::set<EntityId> leased;
    for (const auto& resident : vehicles.residents) {
        core::require(resident.vehicle && resident.vehicle > previous &&
            state.world.vehicles.exists(resident.vehicle) &&
            resident.entity && resident.entity != state.player.controlledEntity &&
            leased.insert(resident.entity).second &&
            (state.world.entities.nextId() == 0 ||
                resident.entity.value() < state.world.entities.nextId()),
            "Invalid vehicle streaming lease");
        const auto& vehicle = state.world.vehicles.get(resident.vehicle);
        core::require(vehicle.condition.health.current > 0, "Invalid vehicle streaming resident");
        core::require(vehicle.activeEntity ? vehicle.activeEntity == resident.entity :
            !state.world.entities.exists(resident.entity), "Vehicle streaming lease collision");
        previous = resident.vehicle;
    }
    if (!state.player.controlledEntity)
        core::require(!vehicles.drivenVehicle, "Driven vehicle requires a player");
}

void appendVehicleSimulationHash(core::StateHash& hash, const GameState& state) {
    const auto& vehicles = state.vehicleSimulation;
    hash.u64(vehicles.version);
    const auto appendFloat = [&](float value) {
        hash.u64(std::bit_cast<std::uint32_t>(value == 0 ? 0.0F : value));
    };
    hash.u64(vehicles.rules.civilianCarsPerDistrict);
    hash.u64(vehicles.rules.policeCarsPerDistrict);
    appendFloat(vehicles.rules.enterRangeMeters);
    appendFloat(vehicles.rules.exitOffsetMeters);
    appendFloat(vehicles.rules.laneOffsetMeters);
    appendFloat(vehicles.rules.collisionDamagePerSpeed);
    appendFloat(vehicles.rules.collisionMinimumDamage);
    hash.u64(vehicles.drivenVehicle.value());
    hash.u64(vehicles.residents.size());
    for (const auto& resident : vehicles.residents) {
        hash.u64(resident.vehicle.value());
        hash.u64(resident.entity.value());
    }
}
} // namespace faultline::simulation
