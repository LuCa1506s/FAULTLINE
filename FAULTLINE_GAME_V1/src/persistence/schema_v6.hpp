#pragma once
#include "persistence/schema_v4.hpp"
namespace faultline::persistence::detail {
#define FAULTLINE_SAVE_FIELDS(Type, ...) \
    template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, Type> \
    void fields(A& a, T& v) { a(__VA_ARGS__); }
FAULTLINE_SAVE_FIELDS(MarketRules,v.intervalTicks,v.priceResponseDivisor,v.disruptionRecovery)
FAULTLINE_SAVE_FIELDS(DealerState,v.faction,v.territory,v.capacity,v.enabled)
FAULTLINE_SAVE_FIELDS(FactionMarketState,v.supply,v.shareBps,v.sold,v.incomeRemainder,v.creditedIncome)
#undef FAULTLINE_SAVE_FIELDS
}
