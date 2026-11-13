#include "persistence/save.hpp"
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    using namespace faultline;
    using namespace faultline::persistence;
    try {
        if (argc!=1 && argc!=3) throw std::invalid_argument("Usage: faultline_savecheck [--seed N | --inspect PATH]");
        if (argc==3 && std::string_view{argv[1]}=="--inspect") {
            auto state=loadGameStateFile(argv[2]);
            std::cout << "CurrentSaveFormatVersion: " << currentSaveFormatVersion.value << "\nGeneratorVersion: "
                << state.metadata.generatorVersion.value << "\nSeed: " << state.metadata.seed.value
                << "\nTick: " << state.clock.tick() << "\nStateHash: " << std::hex << stateHash(state) << "\nValidation: PASS\n";
            return 0;
        }
        std::uint64_t seed=12345;
        if (argc==3) {
            const std::string_view text{argv[2]}; const auto result=std::from_chars(text.data(),text.data()+text.size(),seed);
            if (std::string_view{argv[1]}!="--seed" || result.ec!=std::errc{} || result.ptr!=text.data()+text.size())
                throw std::invalid_argument("Usage: faultline_savecheck [--seed N | --inspect PATH]");
        }
        GameState state{WorldSeed{seed}};
        const auto start=std::chrono::steady_clock::now(); const auto bytes=saveGameState(state);
        const auto saved=std::chrono::steady_clock::now(); auto restored=loadGameState(bytes);
        const auto loaded=std::chrono::steady_clock::now();
        const auto before=stateHash(state), after=stateHash(restored);
        const bool identical=saveGameState(restored)==bytes;
        if (before!=after || !identical) throw std::runtime_error("Round-trip mismatch");
        std::cout << "SaveFormatVersion: " << currentSaveFormatVersion.value << "\nGeneratorVersion: "
            << currentGeneratorVersion.value << "\nSeed: " << seed << "\nBytes: " << bytes.size()
            << "\nStateHash before: " << std::hex << before << "\nStateHash after: " << after << std::dec
            << "\nCanonical bytes: PASS\nValidation: PASS\n" << std::fixed << std::setprecision(3)
            << "Save ms (including validation): " << std::chrono::duration<double,std::milli>(saved-start).count()
            << "\nLoad ms (including validation/rebuild): " << std::chrono::duration<double,std::milli>(loaded-saved).count() << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
