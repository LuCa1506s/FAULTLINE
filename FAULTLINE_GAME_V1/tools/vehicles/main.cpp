#include "simulation/vehicles/vehicle_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include "core/rng/stable_hash.hpp"
#include "world/spatial/character_movement.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

using namespace faultline;
using namespace faultline::simulation;

namespace {
struct Options {
    std::uint64_t first{12345}, last{12345}, ticks{240};
    std::filesystem::path save;
};
std::uint64_t number(const char* text) {
    std::size_t used{}; const auto value=std::stoull(text,&used);
    if (!text[0] || text[used]) throw std::invalid_argument("invalid integer");
    return value;
}
Options parse(int argc,char** argv) {
    Options result;
    for (int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if (arg=="--seed"&&i+1<argc) result.first=result.last=number(argv[++i]);
        else if (arg=="--range"&&i+2<argc) { result.first=number(argv[++i]); result.last=number(argv[++i]); }
        else if (arg=="--ticks"&&i+1<argc) result.ticks=number(argv[++i]);
        else if (arg=="--save"&&i+1<argc) result.save=argv[++i];
        else throw std::invalid_argument("usage: faultline_vehicles [--seed N|--range A B] [--ticks N] [--save PATH]");
    }
    if (result.first>result.last || result.ticks<60 ||
        (!result.save.empty() && result.first!=result.last))
        throw std::invalid_argument("invalid options");
    return result;
}
std::string hex(std::uint64_t value) {
    std::ostringstream out; out<<std::hex<<std::setw(16)<<std::setfill('0')<<value;
    return out.str();
}
world::WorldPosition shifted(const GameState& state,world::WorldPosition position,double x,double z) {
    auto global=world::toGlobal(position,state.world.spatial.cellSize());
    global.x+=x; global.z+=z;
    return world::fromGlobal(global,state.world.spatial.cellSize());
}
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
// Bounded throttle cycle: accelerate, brake, reverse briefly, then repeat. It
// stays inside the verified clear road segment and exercises every speed sign.
PlayerInputFrame stressInput(std::uint64_t tick) {
    PlayerInputFrame input;
    input.moveZ=(tick/30)%2==0 ? 1.0F : -1.0F;
    return input;
}
VehiclePersistentId firstHealthy(const GameState& state) {
    for (const auto& [id,vehicle] : state.world.vehicles.entries())
        if (vehicle.condition.health.current>0) return id;
    return {};
}
struct Totals {
    std::uint64_t worlds{}, skipped{}, vehicles{}, policeVehicles{}, civilianVehicles{}, ticks{};
    std::uint64_t materialized{}, abstracted{}, entered{}, exited{}, collisions{};
    std::uint64_t saveMismatch{}, continuationMismatch{}, hashMismatch{}, identityMismatch{};
    std::uint64_t batch{core::stableHash("FAULTLINE.vehicles.batch.v1")};
};
void enter(GameState& state,Simulation& simulation,VehiclePersistentId target,EntityId player) {
    const auto position=state.world.vehicles.get(target).position;
    auto& transform=state.world.transforms.get(player);
    transform.position=position;
    state.world.spatial.upsert(player,position);
    PlayerInputFrame input; input.enterVehiclePressed=true;
    simulation.tick(state,{state.clock.tick(),input,{}});
}
std::optional<GameState> run(std::uint64_t seed,std::uint64_t ticks,Totals& totals) {
    GameState original{WorldSeed{seed}};
    // Geography can legitimately lack a generated fleet or a clear straight
    // segment. Skip such seeds rather than failing the whole mass-seed gate.
    if (original.world.vehicles.entries().empty()) return std::nullopt;
    const auto target=openVehicle(original,40.0);
    if (!target) return std::nullopt;
    const auto player=createPlayerEntity(original,original.world.vehicles.get(target).position);
    Simulation first;
    enter(original,first,target,player);
    core::require(original.vehicleSimulation.drivenVehicle==target,
        "Scripted scenario did not enter the selected vehicle");
    const auto split=ticks/2;
    for (std::uint64_t tick=0;tick<split;++tick)
        first.tick(original,{original.clock.tick(),stressInput(tick),{}});
    core::require(original.vehicleSimulation.drivenVehicle==target &&
        original.world.vehicles.get(target).condition.health.current>0,
        "Scripted vehicle did not survive the first half");
    const auto checkpoint=persistence::saveGameState(original);
    auto restored=persistence::loadGameState(checkpoint);
    if (stateHash(original)!=stateHash(restored)) ++totals.saveMismatch;
    if (persistence::saveGameState(restored)!=checkpoint) ++totals.saveMismatch;
    Simulation second;
    for (std::uint64_t tick=split;tick<ticks;++tick) {
        const auto input=stressInput(tick);
        first.tick(original,{original.clock.tick(),input,{}});
        second.tick(restored,{restored.clock.tick(),input,{}});
        if (stateHash(original)!=stateHash(restored)) {
            ++totals.hashMismatch; ++totals.continuationMismatch; break;
        }
    }
    if (persistence::saveGameState(original)!=persistence::saveGameState(restored))
        ++totals.continuationMismatch;
    core::require(original.vehicleSimulation.drivenVehicle==target,
        "Scripted vehicle did not survive the second half");

    PlayerInputFrame exit; exit.enterVehiclePressed=true;
    first.tick(original,{original.clock.tick(),exit,{}});
    core::require(!original.vehicleSimulation.drivenVehicle,"Scripted exit failed");

    VehiclePersistentId probe=target;
    if (original.world.vehicles.get(probe).condition.health.current<=0) probe=firstHealthy(original);
    core::require(static_cast<bool>(probe),"No healthy vehicle remains for streaming probe");
    if (!original.world.vehicles.get(probe).activeEntity)
        enter(original,first,probe,player); // materializes; the following tick exits
    if (original.vehicleSimulation.drivenVehicle) {
        PlayerInputFrame leave; leave.enterVehiclePressed=true;
        first.tick(original,{original.clock.tick(),leave,{}});
    }
    core::require(static_cast<bool>(original.world.vehicles.get(probe).activeEntity),
        "Streaming probe has no active body");
    const auto probeBody=original.world.vehicles.get(probe).activeEntity;
    const auto probePosition=original.world.transforms.get(probeBody).position;
    const auto fleetSize=original.world.vehicles.entries().size();

    const auto far=shifted(original,probePosition,20000,20000);
    original.world.transforms.get(player).position=far;
    original.world.spatial.upsert(player,far);
    first.tick(original,{original.clock.tick(),{},{}});
    if (original.world.vehicles.get(probe).activeEntity ||
        original.world.vehicles.get(probe).position!=probePosition ||
        original.world.vehicles.entries().size()!=fleetSize) ++totals.identityMismatch;

    const auto back=original.world.vehicles.get(probe).position;
    original.world.transforms.get(player).position=back;
    original.world.spatial.upsert(player,back);
    first.tick(original,{original.clock.tick(),{},{}});
    const auto& rematerialized=original.world.vehicles.get(probe);
    if (!rematerialized.activeEntity ||
        original.world.vehicleLinks.get(rematerialized.activeEntity).vehicle!=probe ||
        original.world.vehicles.entries().size()!=fleetSize) ++totals.identityMismatch;
    validateState(original);

    std::uint64_t police=0;
    for (const auto& [id,vehicle] : original.world.vehicles.entries()) {
        (void)id;
        if (vehicle.ownership.kind==world::VehicleOwnerKind::Police) ++police;
    }
    ++totals.worlds;
    totals.vehicles+=original.world.vehicles.entries().size();
    totals.policeVehicles+=police;
    totals.civilianVehicles+=original.world.vehicles.entries().size()-police;
    totals.ticks+=ticks;
    totals.materialized+=original.vehicleSimulation.statistics.materialized;
    totals.abstracted+=original.vehicleSimulation.statistics.abstracted;
    totals.entered+=original.vehicleSimulation.statistics.entered;
    totals.exited+=original.vehicleSimulation.statistics.exited;
    totals.collisions+=original.vehicleSimulation.statistics.collisions;
    totals.batch=core::deriveSeed(totals.batch,std::to_string(stateHash(original)));
    return original;
}
}
int main(int argc,char** argv) try {
    const auto options=parse(argc,argv);
    Totals totals;
    std::optional<GameState> last;
    for (auto seed=options.first;;++seed) {
        auto result=run(seed,options.ticks,totals);
        if (result) last=std::move(result);
        else ++totals.skipped;
        if (seed==options.last) break;
    }
    if (!last) {
        std::cerr<<"faultline_vehicles: no seed in range produced a runnable scenario\n";
        return 2;
    }
    if (!options.save.empty()) persistence::saveGameStateFile(*last,options.save);
    const auto& state=*last;
    std::cout<<"FAULTLINE VEHICLES\nSeeds: "<<options.first<<".."<<options.last
        <<"\nTicks: "<<options.ticks<<"\nWorlds: "<<totals.worlds
        <<"\nSkipped seeds: "<<totals.skipped
        <<"\nVehicles: "<<totals.vehicles<<"\nPolice vehicles: "<<totals.policeVehicles
        <<"\nCivilian vehicles: "<<totals.civilianVehicles
        <<"\nMaterialized: "<<totals.materialized<<"\nAbstracted: "<<totals.abstracted
        <<"\nEntered: "<<totals.entered<<"\nExited: "<<totals.exited
        <<"\nCollisions: "<<totals.collisions
        <<"\nSave mismatches: "<<totals.saveMismatch
        <<"\nContinuation mismatches: "<<totals.continuationMismatch
        <<"\nStateHash mismatches: "<<totals.hashMismatch
        <<"\nIdentity mismatches: "<<totals.identityMismatch
        <<"\nDriven state: "<<static_cast<bool>(state.vehicleSimulation.drivenVehicle)
        <<"\nStateHash: "<<hex(stateHash(state))
        <<"\nBatchHash: "<<hex(totals.batch)
        <<"\nValidation: "<<((totals.saveMismatch|totals.continuationMismatch|
            totals.hashMismatch|totals.identityMismatch)==0?"PASS":"FAIL")<<'\n';
    return (totals.saveMismatch|totals.continuationMismatch|totals.hashMismatch|
        totals.identityMismatch)?1:0;
} catch (const std::exception& error) {
    std::cerr<<"faultline_vehicles: "<<error.what()<<'\n'; return 2;
}
