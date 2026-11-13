#include "simulation/runtime/game.hpp"
#include "simulation/player/player_system.hpp"
#include "simulation/npc/character_ai.hpp"
#include "persistence/save.hpp"
#include "core/rng/stable_hash.hpp"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

using namespace faultline;
using namespace faultline::simulation;
using namespace faultline::persistence;

namespace {
struct Options {
    std::uint64_t first{12345}, last{12345}, ticks{2000};
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
        const std::string argument=argv[i];
        if (argument=="--seed" && i+1<argc) result.first=result.last=number(argv[++i]);
        else if (argument=="--range" && i+2<argc) { result.first=number(argv[++i]);result.last=number(argv[++i]); }
        else if (argument=="--ticks" && i+1<argc) result.ticks=number(argv[++i]);
        else if (argument=="--save" && i+1<argc) result.save=argv[++i];
        else throw std::invalid_argument("usage: faultline_characters [--seed N|--range A B] [--ticks N] [--save path]");
    }
    if (result.first>result.last || !result.ticks) throw std::invalid_argument("invalid range/ticks");
    if (!result.save.empty() && result.first!=result.last) throw std::invalid_argument("--save requires one seed");
    return result;
}

world::WorldLocation otherTerritory(const GameState& state,NpcId npc) {
    const auto current=state.world.npcs.get(npc).location.territory;
    for (const auto& territory : state.world.base->territories)
        if (territory.id!=current) return {territory.district,territory.id,{}};
    return state.world.npcs.get(npc).location;
}

void focus(GameState& state,world::WorldPosition position) {
    auto& transform=state.world.transforms.get(state.player.controlledEntity);
    transform.position=world::normalize(position,state.world.spatial.cellSize());
    state.world.spatial.upsert(state.player.controlledEntity,transform.position);
}

struct Totals {
    std::uint64_t worlds{},npcs{},movements{},crossings{},abstractions{},rematerializations{};
    std::uint64_t invalidLinks{},duplicates{},saveMismatches{},continuationMismatches{},hashMismatches{};
    std::uint64_t batchHash{core::stableHash("FAULTLINE.characters.batch.v1")};
};

std::string hex(std::uint64_t value) {
    std::ostringstream out; out<<std::hex<<std::setw(16)<<std::setfill('0')<<value; return out.str();
}

GameState run(std::uint64_t seed,std::uint64_t ticks,Totals& totals) {
    GameState original{WorldSeed{seed}};
    original.factionSimulation.nextEvaluationTick=neverTick;
    original.marketSimulation.nextUpdateTick=neverTick;
    const auto selected=original.world.npcs.entries().begin()->first;
    const auto initial=original.world.npcs.get(selected).position;
    createPlayerEntity(original,initial);
    Simulation uninterrupted;
    uninterrupted.tick(original,{original.clock.tick(),{}, {}});
    for (const auto& resident : original.player.residents) {
        auto& npc=original.world.npcs.get(resident.npc);
        npc.semantic.activity=world::NpcActivity::Hiding;
        npc.semantic.home=otherTerritory(original,resident.npc);
    }
    totals.npcs+=original.player.residents.size();

    const auto saveTick=std::max<std::uint64_t>(2,ticks/2);
    std::optional<GameState> restored;
    Simulation resumed;
    std::map<NpcId,world::CellCoord> cells;
    for (const auto& resident : original.player.residents)
        cells[resident.npc]=original.world.transforms.get(resident.entity).position.cell;
    bool away=false;
    for (std::uint64_t step=1;step<ticks;++step) {
        if (step==ticks/4) {
            auto distant=initial;distant.local.x+=900.0F;focus(original,distant);
            if (restored) focus(*restored,distant);
            away=true;
        }
        if (step==ticks/4+20) {
            focus(original,initial);if (restored) focus(*restored,initial);away=false;
        }
        const auto before=original.player.residents.size();
        uninterrupted.tick(original,{original.clock.tick(),{}, {}});
        if (before && original.player.residents.empty()) ++totals.abstractions;
        if (!before && !original.player.residents.empty() && !away) ++totals.rematerializations;
        for (const auto& event : uninterrupted.characterAiEvents().values()) if (event.moved) ++totals.movements;
        for (const auto& resident : original.player.residents) {
            const auto cell=original.world.transforms.get(resident.entity).position.cell;
            const auto found=cells.find(resident.npc);
            if (found!=cells.end() && found->second!=cell) ++totals.crossings;
            cells[resident.npc]=cell;
        }
        if (step==saveTick) {
            const auto bytes=saveGameState(original);
            restored.emplace(loadGameState(bytes));
            if (stateHash(*restored)!=stateHash(original) || saveGameState(*restored)!=bytes) ++totals.saveMismatches;
            continue;
        }
        if (restored) {
            resumed.tick(*restored,{restored->clock.tick(),{}, {}});
            if (stateHash(*restored)!=stateHash(original)) ++totals.hashMismatches;
            if (restored->player.residents!=original.player.residents ||
                restored->world.entities.nextId()!=original.world.entities.nextId()) ++totals.continuationMismatches;
        }
    }
    validateState(original);
    std::set<EntityId> bodies;
    for (const auto& resident : original.player.residents) {
        const auto& npc=original.world.npcs.get(resident.npc);
        if (npc.activeEntity!=resident.entity || !original.world.npcLinks.contains(resident.entity) ||
            original.world.npcLinks.get(resident.entity).npc!=resident.npc) ++totals.invalidLinks;
        if (!bodies.insert(resident.entity).second) ++totals.duplicates;
    }
    if (restored && saveGameState(*restored)!=saveGameState(original)) ++totals.continuationMismatches;
    ++totals.worlds;
    totals.batchHash=core::deriveSeed(totals.batchHash,std::to_string(stateHash(original)));
    return original;
}
}

int main(int argc,char** argv) try {
    const auto options=parse(argc,argv);Totals totals;std::optional<GameState> final;
    for (auto seed=options.first;;++seed) {
        final=run(seed,options.ticks,totals);
        if (seed==options.last) break;
    }
    if (!options.save.empty()) saveGameStateFile(*final,options.save);
    const auto& npc=final->world.npcs.entries().begin()->second;
    std::cout<<"FAULTLINE CHARACTER AI\n"
             <<"Seeds: "<<options.first<<".."<<options.last<<"\n"
             <<"Ticks per world: "<<options.ticks<<"\n"
             <<"Worlds: "<<totals.worlds<<"\nNPCs activated: "<<totals.npcs
             <<"\nPhysical movements: "<<totals.movements<<"\nCell crossings: "<<totals.crossings
             <<"\nAbstractions: "<<totals.abstractions<<"\nRematerializations: "<<totals.rematerializations
             <<"\nInvalid links: "<<totals.invalidLinks<<"\nDuplicate bodies: "<<totals.duplicates
             <<"\nSave mismatches: "<<totals.saveMismatches
             <<"\nContinuation mismatches: "<<totals.continuationMismatches
             <<"\nStateHash mismatches: "<<totals.hashMismatches
             <<"\nObserved NpcId: "<<final->world.npcs.entries().begin()->first.value()
             <<"\nObserved EntityId: "<<npc.activeEntity.value()
             <<"\nStateHash: "<<hex(stateHash(*final))
             <<"\nBatchHash: "<<hex(totals.batchHash)<<"\nValidation: "
             <<((totals.invalidLinks|totals.duplicates|totals.saveMismatches|totals.continuationMismatches|totals.hashMismatches)==0?"PASS":"FAIL")<<'\n';
    return (totals.invalidLinks|totals.duplicates|totals.saveMismatches|totals.continuationMismatches|totals.hashMismatches)==0?0:1;
} catch (const std::exception& error) {
    std::cerr<<"faultline_characters: "<<error.what()<<'\n';return 2;
}
