#pragma once
#include "simulation/runtime/game.hpp"

namespace faultline::simulation {
// Versioned headless verification script, independent of platform key mapping.
inline PlayerInputFrame playerVerificationInput(std::uint64_t tick) {
    constexpr float directions[][2]={{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
    const auto segment=(tick/1000)%8;
    PlayerInputFrame input;
    input.moveX=directions[segment][0]; input.moveZ=directions[segment][1];
    input.aimX=directions[(segment+2)%8][0]; input.aimZ=directions[(segment+2)%8][1];
    input.interactPressed=tick%137==0;
    return input;
}
}
