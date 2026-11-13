#pragma once
#include "simulation/factions/faction_state.hpp"

namespace faultline::simulation {
inline constexpr std::uint32_t marketUnitLimit=1000000;
inline constexpr std::uint64_t marketIncomeDenominator=gameDayTicks*100;
struct MarketRules {
    std::uint64_t intervalTicks{gameHourTicks};
    std::uint32_t priceResponseDivisor{4}, disruptionRecovery{250};
    auto operator<=>(const MarketRules&) const = default;
};
struct DealerState {
    // Economic assignment, independent of physical representation and daily travel.
    FactionId faction;
    TerritoryId territory;
    std::uint32_t capacity{};
    bool enabled{true};
    auto operator<=>(const DealerState&) const = default;
};
struct FactionMarketState {
    std::uint32_t supply{}, shareBps{}, sold{};
    std::uint64_t incomeRemainder{}, creditedIncome{};
    auto operator<=>(const FactionMarketState&) const = default;
};
struct DistrictMarketState {
    std::uint32_t baselineDemand{}, demandScaleBps{10000}, demand{}, supply{};
    std::uint32_t referencePrice{100}, minimumPrice{50}, maximumPrice{200}, price{100}, sensitivity{60};
    std::uint32_t accessibilityBps{10000}, disruptionBps{};
    std::map<FactionId,FactionMarketState> factions;
    auto operator<=>(const DistrictMarketState&) const = default;
};
struct MarketSimulationState {
    MarketRules rules;
    std::uint64_t nextUpdateTick{gameHourTicks}, updates{};
    std::map<DistrictId,DistrictMarketState> districts;
    std::map<NpcId,DealerState> dealers;
};
}
