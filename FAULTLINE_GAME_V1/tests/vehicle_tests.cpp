#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "simulation/vehicles/vehicle_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include "world/spatial/streaming.hpp"
#include "world/spatial/character_movement.hpp"
#include "save_compatibility.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

namespace {
GameState freshWorld(std::uint64_t seed=12345) {
    world::WorldGenerationConfig config;
    config.width=512; config.depth=512; config.waterPercent=0;
    return GameState{WorldSeed{seed},config};
}
std::vector<VehiclePersistentId> vehicleIds(const GameState& state) {
    std::vector<VehiclePersistentId> result;
    for (const auto& [id,vehicle] : state.world.vehicles.entries()) {
        (void)vehicle;
        result.push_back(id);
    }
    return result;
}
bool resident(const GameState& state,VehiclePersistentId id,EntityId entity) {
    return std::binary_search(state.vehicleSimulation.residents.begin(),
        state.vehicleSimulation.residents.end(),VehicleResidency{id,entity});
}
world::WorldPosition farAway(const GameState& state,world::WorldPosition position) {
    auto global=world::toGlobal(position,state.world.spatial.cellSize());
    global.x+=20000;
    return world::fromGlobal(global,state.world.spatial.cellSize());
}
}

TEST_CASE("fresh games seed deterministic civilian and police cars on district roads") {
    GameState state{WorldSeed{12345}};
    const auto districts=state.world.districts.entries().size();
    REQUIRE(districts>0);
    CHECK(state.vehicleSimulation.version==currentVehicleSimulationVersion);
    CHECK(state.world.vehicles.entries().size()==
        districts*(state.vehicleSimulation.rules.civilianCarsPerDistrict+
                   state.vehicleSimulation.rules.policeCarsPerDistrict));
    std::uint64_t police=0,civilian=0;
    for (const auto& [id,vehicle] : state.world.vehicles.entries()) {
        CAPTURE(id.value());
        CHECK(vehicle.category==world::VehicleCategory::Car);
        CHECK(world::validVehicleArchetype(vehicle.archetype));
        const auto& definition=world::vehicleDefinition(vehicle.archetype);
        CHECK(vehicle.passengerCapacity==definition.passengerCapacity);
        CHECK(vehicle.condition.health.current==definition.maxHealth);
        CHECK(vehicle.condition.health.maximum==definition.maxHealth);
        CHECK(vehicle.position==world::normalize(vehicle.position,state.world.spatial.cellSize()));
        CHECK_FALSE(vehicle.activeEntity);
        REQUIRE(static_cast<bool>(vehicle.location.district));
        CHECK(state.world.districts.exists(vehicle.location.district));
        if (vehicle.ownership.kind==world::VehicleOwnerKind::Police) {
            ++police; CHECK(vehicle.archetype==world::VehicleArchetype::PoliceCruiser);
            CHECK_FALSE(vehicle.ownership.faction);
        } else {
            ++civilian;
            const bool civilianArchetype=vehicle.archetype==world::VehicleArchetype::Sedan ||
                  vehicle.archetype==world::VehicleArchetype::Hatchback;
            CHECK(civilianArchetype);
            if (vehicle.ownership.kind==world::VehicleOwnerKind::Faction) {
                CHECK(state.world.factions.exists(vehicle.ownership.faction));
                CHECK(vehicle.location.territory);
            } else CHECK(vehicle.ownership.kind==world::VehicleOwnerKind::Unowned);
        }
    }
    CHECK(police==districts*state.vehicleSimulation.rules.policeCarsPerDistrict);
    CHECK(civilian==districts*state.vehicleSimulation.rules.civilianCarsPerDistrict);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("vehicle fleet identity and save bytes are seed-deterministic") {
    const auto a=freshWorld(); const auto b=freshWorld();
    CHECK(vehicleIds(a)==vehicleIds(b));
    CHECK(stateHash(a)==stateHash(b));
    CHECK(saveGameState(a)==saveGameState(b));
    for (const auto id : vehicleIds(a))
        CHECK(a.world.vehicles.get(id)==b.world.vehicles.get(id));
    auto restored=loadGameState(saveGameState(a));
    CHECK(stateHash(restored)==stateHash(a));
    CHECK(saveGameState(restored)==saveGameState(a));
    CHECK(restored.world.vehicles.entries().size()==a.world.vehicles.entries().size());
}

TEST_CASE("streaming materializes and abstracts vehicles without changing identity") {
    auto state=freshWorld();
    const auto id=state.world.vehicles.entries().begin()->first;
    const auto semantic=state.world.vehicles.get(id);
    const auto player=createPlayerEntity(state,semantic.position);
    const auto fleetBefore=state.world.vehicles.entries();
    world::WorldStreamingSystem streaming;
    streaming.commit(state.world,semantic.position);
    collectVehicleResidency(state);
    REQUIRE(state.world.vehicles.get(id).activeEntity);
    const auto body=state.world.vehicles.get(id).activeEntity;
    const auto nextIdBeforeAbstraction=state.world.entities.nextId();
    CHECK(state.world.vehicleLinks.get(body).vehicle==id);
    CHECK(state.world.vehicleBodies.contains(body));
    CHECK(state.world.vehiclePhysics.contains(body));
    CHECK(resident(state,id,body));
    CHECK(world::persistentSnapshot(state.world).vehicles.entries()==fleetBefore);
    state.world.transforms.get(player).position=farAway(state,semantic.position);
    streaming.commit(state.world,state.world.transforms.get(player).position);
    collectVehicleResidency(state);
    CHECK_FALSE(state.world.vehicles.get(id).activeEntity);
    CHECK_FALSE(state.world.entities.exists(body));
    CHECK_FALSE(state.world.vehicleBodies.contains(body));
    CHECK_FALSE(state.world.vehiclePhysics.contains(body));
    CHECK_FALSE(resident(state,id,body));
    CHECK(state.world.vehicles.get(id).position==semantic.position);
    CHECK(state.world.vehicles.get(id).condition==semantic.condition);
    CHECK(state.world.entities.nextId()==nextIdBeforeAbstraction);
    CHECK(state.world.vehicles.entries()==fleetBefore);
}

namespace {
VehiclePersistentId openVehicle(const GameState& state,double meters,bool policeOnly=false) {
    for (const auto& [id,vehicle] : state.world.vehicles.entries()) {
        if (vehicle.condition.health.current<=0) continue;
        if (policeOnly && vehicle.ownership.kind!=world::VehicleOwnerKind::Police) continue;
        const auto& definition=world::vehicleDefinition(vehicle.archetype);
        const auto moved=world::moveWithRadius(state.world,vehicle.position,
            std::sin(vehicle.yaw)*static_cast<float>(meters),
            std::cos(vehicle.yaw)*static_cast<float>(meters),definition.collisionRadius);
        if (moved.collided) continue;
        const auto travelled=std::sqrt(world::distanceSquared(vehicle.position,moved.position,
            state.world.spatial.cellSize()));
        if (travelled>=meters*0.9) return id;
    }
    return {};
}
PlayerInputFrame driveInput(std::uint64_t tick) {
    PlayerInputFrame input;
    input.moveZ=1.0F;
    // Straight acceleration for clearance, then a gentle deterministic curve.
    if (tick>=30) input.moveX=0.25F;
    return input;
}
std::map<std::uint32_t,SaveBytes> sectionPayloads(const SaveBytes& bytes) {
    detail::Reader reader{bytes}; reader.offset=48;
    std::map<std::uint32_t,SaveBytes> result;
    while (reader.remaining()) {
        std::uint32_t id{},length{}; reader(id,length); reader.need(length);
        result.emplace(id,SaveBytes{bytes.begin()+static_cast<std::ptrdiff_t>(reader.offset),
            bytes.begin()+static_cast<std::ptrdiff_t>(reader.offset+length)});
        reader.offset+=length;
    }
    return result;
}
}

TEST_CASE("enter drive save load and exit preserve persistent vehicle identity") {
    auto state=freshWorld();
    const auto id=openVehicle(state,40.0);
    REQUIRE(id);
    const auto semantic=state.world.vehicles.get(id);
    const auto player=createPlayerEntity(state,semantic.position);
    Simulation simulation;
    PlayerInputFrame enter; enter.enterVehiclePressed=true;
    simulation.tick(state,{state.clock.tick(),enter,{}});
    REQUIRE(state.vehicleSimulation.drivenVehicle==id);
    const auto body=state.world.vehicles.get(id).activeEntity;
    REQUIRE(body);
    CHECK(state.world.vehicleLinks.get(body).vehicle==id);
    CHECK(state.world.transforms.get(player).position==state.world.transforms.get(body).position);
    CHECK(state.world.characterBodies.get(player).yaw==state.world.vehicleBodies.get(body).yaw);
    CHECK(state.player.yaw==state.world.characterBodies.get(player).yaw);

    const auto startPosition=state.world.transforms.get(body).position;
    auto parallel=state;
    Simulation reference;
    for (std::uint64_t tick=0;tick<30;++tick) {
        const auto input=driveInput(tick);
        simulation.tick(state,{state.clock.tick(),input,{}});
        reference.tick(parallel,{parallel.clock.tick(),input,{}});
        CHECK(stateHash(state)==stateHash(parallel));
    }
    CHECK(state.vehicleSimulation.drivenVehicle==id);
    CHECK(state.world.transforms.get(body).position!=startPosition);
    CHECK(state.world.vehiclePhysics.get(body).speed>0);
    CHECK_NOTHROW(validateState(state));

    const auto checkpoint=saveGameState(state);
    auto restored=loadGameState(checkpoint);
    CHECK(readSaveHeader(checkpoint).saveFormatVersion.value==14);
    CHECK(stateHash(restored)==stateHash(state));
    CHECK(saveGameState(restored)==checkpoint);
    CHECK(restored.vehicleSimulation.drivenVehicle==id);
    CHECK_FALSE(restored.world.vehicles.get(id).activeEntity);
    REQUIRE(!restored.vehicleSimulation.residents.empty());
    restoreVehicleResidency(restored);
    CHECK(restored.world.vehicles.get(id).activeEntity==body);
    CHECK(restored.world.transforms.get(body).position==state.world.transforms.get(body).position);
    CHECK(restored.world.vehicleBodies.get(body)==state.world.vehicleBodies.get(body));
    CHECK(restored.world.vehiclePhysics.get(body).speed==state.world.vehiclePhysics.get(body).speed);

    Simulation resumed;
    for (std::uint64_t tick=30;tick<45;++tick) {
        const auto input=driveInput(tick);
        simulation.tick(state,{state.clock.tick(),input,{}});
        resumed.tick(restored,{restored.clock.tick(),input,{}});
        CHECK(stateHash(state)==stateHash(restored));
        CHECK(state.world.transforms.get(body).position==restored.world.transforms.get(body).position);
    }
    CHECK(saveGameState(state)==saveGameState(restored));

    const auto vehiclePosition=state.world.transforms.get(body).position;
    PlayerInputFrame exit; exit.enterVehiclePressed=true;
    simulation.tick(state,{state.clock.tick(),exit,{}});
    CHECK_FALSE(state.vehicleSimulation.drivenVehicle);
    CHECK(state.world.transforms.get(player).position!=vehiclePosition);
    CHECK(state.world.transforms.get(body).position==vehiclePosition);
    CHECK_NOTHROW(validateState(state));
}

TEST_CASE("police cars retain category ownership and identity through control") {
    auto state=freshWorld();
    const auto id=openVehicle(state,30.0,true);
    REQUIRE(id);
    const auto& semantic=state.world.vehicles.get(id);
    CHECK(semantic.category==world::VehicleCategory::Car);
    CHECK(semantic.archetype==world::VehicleArchetype::PoliceCruiser);
    CHECK(semantic.ownership.kind==world::VehicleOwnerKind::Police);
    CHECK_FALSE(semantic.ownership.faction);
    const auto player=createPlayerEntity(state,semantic.position);
    Simulation simulation;
    PlayerInputFrame enter; enter.enterVehiclePressed=true;
    simulation.tick(state,{state.clock.tick(),enter,{}});
    REQUIRE(state.vehicleSimulation.drivenVehicle==id);
    for (std::uint64_t tick=0;tick<20;++tick)
        simulation.tick(state,{state.clock.tick(),driveInput(tick),{}});
    CHECK(state.world.vehicles.get(id).ownership.kind==world::VehicleOwnerKind::Police);
    auto restored=loadGameState(saveGameState(state));
    CHECK(restored.vehicleSimulation.drivenVehicle==id);
    CHECK(restored.world.vehicles.get(id).archetype==world::VehicleArchetype::PoliceCruiser);
    CHECK(restored.world.vehicles.get(id).ownership==semantic.ownership);
    CHECK(restored.world.transforms.get(player).position==state.world.transforms.get(player).position);
    CHECK_NOTHROW(validateState(restored));
}

TEST_CASE("authentic v12 migration leaves the fleet empty without inventing vehicles") {
    const auto path=std::filesystem::path{FAULTLINE_TEST_SOURCE_DIR}/"tests/fixtures/save_v12_phase19.flsave";
    std::ifstream file{path,std::ios::binary}; REQUIRE(file.is_open());
    const SaveBytes oldBytes{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    REQUIRE(readSaveHeader(oldBytes).saveFormatVersion.value==12);
    auto state=loadGameState(oldBytes);
    CHECK(state.world.vehicles.entries().empty());
    CHECK(state.vehicleSimulation.version==currentVehicleSimulationVersion);
    CHECK_FALSE(state.vehicleSimulation.drivenVehicle);
    CHECK(state.vehicleSimulation.residents.empty());
    CHECK_NOTHROW(validateState(state));
    // The migration is deterministic and does not seed a fresh fleet or consume
    // gameplay RNG just because a v14 writer now exists.
    const auto migratedBytes=saveGameState(state);
    CHECK(readSaveHeader(migratedBytes).saveFormatVersion.value==14);
    const auto oldSections=sectionPayloads(oldBytes);
    const auto migratedSections=sectionPayloads(migratedBytes);
    for (const auto& [id,payload] : oldSections) {
        REQUIRE(migratedSections.contains(id));
        CHECK(migratedSections.at(id)==payload);
    }
    auto restored=loadGameState(migratedBytes);
    CHECK(restored.world.vehicles.entries().empty());
    CHECK(stateHash(restored)==stateHash(state));
    CHECK(saveGameState(restored)==migratedBytes);
}

TEST_CASE("v14 vehicle simulation rejects unsupported versions") {
    auto state=freshWorld();
    const auto bytes=saveGameState(state);
    auto malformed=bytes;
    detail::Reader reader{malformed}; reader.offset=48;
    bool found=false;
    while (reader.remaining()) {
        std::uint32_t id{},length{}; reader(id,length); reader.need(length);
        if (id==22) { malformed[reader.offset]=99; found=true; break; }
        reader.offset+=length;
    }
    REQUIRE(found);
    CHECK_THROWS((void)loadGameState(malformed));
}
