#include "simulation/history/history_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "persistence/save.hpp"
#include <charconv>
#include <iostream>
using namespace faultline;
using namespace faultline::simulation;
namespace {
std::uint64_t number(std::string_view s) {
    std::uint64_t n{}; auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),n);
    if (ec!=std::errc{} || end!=s.data()+s.size()) throw std::invalid_argument("Invalid number");
    return n;
}
}
int main(int argc,char** argv) {
    try {
        std::uint64_t first=12345,last=12345,origin=1; std::uint32_t months=48;
        bool fixture=false,replay=false,small=false; std::string load,save;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            const auto value=[&]() -> std::string_view { if (++i==argc) throw std::invalid_argument("Missing argument"); return argv[i]; };
            if (arg=="--seed") first=last=number(value());
            else if (arg=="--range") { first=number(value()); last=number(value()); }
            else if (arg=="--origin") origin=number(value());
            else if (arg=="--months") { const auto n=number(value()); if (n>48) throw std::invalid_argument("Months must be 0..48"); months=static_cast<std::uint32_t>(n); }
            else if (arg=="--load") load=value();
            else if (arg=="--save") save=value();
            else if (arg=="--fixture-prologue") fixture=true;
            else if (arg=="--replay") replay=true;
            else if (arg=="--small") small=true;
            else throw std::invalid_argument("Unknown history option");
        }
        if (first>last || last-first>9999 || (!load.empty() && first!=last)) throw std::invalid_argument("Invalid seed range");
        if (load.empty() && !fixture) throw std::invalid_argument("Tool generation requires explicit --fixture-prologue; runtime must supply real prologue evidence");
        std::uint64_t count=0,events=0,dealers=0,maxMarket=0,maxControl=0;
        core::StateHash batch;
        for (auto seed=first;;++seed) {
            world::WorldGenerationConfig config;
            if (small) { config.width=512; config.depth=512; config.waterPercent=0; }
            auto s=load.empty() ? GameState{WorldSeed{seed},config} : persistence::loadGameStateFile(load);
            if (load.empty()) {
                auto p=historyPrologueCandidate(s,FactionId{origin}); p.firstDeliveryCompleted=true; // Explicit test fixture only.
                beginStrategicHistory(s,p);
            }
            auto duplicate=s;
            while (s.history.months<months) advanceStrategicHistory(s);
            if (replay) {
                while (duplicate.history.months<months) advanceStrategicHistory(duplicate);
                core::require(stateHash(s)==stateHash(duplicate),"History replay mismatch");
            }
            validateState(s);
            if (s.history.months==48) validatePostTimeskipWorld(s);
            batch.u64(stateHash(s)); ++count; events+=s.history.events.size(); dealers+=s.marketSimulation.dealers.size();
            std::map<FactionId,std::uint64_t> supply; std::uint64_t total=0;
            for (const auto& [id,m] : s.marketSimulation.districts) { (void)id;
                for (const auto& [f,e] : m.factions) { supply[f]+=e.supply; total+=e.supply; }
            }
            for (const auto& [f,n] : s.world.factions.entries()) { (void)n;
                maxMarket=std::max(maxMarket,total ? supply[f]*10000/total : 0);
                maxControl=std::max(maxControl,controlledTerritoryCount(s,f)*10000/s.world.territories.entries().size());
            }
            if (first==last) {
                std::cout<<"Seed: "<<s.metadata.seed.value<<"\nHistoryVersion: "<<s.history.version<<"\nMonths: "<<s.history.months
                    <<"\nTick: "<<s.clock.tick()<<"\nDealers: "<<s.marketSimulation.dealers.size()<<"\nEvents: "<<s.history.events.size()
                    <<"\nMarket updates: "<<s.marketSimulation.updates<<"\nBaseWorldHash: "<<std::hex<<world::baseWorldHash(*s.world.base)
                    <<"\nStateHash: "<<stateHash(s)<<std::dec<<'\n';
            }
            if (!save.empty()) { core::require(first==last,"Save requires one seed"); persistence::saveGameStateFile(s,save); }
            if (seed==last) break;
        }
        std::cout<<"Worlds: "<<count<<"\nHistory events total: "<<events<<"\nDealers total: "<<dealers
            <<"\nLargest market share bps: "<<maxMarket<<"\nLargest territorial share bps: "<<maxControl
            <<"\nReplay: "<<(replay ? "PASS" : "not requested")<<"\nBatchHash: "<<std::hex<<batch.value()<<"\nValidation: PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
