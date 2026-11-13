#pragma once
#include "simulation/runtime/game_state.hpp"
#include <filesystem>
#include <span>
#include <stdexcept>

namespace faultline::persistence {
struct SaveFormatVersion { std::uint32_t value; auto operator<=>(const SaveFormatVersion&) const = default; };
inline constexpr SaveFormatVersion currentSaveFormatVersion{14};
struct GameVersion {
    std::uint32_t major{}, minor{1}, patch{};
    auto operator<=>(const GameVersion&) const = default;
};
inline constexpr GameVersion currentGameVersion{};
inline constexpr std::array<std::uint8_t, 16> saveMagic{
    'F','A','U','L','T','L','I','N','E','_','S','A','V','E',0,0};
struct SaveHeader {
    SaveFormatVersion saveFormatVersion{currentSaveFormatVersion};
    GameVersion gameVersion{currentGameVersion};
    GeneratorVersion generatorVersion;
    WorldSeed worldSeed;
    auto operator<=>(const SaveHeader&) const = default;
};
enum class SaveErrorCode {
    InvalidMagic, UnsupportedSaveVersion, UnsupportedGeneratorVersion, UnsupportedGameVersion,
    TruncatedData, InvalidData, ValidationFailure, IoError
};
// Public API reports typed failures; no partially loaded state ever escapes.
class SaveError : public std::runtime_error {
public:
    SaveError(SaveErrorCode code, std::string section, std::size_t offset, std::string message,
              SaveHeader header = {});
    SaveErrorCode code;
    std::string section;
    std::size_t offset;
    SaveHeader header;
};
using SaveBytes = std::vector<std::uint8_t>;
inline constexpr std::size_t maxSaveBytes = 64 * 1024 * 1024;
// Single authoritative thread, completed-tick boundary only. Game::state() exposes
// such a boundary; queued application input and Simulation buffers are not GameState.
[[nodiscard]] SaveBytes saveGameState(const GameState& state);
[[nodiscard]] SaveHeader readSaveHeader(std::span<const std::uint8_t> bytes);
[[nodiscard]] GameState loadGameState(std::span<const std::uint8_t> bytes);
void saveGameStateFile(const GameState& state, const std::filesystem::path& path);
[[nodiscard]] GameState loadGameStateFile(const std::filesystem::path& path);
}
