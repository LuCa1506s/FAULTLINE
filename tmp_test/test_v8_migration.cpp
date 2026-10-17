#include "persistence/save.hpp"
#include "simulation/runtime/game.hpp"
#include <iostream>
#include <vector>
#include <cstdint>

int main() {
    using namespace faultline;
    using namespace faultline::persistence;
    using namespace faultline::simulation;

    try {
        // Load v8 fixture
        const std::string v8Path = R"(C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1\tests\fixtures\save_v8_phase14.flsave)";
        GameState state = loadGameStateFile(v8Path);
        std::cout << "Loaded v8 fixture successfully.\n";
        std::cout << "  Seed: " << state.metadata.seed.value << "\n";
        std::cout << "  GeneratorVersion: " << state.metadata.generatorVersion.value << "\n";
        std::cout << "  Tick: " << state.clock.tick() << "\n";
        uint64_t hash1 = stateHash(state);
        std::cout << "  StateHash: " << std::hex << hash1 << std::dec << "\n";

        // Save state to buffer
        std::vector<std::uint8_t> savedBytes = saveGameState(state);
        std::cout << "Saved state size: " << savedBytes.size() << " bytes\n";

        // Read header of saved bytes to check SaveFormatVersion
        SaveHeader savedHeader = readSaveHeader(savedBytes);
        std::cout << "  Saved SaveFormatVersion: " << savedHeader.saveFormatVersion.value << "\n";
        std::cout << "  Saved GeneratorVersion: " << savedHeader.generatorVersion.value << "\n";
        std::cout << "  Saved Seed: " << savedHeader.worldSeed.value << "\n";

        // Load saved buffer back
        GameState state2 = loadGameState(savedBytes);
        std::cout << "Round-trip loaded successfully.\n";
        uint64_t hash2 = stateHash(state2);
        std::cout << "  Round-trip StateHash: " << std::hex << hash2 << std::dec << "\n";

        // Compare
        if (hash1 != hash2) {
            std::cerr << "ERROR: StateHash mismatch after round-trip!\n";
            return 1;
        }
        // Optional: deeper equality check via saveGameState equality
        if (saveGameState(state) != saveGameState(state2)) {
            std::cerr << "ERROR: Full state mismatch after round-trip!\n";
            return 1;
        }

        // Check that saved SaveFormatVersion is current (9)
        if (savedHeader.saveFormatVersion.value != currentSaveFormatVersion.value) {
            std::cerr << "ERROR: Saved SaveFormatVersion is not current (expected " 
                      << currentSaveFormatVersion.value << ", got " 
                      << savedHeader.saveFormatVersion.value << ")\n";
            return 1;
        }

        std::cout << "SUCCESS: v8 fixture loads, saves as v9, and round-trip preserves StateHash.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
