#include "simulation/police/police_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include "core/rng/stable_hash.hpp"
#include "world/spatial/streaming.hpp"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

using namespace faultline;
using namespace faultline::simulation;
namespace {
struct Options { std::uint64_t first{12345},last{12345},ticks{240}; std::filesystem::path save; };
std::uint64_t number(const char* text){std::size_t used{};auto value=std::stoull(text,&used);if(!text[0]||text[used])throw std::invalid_argument("invalid integer");return value;}
Options parse(int argc,char** argv){Options result;for(int i=1;i<argc;++i){const std::string arg=argv[i];
    if(arg=="--seed"&&i+1<argc)result.first=result.last=number(argv[++i]);
    else if(arg=="--range"&&i+2<argc){result.first=number(argv[++i]);result.last=number(argv[++i]);}
    else if(arg=="--ticks"&&i+1<argc)result.ticks=number(argv[++i]);
    else if(arg=="--save"&&i+1<argc)result.save=argv[++i];
    else throw std::invalid_argument("usage: faultline_police [--seed N|--range A B] [--ticks N] [--save PATH]");}
    if(result.first>result.last||result.ticks<120||(!result.save.empty()&&result.first!=result.last)) {
        throw std::invalid_argument("invalid options");
    }
    return result;
}
std::string hex(std::uint64_t value){std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<value;return out.str();}
world::WorldPosition shift(const GameState& state,world::WorldPosition position,double x,double z=0){auto p=world::toGlobal(position,state.world.spatial.cellSize());p.x+=x;p.z+=z;return world::fromGlobal(p,state.world.spatial.cellSize());}
EntityId activate(GameState& state,NpcId id,world::WorldPosition position){auto& npc=state.world.npcs.get(id);npc.semantic.travel.reset();npc.position=position;const auto entity=world::materializeNpc(state.world,id);state.world.transforms.get(entity).position=position;state.world.spatial.upsert(entity,position);state.player.residents.push_back({id,entity});std::sort(state.player.residents.begin(),state.player.residents.end());return entity;}
struct Totals {std::uint64_t worlds{},crimes{},unwitnessed{},witnessed{},reports{},transitions{},searchTransitions{},pursuitTransitions{},responses{},reacquisitions{},arrests{},duplicates{},saveMismatch{},continuationMismatch{},hashMismatch{};std::uint64_t batch{core::stableHash("FAULTLINE.police.batch.v1")};};
GameState run(std::uint64_t seed,std::uint64_t ticks,Totals& totals){
    GameState original{WorldSeed{seed}}; std::vector<NpcId> actors;
    for(const auto& [id,npc]:original.world.npcs.entries())if(!npc.roles.has(world::NpcRole::FactionLeader)){actors.push_back(id);if(actors.size()==4)break;}
    core::require(actors.size()==4,"Police scenario needs four non-leader NPCs");
    const auto origin=original.world.npcs.get(actors[0]).position;createPlayerEntity(original,origin);
    const auto victimEntity=activate(original,actors[0],shift(original,origin,2));
    activate(original,actors[1],shift(original,origin,-2));activate(original,actors[2],shift(original,origin,0,2));activate(original,actors[3],shift(original,origin,0,-2));
    NpcEventBuffer npcEvents;CombatEventBuffer combat;PoliceEventBuffer policeEvents;
    const auto shot=resolveCombatIntent(original,{{CombatActorKind::Player,original.player.controlledEntity,{}},{CombatActorKind::Npc,victimEntity,actors[0]}},npcEvents);
    core::require(shot.result==CombatResult::Hit,"Scripted police scenario attack did not hit");combat.push(shot);
    ingestCombatCrimes(original,combat,policeEvents);observeNewCrimes(original,policeEvents);
    (void)recordCrime(original,CrimeType::WeaponDischarge,{CrimeActorKind::Player,{}},{},
        shift(original,origin,500,500),1500,CrimeObservability::VisualAndAudible,policeEvents);
    observeNewCrimes(original,policeEvents);
    const auto split=ticks/2;Simulation first;for(std::uint64_t i=0;i<split;++i)first.tick(original,{original.clock.tick(),{}, {}});
    const auto checkpoint=persistence::saveGameState(original);auto restored=persistence::loadGameState(checkpoint);
    if(stateHash(original)!=stateHash(restored)||persistence::saveGameState(restored)!=checkpoint)++totals.saveMismatch;
    Simulation second;
    for(std::uint64_t i=split;i<ticks;++i){first.tick(original,{original.clock.tick(),{}, {}});second.tick(restored,{restored.clock.tick(),{}, {}});if(stateHash(original)!=stateHash(restored)){++totals.hashMismatch;++totals.continuationMismatch;break;}}
    if(persistence::saveGameState(original)!=persistence::saveGameState(restored))++totals.continuationMismatch;
    validateState(original);
    std::vector<CrimeEventId> observed;
    for (const auto& value:original.police.observations) observed.push_back(value.crime);
    std::sort(observed.begin(),observed.end()); observed.erase(std::unique(observed.begin(),observed.end()),observed.end());
    ++totals.worlds;totals.crimes+=original.police.statistics.crimes;totals.witnessed+=observed.size();
    totals.unwitnessed+=original.police.crimes.size()-observed.size();totals.reports+=original.police.statistics.reportsDelivered;totals.transitions+=original.police.statistics.wantedTransitions;totals.searchTransitions+=original.police.statistics.searchTransitions;totals.pursuitTransitions+=original.police.statistics.pursuitTransitions;totals.responses+=original.police.statistics.responses;totals.reacquisitions+=original.police.statistics.reacquisitions;totals.arrests+=original.police.statistics.arrests;totals.duplicates+=original.police.statistics.duplicateFacts;totals.batch=core::deriveSeed(totals.batch,std::to_string(stateHash(original)));return original;
}
}
int main(int argc,char** argv)try{const auto options=parse(argc,argv);Totals totals;std::optional<GameState> last;for(auto seed=options.first;;++seed){last=run(seed,options.ticks,totals);if(seed==options.last)break;}if(!options.save.empty())persistence::saveGameStateFile(*last,options.save);
    const auto& state=*last;std::cout<<"FAULTLINE WITNESS + POLICE\nSeeds: "<<options.first<<".."<<options.last<<"\nTicks: "<<options.ticks<<"\nWorlds: "<<totals.worlds<<"\nCrimes: "<<totals.crimes<<"\nUnwitnessed crimes: "<<totals.unwitnessed<<"\nWitnessed crimes: "<<totals.witnessed<<"\nReports: "<<totals.reports<<"\nWanted transitions: "<<totals.transitions<<"\nSearch transitions: "<<totals.searchTransitions<<"\nPursuit transitions: "<<totals.pursuitTransitions<<"\nPolice responses: "<<totals.responses<<"\nReacquisitions: "<<totals.reacquisitions<<"\nArrests: "<<totals.arrests<<"\nDuplicate facts: "<<totals.duplicates<<"\nSave mismatches: "<<totals.saveMismatch<<"\nContinuation mismatches: "<<totals.continuationMismatch<<"\nStateHash mismatches: "<<totals.hashMismatch<<"\nWanted state: "<<static_cast<unsigned>(state.police.wanted.state)<<"\nResponders: "<<state.police.responders.size()<<"\nStateHash: "<<hex(stateHash(state))<<"\nBatchHash: "<<hex(totals.batch)<<"\nValidation: "<<((totals.saveMismatch|totals.continuationMismatch|totals.hashMismatch)==0?"PASS":"FAIL")<<'\n';return (totals.saveMismatch|totals.continuationMismatch|totals.hashMismatch)?1:0;
}catch(const std::exception& error){std::cerr<<"faultline_police: "<<error.what()<<'\n';return 2;}
