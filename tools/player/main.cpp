#include "simulation/player/player_system.hpp"
#include "simulation/player/player_script.hpp"
#include "persistence/save.hpp"
#include <charconv>
#include <iostream>

using namespace faultline;
using namespace faultline::simulation;
namespace {
std::uint64_t number(std::string_view s) {
    std::uint64_t n{}; const auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),n);
    core::require(ec==std::errc{} && end==s.data()+s.size(),"Invalid number"); return n;
}
}
int main(int argc,char** argv) {
    try {
        std::uint64_t first=12345,last=12345,ticks=12000;
        std::string save;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            const auto value=[&]() -> std::string_view { core::require(++i<argc,"Missing argument"); return argv[i]; };
            if (arg=="--seed") first=last=number(value());
            else if (arg=="--range") { first=number(value()); last=number(value()); }
            else if (arg=="--ticks") ticks=number(value());
            else if (arg=="--save") save=value();
            else throw std::invalid_argument("Unknown player option");
        }
        core::require(first<=last && last-first<1000 && ticks>=2 && ticks<=100000,"Invalid range/duration");
        core::require(save.empty() || first==last,"Save requires single seed");
        std::uint64_t worlds=0,crossings=0; core::StateHash batch;
        for (auto seed=first;;++seed) {
            GameState a{WorldSeed{seed}};
            const auto spawn=a.world.npcs.entries().begin()->second.position;
            const auto entity=createPlayerEntity(a,spawn);
            Simulation uninterrupted,resumed;
            for (std::uint64_t t=0;t<ticks/2;++t) {
                const auto before=a.world.transforms.get(entity).position.cell;
                uninterrupted.tick(a,{a.clock.tick(),playerVerificationInput(t),{}});
                crossings+=a.world.transforms.get(entity).position.cell!=before;
            }
            const auto boundary=persistence::saveGameState(a);
            auto b=persistence::loadGameState(boundary);
            core::require(stateHash(a)==stateHash(b) && persistence::saveGameState(b)==boundary,"Boundary mismatch");
            for (auto t=ticks/2;t<ticks;++t) {
                const auto input=playerVerificationInput(t);
                const auto before=a.world.transforms.get(entity).position.cell;
                uninterrupted.tick(a,{a.clock.tick(),input,{}});
                resumed.tick(b,{b.clock.tick(),input,{}});
                crossings+=a.world.transforms.get(entity).position.cell!=before;
                core::require(a.player==b.player && a.world.transforms.get(entity).position==b.world.transforms.get(entity).position &&
                    a.world.characterBodies.get(entity)==b.world.characterBodies.get(entity) &&
                    a.world.entities.nextId()==b.world.entities.nextId() && a.world.entities.entries()==b.world.entities.entries(),"First player/entity continuation mismatch");
                for (std::size_t d=0;d<a.randomness.streams.size();++d)
                    core::require(a.randomness.streams[d].state()==b.randomness.streams[d].state(),"RNG continuation mismatch");
                if (t%256==0) core::require(stateHash(a)==stateHash(b),"Semantic continuation mismatch");
            }
            validateState(a);validateState(b);
            core::require(stateHash(a)==stateHash(b) && persistence::saveGameState(a)==persistence::saveGameState(b),"Final save/hash mismatch");
            const auto pos=a.world.transforms.get(entity).position;
            core::require(world::normalize(pos,a.world.spatial.cellSize())==pos,"Invalid position");
            core::require(a.world.characterBodies.contains(entity) &&
                a.world.characterBodies.entries().size()==1+a.player.residents.size(),
                "Duplicate/lost physical character body");
            if (first==last) {
                const auto& body=a.world.characterBodies.get(entity);
                std::cout<<"Seed: "<<seed<<"\nEntity: "<<entity.value()<<"\nCell: "<<pos.cell.x<<','<<pos.cell.z
                    <<"\nLocal: "<<pos.local.x<<','<<pos.local.y<<','<<pos.local.z<<"\nYaw: "<<body.yaw
                    <<"\nSpeed: "<<body.movementSpeed<<"\nStance height: "<<body.stanceHeight
                    <<"\nStateHash: "<<std::hex<<stateHash(a)<<std::dec<<'\n';
            }
            if (!save.empty()) persistence::saveGameStateFile(a,save);
            batch.u64(stateHash(a)); ++worlds;
            if (seed==last) break;
        }
        std::cout<<"Worlds: "<<worlds<<"\nPlayers initialized: "<<worlds<<"\nMovement scripts: "<<worlds
            <<"\nTicks per script: "<<ticks<<"\nCell crossings: "<<crossings
            <<"\nInvalid positions/duplicate players/lost components: 0/0/0\nSave/continuation/hash mismatches: 0/0/0"
            <<"\nBatchHash: "<<std::hex<<batch.value()<<std::dec<<"\nValidation: PASS\n";
    } catch (const std::exception& e) { std::cerr<<"FAULTLINE PLAYER: "<<e.what()<<'\n';return 1; }
}
