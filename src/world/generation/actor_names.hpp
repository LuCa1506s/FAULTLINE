#pragma once
#include "world/state/actor_types.hpp"
#include "core/rng/deterministic_rng.hpp"
#include "core/rng/stable_hash.hpp"
#include <algorithm>
#include <set>
#include <string_view>

namespace faultline::world::actor_detail {
struct NamePool {
    std::array<std::string_view,8> factionFirst, factionLast, given, surname;
};
inline constexpr std::array<NamePool,4> names{{
    {{{"Eastside","Northbridge","Southbank","Westhaven","Riverside","Crown","Union","Hillcrest"}},
     {{"Kings","Crew","Circle","Collective","Alliance","Union","Brotherhood","Network"}},
     {{"Marcus","Darius","Andre","Malcolm","Isaiah","Jordan","Damon","Terrell"}},
     {{"Carter","Brooks","Reed","Hayes","Johnson","Morgan","Davis","Bennett"}}},
    {{{"Moretti","Bellini","Romano","Vitale","Ferraro","Conti","Lombardi","Marino"}},
     {{"Family","Circle","Union","Syndicate","Consortium","Association","Network","Society"}},
     {{"Carlo","Marco","Luca","Antonio","Matteo","Paolo","Giovanni","Enzo"}},
     {{"Moretti","Bellini","Romano","Vitale","Ferraro","Conti","Lombardi","Marino"}}},
    {{{"Kuroda","Mizuno","Takeda","Ishida","Aoki","Mori","Hayashi","Sakai"}},
     {{"Association","Union","Society","Circle","Syndicate","Network","Alliance","Consortium"}},
     {{"Kenji","Akira","Daichi","Hiroshi","Ren","Takumi","Naoki","Shinji"}},
     {{"Kuroda","Mizuno","Takeda","Ishida","Aoki","Mori","Hayashi","Sakai"}}},
    {{{"Sierra","Costa","Rio","Sol","Valle","Cumbre","Frontera","Puerto"}},
     {{"Cartel","Union","Consortium","Network","Alliance","Circle","Syndicate","Association"}},
     {{"Diego","Mateo","Rafael","Javier","Luis","Carlos","Emilio","Andres"}},
     {{"Vargas","Navarro","Rojas","Medina","Castillo","Herrera","Salazar","Ortega"}}}
}};
inline std::string uniqueName(FactionFamily family, bool npc, core::DeterministicRng& rng,
                              std::uint64_t id, std::set<std::string>& used) {
    const auto& pool = names.at(static_cast<std::size_t>(family));
    const auto& first = npc ? pool.given : pool.factionFirst;
    const auto& last = npc ? pool.surname : pool.factionLast;
    std::string candidate;
    for (int attempt=0; attempt<16; ++attempt) {
        candidate = std::string{first[static_cast<std::size_t>(rng.uniform(first.size()))]} + " " +
            std::string{last[static_cast<std::size_t>(rng.uniform(last.size()))]};
        if (used.insert(candidate).second) return candidate;
    }
    // Numeric suffixes cannot collide with the alphabetic catalogue; IDs are unique.
    candidate += " #" + std::to_string(id);
    core::require(used.insert(candidate).second,"Duplicate name fallback ID"); return candidate;
}
inline std::uint16_t bps(std::int64_t value) {
    return static_cast<std::uint16_t>(std::clamp<std::int64_t>(value,0,10000));
}
inline std::uint64_t pairSeed(std::uint64_t seed, std::uint64_t from, std::uint64_t to) {
    core::StateHash hash; hash.u64(seed); hash.u64(from); hash.u64(to); return hash.value();
}
}
