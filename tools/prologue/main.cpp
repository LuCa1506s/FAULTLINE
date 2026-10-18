#include "simulation/prologue/prologue_system.hpp"
#include "simulation/history/history_system.hpp"
#include "simulation/story/story_system.hpp"
#include "simulation/runtime/game.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/factions/faction_system.hpp"
#include "persistence/save.hpp"
#include <charconv>
#include <iostream>
using namespace faultline;
using namespace faultline::simulation;

namespace {
std::uint64_t number(std::string_view s) {
    std::uint64_t n{}; const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{} || end != s.data() + s.size()) throw std::invalid_argument("Invalid number");
    return n;
}
}

int main(int argc, char** argv) {
    try {
        std::uint64_t first = 12345, last = 12345, origin = 1;
        bool replay = false, small = false, verbose = false;
        std::string savePath, loadPath, inspectPath;
        std::uint32_t inspectMonths = 48;

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            const auto value = [&]() -> std::string_view {
                if (++i == argc) throw std::invalid_argument("Missing argument");
                return argv[i];
            };
            if (arg == "--seed") first = last = number(value());
            else if (arg == "--range") { first = number(value()); last = number(value()); }
            else if (arg == "--origin") origin = number(value());
            else if (arg == "--replay") replay = true;
            else if (arg == "--small") small = true;
            else if (arg == "--verbose") verbose = true;
            else if (arg == "--save") savePath = std::string(value());
            else if (arg == "--load") loadPath = std::string(value());
            else if (arg == "--inspect") inspectPath = std::string(value());
            else if (arg == "--months") inspectMonths = static_cast<std::uint32_t>(number(value()));
            else throw std::invalid_argument("Unknown prologue option: " + std::string(arg));
        }

        core::require(first <= last && last - first < 10000, "Invalid seed range");
        core::require(origin >= 1 && origin <= 5, "Origin must be 1..5");
        core::require(loadPath.empty() || (first == last && savePath.empty()), "Load mode requires exactly one seed and no save");

        std::uint64_t worlds = 0, totalDealers = 0, totalEvents = 0;
        std::uint64_t maxMarket = 0, maxControl = 0;
        core::StateHash batch;

        for (auto seed = first;; ++seed) {
            world::WorldGenerationConfig config;
            if (small) { config.width = 512; config.depth = 512; config.waterPercent = 0; }

            GameState state = loadPath.empty()
                ? GameState{WorldSeed{seed}, config}
                : persistence::loadGameStateFile(loadPath);

            if (loadPath.empty()) {
                runPrologue(state, FactionId{origin});
            } else {
                core::require(state.prologue.phase != ProloguePhase::PreMission,
                              "Loaded save does not contain prologue progress");
                if (state.prologue.phase == ProloguePhase::DeliveryComplete ||
                    state.prologue.phase == ProloguePhase::MissionActive) {
                    core::require(!state.history.version,
                                  "Loaded prologue with history already started");
                    const auto evidence = generatePrologueEvidence(
                        state, state.prologue.origin, state.prologue.evidence.deliveryTick);
                    commitPrologueEvidence(state, evidence);
                    beginStrategicHistory(state, evidence);
                }
                while (state.history.months < inspectMonths) {
                    advanceStrategicHistory(state);
                }
                if (state.history.months == historyMonths && !state.story.version) {
                    initializeStoryGraph(state);
                }
                state.prologue.phase = ProloguePhase::Complete;

            }

            validateState(state);

            if (state.history.months == historyMonths) {
                validatePostTimeskipWorld(state);
            }

            if (verbose) {
                std::cout << "Seed: " << state.metadata.seed.value
                    << "\nFaction: " << origin
                    << "\nPhase: " << static_cast<int>(state.prologue.phase)
                    << "\nMission: " << (state.prologue.firstDelivery ? state.prologue.firstDelivery.value() : 0)
                    << "\nHistory months: " << state.history.months
                    << "\nBaseWorldHash: " << std::hex << world::baseWorldHash(*state.world.base)
                    << "\nStateHash: " << stateHash(state) << std::dec
                    << "\nIncident: " << static_cast<int>(state.prologue.evidence.incident)
                    << "\nRival: " << (state.prologue.evidence.rival ? state.prologue.evidence.rival.value() : 0)
                    << "\nDiscovery incident: " << static_cast<int>(state.prologue.evidence.incident);
            }

            if (replay && loadPath.empty()) {
                GameState duplicate = loadPath.empty()
                    ? GameState{WorldSeed{seed}, config}
                    : persistence::loadGameStateFile(loadPath);
                runPrologue(duplicate, FactionId{origin});
                if (stateHash(state) != stateHash(duplicate))
                    throw std::runtime_error("Prologue replay mismatch at seed " + std::to_string(seed));
            }

            if (inspectPath == std::to_string(seed)) {
                std::cout << "BaseWorldHash: " << std::hex << world::baseWorldHash(*state.world.base) << std::dec << "\n";
            }

            batch.u64(stateHash(state));
            ++worlds;
            totalEvents += state.history.events.size();
            totalDealers += state.marketSimulation.dealers.size();

            std::map<FactionId, std::uint64_t> supply;
            std::uint64_t total = 0;
            for (const auto& [id, m] : state.marketSimulation.districts) {
                (void)id;
                for (const auto& [f, e] : m.factions) { supply[f] += e.supply; total += e.supply; }
            }
            for (const auto& [f, n] : state.world.factions.entries()) {
                (void)n;
                maxMarket = std::max(maxMarket, total ? supply[f] * 10000 / total : 0);
                maxControl = std::max(maxControl,
                    static_cast<std::uint64_t>(controlledTerritoryCount(state, f)) * 10000 /
                        state.world.territories.entries().size());
            }

            if (!savePath.empty() && first == last) {
                persistence::saveGameStateFile(state, savePath);
            }

            if (seed == last) break;
        }

        std::cout << "Worlds: " << worlds
            << "\nHistory events total: " << totalEvents
            << "\nDealers total: " << totalDealers
            << "\nLargest market share bps: " << maxMarket
            << "\nLargest territorial share bps: " << maxControl
            << "\nReplay: " << (replay ? "PASS" : "not requested")
            << "\nBatchHash: " << std::hex << batch.value() << std::dec
            << "\nValidation: PASS\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAULTLINE PROLOGUE: " << e.what() << '\n';
        return 1;
    }
}
