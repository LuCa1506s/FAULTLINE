#pragma once
#include "simulation/history/history_state.hpp"
#include "world/state/actor_types.hpp"
#include <cstdint>

namespace faultline::simulation {

enum class ProloguePhase : std::uint8_t {
    PreMission = 0,
    MissionAvailable,
    MissionActive,
    DeliveryComplete,
    Complete
};

inline constexpr std::uint32_t currentPrologueStateVersion = 1;

struct PrologueState {
    std::uint32_t version{currentPrologueStateVersion};
    ProloguePhase phase{ProloguePhase::PreMission};
    MissionId firstDelivery{};
    FactionId origin{};
    NpcId giver{};
    NpcId recipient{};
    PrologueEvidence evidence{};
    auto operator<=>(const PrologueState&) const = default;
};

}
