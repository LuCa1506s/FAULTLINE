#include "doctest.h"
#include "simulation/runtime/game.hpp"
#include "app/fixed_step_runner.hpp"
using namespace faultline;

TEST_CASE("runtime 1000 ticks headlessly is deterministic") {
    Game a{WorldSeed{12345}}, b{WorldSeed{12345}}, c{WorldSeed{54321}};
    for (int i = 0; i < 1000; ++i) { a.tick(); b.tick(); c.tick(); }
    CHECK(a.state().clock.tick() == 1000);
    CHECK(stateHash(a.state()) == stateHash(b.state()));
    CHECK(stateHash(a.state()) != stateHash(c.state()));
}
TEST_CASE("state hashing includes every runtime RNG and clock") {
    GameState a{WorldSeed{42}}, b = a;
    for (std::size_t i = 0; i < static_cast<std::size_t>(RandomDomain::Count); ++i) {
        b = a; (void)b.randomness.streams[i].nextU64(); CHECK(stateHash(a) != stateHash(b));
    }
    b = a; b.clock.advance(); CHECK(stateHash(a) != stateHash(b));
    Simulation sim; GameState restored = a;
    for (int i = 0; i < 1000; ++i) {
        sim.tick(a, {a.clock.tick(), {}, {}});
        sim.tick(restored, {restored.clock.tick(), {}, {}});
    }
    CHECK(stateHash(a) == stateHash(restored));
}
TEST_CASE("command queue preserves sequence and target tick") {
    Game game{WorldSeed{1}};
    game.requestValidation(2); game.requestValidation(0); game.requestValidation(0);
    game.tick(); REQUIRE(game.events().values().size() == 2);
    CHECK(game.events().values()[0].commandSequence == 1);
    CHECK(game.events().values()[1].commandSequence == 2);
    game.tick(); CHECK(game.events().empty());
    game.tick(); REQUIRE(game.events().values().size() == 1);
    CHECK(game.events().values()[0].commandSequence == 0);
    CHECK_THROWS(game.requestValidation(0));
}
TEST_CASE("replay input validation happens before mutation") {
    GameState state{WorldSeed{1}}; Simulation sim; const auto before = stateHash(state);
    CHECK_THROWS(sim.tick(state, {1, {}, {}}));
    CHECK_THROWS(sim.tick(state, {0, {}, {{0, 2, {}}, {0, 1, {}}}}));
    CHECK(stateHash(state) == before);
}
TEST_CASE("ordered buffers isolate commit and preserve insertion order") {
    core::OrderedBuffer<int> buffer;
    std::vector<int> order;
    buffer.push(1); buffer.push(2);
    buffer.commit([&](int value) { order.push_back(value); if (value == 1) buffer.push(3); });
    CHECK(order == std::vector<int>{1, 2});
    buffer.commit([&](int value) { order.push_back(value); });
    CHECK(order == std::vector<int>{1, 2, 3});
}
TEST_CASE("typed structural commands defer entity mutation until commit") {
    world::WorldState state; StructuralCommandBuffer commands;
    commands.push(world::CreateEntityCommand{}); commands.push(world::CreateEntityCommand{});
    commands.push(world::DestroyEntityCommand{EntityId{1}});
    CHECK(state.entities.entries().empty());
    commands.commit([&](const auto& command) { world::applyStructuralCommand(state, command); });
    CHECK_FALSE(state.entities.exists(EntityId{1})); CHECK(state.entities.exists(EntityId{2}));
    CHECK(state.entities.nextId() == 3); CHECK(commands.empty());
}
TEST_CASE("fixed step runner retains integer backlog") {
    using namespace std::chrono_literals;
    FixedStepRunner runner; runner.addTime(49ms); CHECK_FALSE(runner.shouldTick());
    runner.addTime(102ms); int ticks = 0;
    while (runner.shouldTick()) { ++ticks; runner.consumeTick(); }
    CHECK(ticks == 3); CHECK(runner.backlog() == 1ms);
    CHECK_THROWS(runner.consumeTick()); CHECK_THROWS(runner.addTime(-1ns));
}
