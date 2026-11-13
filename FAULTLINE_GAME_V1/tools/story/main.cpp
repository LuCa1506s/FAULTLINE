#include "simulation/story/story_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/market/market_system.hpp"
#include "persistence/save.hpp"
#include <charconv>
#include <iostream>
using namespace faultline;
using namespace faultline::simulation;
namespace {
std::uint64_t number(std::string_view s) {
    std::uint64_t n{};const auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),n);
    if (ec!=std::errc{} || end!=s.data()+s.size()) throw std::invalid_argument("Invalid number");
    return n;
}
}
int main(int argc,char** argv) {
    try {
        std::uint64_t first=12345,last=12345;bool fixture=false,small=false;std::string load,save;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            const auto value=[&]() -> std::string_view { if (++i==argc) throw std::invalid_argument("Missing argument");return argv[i]; };
            if (arg=="--seed") first=last=number(value());
            else if (arg=="--range") { first=number(value());last=number(value()); }
            else if (arg=="--fixture-prologue") fixture=true;
            else if (arg=="--small") small=true;
            else if (arg=="--load") load=value();
            else if (arg=="--save") save=value();
            else throw std::invalid_argument("Unknown story option");
        }
        core::require(first<=last && last-first<10000 && (load.empty() || first==last),"Invalid range");
        core::require(!load.empty() || fixture,"Fresh tool worlds require --fixture-prologue; this is not FIRST DELIVERY runtime");
        std::uint64_t worlds=0,nodes=0,edges=0,eligible=0,empty=0,contacts=0,coverage=0,repaired=0,invalidated=0;
        std::size_t minimum=4096,maximum=0;core::StateHash batch;
        for (auto seed=first;;++seed) {
            world::WorldGenerationConfig config;if (small) {config.width=512;config.depth=512;config.waterPercent=0;}
            auto s=load.empty() ? GameState{WorldSeed{seed},config} : persistence::loadGameStateFile(load);
            if (load.empty()) {
                auto evidence=historyPrologueCandidate(s,FactionId{1});evidence.firstDeliveryCompleted=true;
                beginStrategicHistory(s,evidence);while (s.history.months<48) advanceStrategicHistory(s);
            }
            auto repeat=s;
            if (!s.story.version) initializeStoryGraph(s);
            if (!repeat.story.version) initializeStoryGraph(repeat);
            core::require(persistence::saveGameState(s)==persistence::saveGameState(repeat),"Story replay mismatch");
            const auto bytes=persistence::saveGameState(s);auto restored=persistence::loadGameState(bytes);
            core::require(stateHash(s)==stateHash(restored) && persistence::saveGameState(restored)==bytes,"Story round-trip mismatch");
            std::set<NpcId> refs;std::set<FactionId> factions;std::uint64_t viable=0;
            for (const auto& [id,n] : s.story.nodes.entries()) {
                edges+=n.prerequisites.size();factions.insert(n.faction);
                if (n.contact && s.world.npcs.get(n.contact).major) refs.insert(n.contact);
                if (storyAvailability(s,id)==StoryAvailability::Eligible) ++viable;
            }
            nodes+=s.story.nodes.entries().size();minimum=std::min(minimum,s.story.nodes.entries().size());maximum=std::max(maximum,s.story.nodes.entries().size());
            eligible+=viable;if (!viable) ++empty;contacts+=refs.size();coverage+=factions.size();++worlds;batch.u64(stateHash(s));
            if (first==last) std::cout<<"Seed: "<<s.metadata.seed.value<<"\nStoryGraphVersion: "<<s.story.version
                <<"\nNodes: "<<s.story.nodes.entries().size()<<"\nEligible: "<<viable<<"\nStateHash: "<<std::hex<<stateHash(s)<<std::dec<<'\n';
            if (!save.empty()) {core::require(first==last,"Save requires one world");persistence::saveGameStateFile(s,save);}
            // Deterministic mutation on a copy; no changes to the saved world.
            NpcEventBuffer events;NpcId victim;
            for (const auto& [id,n] : s.story.nodes.entries()) {
                (void)id;if (n.kind==StoryKind::DealerConflict && n.contact && s.world.npcs.get(n.contact).major) {victim=n.contact;break;}
            }
            if (victim) {
                setNpcLifecycle(s,victim,world::NpcLifecycle::Dead,events);refreshMarkets(s);repairStoryGraph(s);validateState(s);
                for (const auto& [id,n] : s.story.nodes.entries()) {
                    if (repeat.story.nodes.get(id).contact==victim && n.contact!=victim) ++repaired;
                    if (storyAvailability(s,id)==StoryAvailability::Invalidated) ++invalidated;
                }
            }
            if (seed==last) break;
        }
        std::cout<<"Worlds: "<<worlds<<"\nNodes total/min/max: "<<nodes<<'/'<<minimum<<'/'<<maximum
            <<"\nDependencies: "<<edges<<"\nEligible total: "<<eligible<<"\nNo viable possibility: "<<empty
            <<"\nReferenced major NPCs total: "<<contacts<<"\nFaction coverage total: "<<coverage
            <<"\nDeath contact repairs: "<<repaired<<"\nInvalidated after mutations: "<<invalidated
            <<"\nBatchHash: "<<std::hex<<batch.value()<<"\nValidation: PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
