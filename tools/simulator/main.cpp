#include "simulation/runtime/game.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "persistence/save.hpp"
#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>

namespace {
using namespace faultline;
using namespace faultline::simulation;
std::uint64_t number(std::string_view text) {
    std::uint64_t value{}; const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("Invalid unsigned integer");
    return value;
}
struct Statistics {
    std::uint64_t operations{}, attacks{}, defenses{}, resolved{}, failed{}, cancelled{}, live{};
    std::uint64_t manpowerLoss{}, treasurySpent{}, commitmentPeak{}, resourceExhausted{};
    NpcStatistics npcs;
    std::uint64_t controlChanges{}, contests{}, uncontrolledTransitions{}, transitions{};
    std::uint64_t zeroTerritoryFactions{}, staticWorlds{}, minChanges{neverTick}, maxChanges{}, largestShareBps{};
};
void accumulate(Statistics& a,const Statistics& b) {
    a.controlChanges+=b.controlChanges; a.contests+=b.contests; a.uncontrolledTransitions+=b.uncontrolledTransitions; a.transitions+=b.transitions;
    a.zeroTerritoryFactions+=b.zeroTerritoryFactions; a.staticWorlds+=b.staticWorlds;
    a.minChanges=std::min(a.minChanges,b.controlChanges); a.maxChanges=std::max(a.maxChanges,b.controlChanges);
    a.largestShareBps=std::max(a.largestShareBps,b.largestShareBps);
    a.operations+=b.operations; a.attacks+=b.attacks; a.defenses+=b.defenses; a.resolved+=b.resolved; a.failed+=b.failed;
    a.cancelled+=b.cancelled; a.live+=b.live; a.manpowerLoss+=b.manpowerLoss; a.treasurySpent+=b.treasurySpent;
    a.commitmentPeak=std::max(a.commitmentPeak,b.commitmentPeak); a.resourceExhausted+=b.resourceExhausted;
    a.npcs.updates+=b.npcs.updates; a.npcs.travelsStarted+=b.npcs.travelsStarted; a.npcs.travelsCompleted+=b.npcs.travelsCompleted;
    a.npcs.travelsCancelled+=b.npcs.travelsCancelled; a.npcs.participations+=b.npcs.participations;
    a.npcs.relationshipChanges+=b.npcs.relationshipChanges; a.npcs.promotions+=b.npcs.promotions;
}
void printStats(const Statistics& s) {
    std::cout << "Control changes: " << s.controlChanges << "\nContested transitions: " << s.contests
        << "\nUncontrolled transitions: " << s.uncontrolledTransitions << "\nTerritory transitions: " << s.transitions
        << "\nFactions reaching zero controlled territories: " << s.zeroTerritoryFactions
        << "\nLargest final faction share (basis points): " << s.largestShareBps << '\n';
    std::cout << "NPC semantic updates: " << s.npcs.updates << "\nTravels started: " << s.npcs.travelsStarted
        << "\nTravels completed: " << s.npcs.travelsCompleted << "\nTravels cancelled: " << s.npcs.travelsCancelled
        << "\nNPC operation participations: " << s.npcs.participations << "\nRelationship changes: " << s.npcs.relationshipChanges
        << "\nPromotions: " << s.npcs.promotions << '\n';
    std::cout << "Operations created: " << s.operations << "\nResolved (successful): " << s.resolved << "\nFailed: " << s.failed
        << "\nCancelled: " << s.cancelled << "\nActive/preparing: " << s.live << "\nAttacks: " << s.attacks << "\nDefenses: " << s.defenses
        << "\nManpower lost: " << s.manpowerLoss << "\nTreasury spent: " << s.treasurySpent
        << "\nPeak committed manpower: " << s.commitmentPeak << "\nFactions unable to mobilize (resources): " << s.resourceExhausted << '\n';
}
Statistics run(GameState& state,std::uint64_t days,bool verbose,bool log) {
    std::map<FactionId,FactionResources> initial; std::map<FactionId,std::int64_t> money; std::set<FactionId> zeroTerritories;
    std::uint64_t initialCommitted=0;
    const auto initialOperations=state.factionSimulation.operations.entries().size();
    const auto npcBefore=state.npcSimulation.statistics;
    const auto territoryBefore=state.territorySimulation.statistics;
    for (const auto& [id,f] : state.world.factions.entries()) {
        const auto resources=factionResources(state,id); initial.emplace(id,resources); money.emplace(id,f.treasury);
        initialCommitted+=resources.committedManpower;
    }
    const auto trackControl=[&]() {
        for (const auto& [id,f] : state.world.factions.entries()) { (void)f;
            if (controlledTerritoryCount(state,id)==0) zeroTerritories.insert(id);
        }
    };
    trackControl();
    Statistics stats; stats.commitmentPeak=initialCommitted; Simulation simulation;
    auto lastTerritoryTick=neverTick;
    const auto observe=[&](const FactionOperationEvent& event) {
        if (!simulation.territoryEvents().empty() && lastTerritoryTick!=event.tick) { trackControl(); lastTerritoryTick=event.tick; }
        std::uint64_t committed=0;
        for (const auto& [id,f] : state.world.factions.entries()) { (void)f; committed+=factionResources(state,id).committedManpower; }
        stats.commitmentPeak=std::max(stats.commitmentPeak,committed);
        if (log) {
            const auto& operation=state.factionSimulation.operations.get(event.operation);
            constexpr std::array names{"Created","Started","Resolved","Failed","Cancelled"};
            std::cout << "Tick " << event.tick << " Operation #" << event.operation << ' ' << names.at(static_cast<std::size_t>(event.kind))
                << " Faction #" << event.faction << (operation.type==OperationType::AttackTerritory ? " ATTACK Territory #" : " DEFEND Territory #") << event.territory << '\n';
        }
    };
    validateState(state); simulation.advanceHeadless(state,days*gameDayTicks,observe); validateState(state);
    const auto& territoryAfter=state.territorySimulation.statistics;
    stats.controlChanges=territoryAfter.controlChanges-territoryBefore.controlChanges;
    stats.contests=territoryAfter.contestedTransitions-territoryBefore.contestedTransitions;
    stats.uncontrolledTransitions=territoryAfter.uncontrolledTransitions-territoryBefore.uncontrolledTransitions;
    stats.transitions=territoryAfter.transitions-territoryBefore.transitions;
    stats.staticWorlds=stats.transitions==0 ? 1 : 0; stats.zeroTerritoryFactions=zeroTerritories.size();
    for (const auto& [id,op] : state.factionSimulation.operations.entries()) {
        (void)id; ++stats.operations;
        if (op.type==OperationType::AttackTerritory) ++stats.attacks; else ++stats.defenses;
        if (pending(op.status)) ++stats.live;
        else if (op.status==OperationStatus::Resolved) ++stats.resolved;
        else if (op.status==OperationStatus::Failed) ++stats.failed;
        else ++stats.cancelled;
    }
    const auto& npcAfter=state.npcSimulation.statistics;
    stats.npcs={npcAfter.updates-npcBefore.updates,npcAfter.travelsStarted-npcBefore.travelsStarted,
        npcAfter.travelsCompleted-npcBefore.travelsCompleted,npcAfter.travelsCancelled-npcBefore.travelsCancelled,
        npcAfter.participations-npcBefore.participations,npcAfter.relationshipChanges-npcBefore.relationshipChanges,npcAfter.promotions-npcBefore.promotions};
    if (verbose) std::cout << "FAULTLINE HEADLESS SIM\nSeed: " << state.metadata.seed.value << "\nGenerator: "
        << state.metadata.generatorVersion.value << "\nSimulated: " << days << " days\nTick: " << state.clock.tick()
        << "\nFactions: " << state.world.factions.entries().size() << '\n';
    for (const auto& [id,faction] : state.world.factions.entries()) {
        const auto r=factionResources(state,id);
        const auto territoryCount=controlledTerritoryCount(state,id);
        if (!state.world.territories.entries().empty()) stats.largestShareBps=std::max(stats.largestShareBps,
            static_cast<std::uint64_t>(territoryCount*10000/state.world.territories.entries().size()));
        stats.manpowerLoss+=initial.at(id).totalManpower-r.totalManpower;
        stats.treasurySpent+=static_cast<std::uint64_t>(money.at(id)-faction.treasury);
        const auto minimum=std::min(state.factionSimulation.config.attackBaseCost,state.factionSimulation.config.defenseBaseCost)+
            state.factionSimulation.config.costPerMember*state.factionSimulation.config.minForce;
        if (faction.reserveMembers<state.factionSimulation.config.minForce || faction.treasury<minimum) ++stats.resourceExhausted;
        if (verbose) std::cout << "Faction #" << id << " | " << faction.name << "\n  Manpower: " << r.totalManpower
            << " | Reserve: " << faction.reserveMembers << " | Committed: " << r.committedManpower << " | Treasury: " << faction.treasury
            << "\n  Cohesion: " << faction.cohesion << " | Confidence: " << faction.strategy.confidence
            << " | Capacity: " << r.operationalCapacity << " | Active Operations: " << r.activeOperations
            << " | Controlled territories: " << territoryCount << " | Operations created: " << std::count_if(state.factionSimulation.operations.entries().begin(),state.factionSimulation.operations.entries().end(),
                [&](const auto& entry){return entry.second.faction==id;}) << '\n';
    }
    if (verbose) {
        std::uint64_t contested=0,uncontrolled=0;
        for (const auto& [id,t] : state.world.territories.entries()) { (void)t;
            const auto control=territoryControl(state,id);
            if (control.state==TerritoryControlState::Contested) ++contested;
            if (control.state==TerritoryControlState::Uncontrolled) ++uncontrolled;
        }
        std::cout << "Current contested: " << contested << "\nCurrent uncontrolled: " << uncontrolled << '\n';
        printStats(stats);
        const auto factions=state.world.factions.entries().size();
        const auto rate=factions==0 ? 0 : (stats.operations-initialOperations)*3000/(factions*days);
        std::cout << "Operations/faction/30-days: " << rate/100 << '.' << (rate%100<10 ? "0" : "") << rate%100
            << "\nValidation: PASS\nStateHash: " << std::hex << stateHash(state) << std::dec << '\n';
    }
    return stats;
}
}
int main(int argc,char** argv) {
    try {
        std::uint64_t seed=12345,days=30,begin=0,end=0; bool log=false,range=false;
        std::string savePath,loadPath;
        for (int i=1;i<argc;++i) {
            const std::string_view option{argv[i]};
            const auto value=[&]() -> std::string_view { if (++i>=argc) throw std::invalid_argument("Missing option value"); return argv[i]; };
            if (option=="--seed") seed=number(value());
            else if (option=="--days") days=number(value());
            else if (option=="--range") { begin=number(value()); end=number(value()); range=true; }
            else if (option=="--log-operations") log=true;
            else if (option=="--save") savePath=value();
            else if (option=="--load") loadPath=value();
            else throw std::invalid_argument("Usage: faultline_sim [--seed N | --range A B] --days N [--log-operations] [--save PATH] [--load PATH]");
        }
        if (days==0 || days>36500 || (range && (end<begin || end-begin>=10000 || !savePath.empty() || !loadPath.empty() || log)))
            throw std::invalid_argument("Invalid duration/range or incompatible batch options");
        if (range) {
            const auto start=std::chrono::steady_clock::now(); Statistics total;
            for (auto current=begin;;++current) {
                try {
                    GameState state{WorldSeed{current}}; const auto stats=run(state,days,false,false);
                    core::require(stats.operations>0,"Generated world produced no autonomous operations"); accumulate(total,stats);
                }
                catch (const std::exception& error) { throw std::runtime_error("Batch seed="+std::to_string(current)+": "+error.what()); }
                if (current==end) break;
            }
            const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout << "FAULTLINE HEADLESS SIM BATCH\nSeeds: " << begin << ".." << end << "\nDays/seed: " << days << '\n'; printStats(total);
            std::cout << "Average control changes/seed: " << static_cast<double>(total.controlChanges)/static_cast<double>(end-begin+1)
                << "\nAverage contested transitions/seed: " << static_cast<double>(total.contests)/static_cast<double>(end-begin+1)
                << "\nControl change range: " << total.minChanges << ".." << total.maxChanges
                << "\nFully static worlds: " << total.staticWorlds << '\n';
            std::cout << "Validation: PASS\nBatch seconds: " << seconds << "\nMilliseconds/seed: " << seconds*1000/static_cast<double>(end-begin+1) << '\n';
        } else {
            auto state=loadPath.empty() ? GameState{WorldSeed{seed}} : persistence::loadGameStateFile(loadPath);
            (void)run(state,days,true,log); if (!savePath.empty()) persistence::saveGameStateFile(state,savePath);
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAULTLINE SIM: " << error.what() << '\n'; return 1; }
}
