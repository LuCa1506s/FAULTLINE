#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "core/id/id.hpp"
#include "core/rng/deterministic_rng.hpp"
#include "core/rng/stable_hash.hpp"
#include "core/time/simulation_clock.hpp"
#include "core/diagnostics/diagnostics.hpp"
#include <limits>
#include <sstream>
#include <type_traits>
#include <unordered_map>

using namespace faultline;
using namespace faultline::core;
static_assert(!std::is_convertible_v<NpcId, EntityId>);
static_assert(!std::is_constructible_v<NpcId, FactionId>);
static_assert(!std::is_convertible_v<std::uint64_t, NpcId>);
TEST_CASE("typed IDs have typed null, ordering and container support") {
    CHECK_FALSE(static_cast<bool>(NpcId{}));
    CHECK(NpcId{1} < NpcId{2});
    std::unordered_map<NpcId, int> values{{NpcId{42}, 7}};
    CHECK(values.at(NpcId{42}) == 7);
    std::ostringstream out; out << NpcId{42}; CHECK(out.str() == "42");
}
TEST_CASE("FNV-1a published byte-string vectors and canonical encoding") {
    CHECK(stableHash("") == 0xcbf29ce484222325ULL);
    CHECK(stableHash("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(stableHash("foobar") == 0x85944171f73967e8ULL);
    StateHash integer, bytes;
    integer.u64(0x0807060504030201ULL);
    for (std::uint8_t i = 1; i <= 8; ++i) bytes.byte(i);
    CHECK(integer.value() == bytes.value());
    StateHash a, b; a.string("ab"); a.string("c"); b.string("a"); b.string("bc");
    CHECK(a.value() != b.value());
}
TEST_CASE("domain seeds are reproducible and separated") {
    CHECK(deriveSeed(12345, "roads") == deriveSeed(12345, "roads"));
    CHECK(deriveSeed(12345, "roads") != deriveSeed(12345, "npc"));
    CHECK(deriveSeed(12345, "npc", NpcId{1}) != deriveSeed(12345, "npc", NpcId{2}));
    DeterministicRng roads{deriveSeed(12345, "roads")}, copy = roads;
    DeterministicRng npc{deriveSeed(12345, "npc")};
    for (int i = 0; i < 100; ++i) (void)npc.nextU64();
    CHECK(roads.nextU64() == copy.nextU64());
}
TEST_CASE("xoshiro256 starstar reference state vector") {
    DeterministicRng rng{0}; rng.restore({1, 2, 3, 4});
    CHECK(rng.nextU64() == 11520);
    CHECK(rng.nextU64() == 0);
    CHECK(rng.nextU64() == 1509978240);
    CHECK_THROWS_AS(rng.restore({0, 0, 0, 0}), std::invalid_argument);
}
TEST_CASE("Gate A seed 12345 and state restoration") {
    DeterministicRng a{12345}, b{12345};
    DeterministicRng golden{12345};
    for (const auto value : {13720838825685603483ULL, 2398916695208396998ULL,
            17770384849984869256ULL, 891717726879801395ULL, 10241316046318454344ULL}) {
        CHECK(golden.nextU64() == value);
    }
    for (int i = 0; i < 10000; ++i) CHECK(a.nextU64() == b.nextU64());
    const auto saved = a.state();
    DeterministicRng restored{999}; restored.restore(saved);
    for (int i = 0; i < 1000; ++i) CHECK(a.nextU64() == restored.nextU64());
}
TEST_CASE("unbiased bounded API handles boundaries and invalid inputs") {
    DeterministicRng rng{12345};
    for (int i = 0; i < 10000; ++i) {
        CHECK(rng.uniform(1) == 0);
        CHECK(rng.uniform(3) < 3);
        CHECK(rng.uniform(10, 20) >= 10);
        CHECK(rng.uniform(10, 20) < 20);
        CHECK(rng.uniform(std::numeric_limits<std::uint64_t>::max()) < std::numeric_limits<std::uint64_t>::max());
        CHECK(rng.chance(1, 1)); CHECK_FALSE(rng.chance(0, 1));
    }
    CHECK_THROWS_AS((void)rng.uniform(0), std::invalid_argument);
    CHECK_THROWS_AS((void)rng.uniform(20, 10), std::invalid_argument);
    CHECK_THROWS_AS((void)rng.chance(2, 1), std::invalid_argument);
    CHECK_THROWS_AS((void)rng.chance(0, 0), std::invalid_argument);
}
TEST_CASE("clock uses exact integer game time and guards overflow") {
    SimulationClock clock;
    for (int i = 0; i < 38400; ++i) clock.advance();
    CHECK(clock.tick() == 38400);
    CHECK(clock.gameTime().milliseconds == 86400000);
    CHECK(SimulationClock::tickDuration.count() == 50);
    SimulationClock restored{1000}; CHECK(restored.gameTime().milliseconds == 2250000);
    SimulationClock end{SimulationClock::maxTick}; CHECK_THROWS_AS(end.advance(), std::overflow_error);
    CHECK_THROWS_AS(SimulationClock{SimulationClock::maxTick + 1}, std::overflow_error);
}
TEST_CASE("diagnostics expose reproducibility context and fail loudly") {
    std::ostringstream out; out << DebugContext{12345, 1, 1000, {2250000}};
    CHECK(out.str().find("WorldSeed: 12345") != std::string::npos);
    CHECK(out.str().find("SimulationTick: 1000") != std::string::npos);
    CHECK_THROWS_AS(require(false, "broken invariant"), std::logic_error);
}
