#include "simulation/combat/combat_system.hpp"
#include "simulation/player/player_system.hpp"
#include "persistence/save.hpp"
#include "core/rng/stable_hash.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

using namespace faultline; using namespace faultline::simulation;
namespace {
struct Options{std::uint64_t first{12345},last{12345},rounds{6};std::filesystem::path save;};
std::uint64_t number(const char* s){std::size_t n{};auto v=std::stoull(s,&n);if(!s[0]||s[n])throw std::invalid_argument("invalid integer");return v;}
Options parse(int argc,char** argv){Options o;for(int i=1;i<argc;++i){std::string a=argv[i];
 if(a=="--seed"&&i+1<argc)o.first=o.last=number(argv[++i]);else if(a=="--range"&&i+2<argc){o.first=number(argv[++i]);o.last=number(argv[++i]);}
 else if(a=="--rounds"&&i+1<argc)o.rounds=number(argv[++i]);else if(a=="--save"&&i+1<argc)o.save=argv[++i];else throw std::invalid_argument("usage: faultline_combat [--seed N|--range A B] [--rounds N] [--save path]");}
 if(o.first>o.last||!o.rounds||(!o.save.empty()&&o.first!=o.last))throw std::invalid_argument("invalid options");
 return o;}
std::string hex(std::uint64_t v){std::ostringstream o;o<<std::hex<<std::setw(16)<<std::setfill('0')<<v;return o.str();}
EntityId activate(GameState& s,NpcId id,double offset){auto& n=s.world.npcs.get(id);auto p=world::toGlobal(s.world.transforms.get(s.player.controlledEntity).position);p.x+=offset;n.position=world::fromGlobal(p);n.semantic.travel.reset();n.semantic.activity=world::NpcActivity::Idle;
 const auto e=s.world.entities.create();s.world.transforms.set(s.world.entities,e,{n.position});s.world.health.set(s.world.entities,e,n.health);s.world.characterBodies.set(s.world.entities,e,{0,3.5F,1.75F});s.world.npcLinks.set(s.world.entities,e,{id});s.world.spatial.upsert(e,n.position);n.activeEntity=e;s.player.residents.push_back({id,e});std::sort(s.player.residents.begin(),s.player.residents.end());return e;}
struct Totals{std::uint64_t worlds{},scenarios{},attempts{},hits{},misses{},deaths{},saveMismatch{},continuationMismatch{},hashMismatch{};std::uint64_t batch{core::stableHash("FAULTLINE.combat.batch.v1")};};
GameState run(std::uint64_t seed,std::uint64_t rounds,Totals& totals){GameState a{WorldSeed{seed}};createPlayerEntity(a,a.world.npcs.entries().begin()->second.position);
 std::vector<NpcId> targets;for(const auto& [id,n]:a.world.npcs.entries())if(!n.roles.has(world::NpcRole::FactionLeader)){activate(a,id,2.0+static_cast<double>(targets.size())*1.5);targets.push_back(id);if(targets.size()==3)break;}
 auto& pw=a.combat.weapons.get(a.combat.player.equipped);pw.definition=weaponDefinitionId(WeaponArchetype::Smg);pw.ammoInMagazine=30;pw.reserveAmmo=90;
 std::optional<GameState>b;NpcEventBuffer ea,eb;for(std::uint64_t i=0;i<rounds;++i){auto id=targets[static_cast<std::size_t>(i%targets.size())];
  if(a.world.npcs.get(id).lifecycle==world::NpcLifecycle::Dead)continue;
  a.clock=core::SimulationClock{i*3};auto ra=resolveCombatIntent(a,{{CombatActorKind::Player,a.player.controlledEntity,{}},{CombatActorKind::Npc,a.world.npcs.get(id).activeEntity,id}},ea);
  if(ra.result==CombatResult::Hit)++totals.hits;else if(ra.result==CombatResult::MissBlocked||ra.result==CombatResult::MissOutOfRange)++totals.misses;if(ra.death)++totals.deaths;++totals.attempts;
  if(i==rounds/2){auto bytes=persistence::saveGameState(a);b.emplace(persistence::loadGameState(bytes));if(stateHash(a)!=stateHash(*b)||persistence::saveGameState(*b)!=bytes)++totals.saveMismatch;restorePlayerResidency(*b);}
  else if(b){auto& bn=b->world.npcs.get(id);if(bn.lifecycle!=world::NpcLifecycle::Dead){b->clock=core::SimulationClock{i*3};auto rb=resolveCombatIntent(*b,{{CombatActorKind::Player,b->player.controlledEntity,{}},{CombatActorKind::Npc,bn.activeEntity,id}},eb);if(rb.result!=ra.result||rb.damage!=ra.damage||rb.death!=ra.death)++totals.continuationMismatch;}if(stateHash(a)!=stateHash(*b))++totals.hashMismatch;}
 }
 validateState(a);++totals.worlds;++totals.scenarios;totals.batch=core::deriveSeed(totals.batch,std::to_string(stateHash(a)));return a;}
}
int main(int argc,char**argv)try{const auto o=parse(argc,argv);Totals t;std::optional<GameState> last;for(auto seed=o.first;;++seed){last=run(seed,o.rounds,t);if(seed==o.last)break;}if(!o.save.empty())persistence::saveGameStateFile(*last,o.save);
 std::cout<<"FAULTLINE COMBAT\nSeeds: "<<o.first<<".."<<o.last<<"\nRounds: "<<o.rounds<<"\nWorlds: "<<t.worlds<<"\nScenarios: "<<t.scenarios<<"\nAttack attempts: "<<t.attempts<<"\nHits: "<<t.hits<<"\nMisses: "<<t.misses<<"\nDeaths: "<<t.deaths<<"\nSave mismatches: "<<t.saveMismatch<<"\nContinuation mismatches: "<<t.continuationMismatch<<"\nStateHash mismatches: "<<t.hashMismatch<<"\nStateHash: "<<hex(stateHash(*last))<<"\nBatchHash: "<<hex(t.batch)<<"\nValidation: "<<((t.saveMismatch|t.continuationMismatch|t.hashMismatch)==0?"PASS":"FAIL")<<'\n';return (t.saveMismatch|t.continuationMismatch|t.hashMismatch)?1:0;
}catch(const std::exception&e){std::cerr<<"faultline_combat: "<<e.what()<<'\n';return 2;}
