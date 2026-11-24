# C++ SDK

Use the repository root as a CMake subdirectory and link `faultline::sdk`.
CMake 3.20 and a C++20 compiler are required. A policy integration includes only
`<faultline/simulation.hpp>`.

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyAgent LANGUAGES CXX)

add_subdirectory(path/to/FAULTLINE faultline EXCLUDE_FROM_ALL)
add_executable(my_agent main.cpp)
target_link_libraries(my_agent PRIVATE faultline::sdk)
```

```cpp
#include <faultline/simulation.hpp>
#include <iostream>

int main() {
    using namespace faultline::sdk;
    Simulation sim{42, {WorldProfile::compact}};
    const auto result = sim.step(Action::east, 16);
    std::cout << result.observation.position.x << ' ' << result.observation.position.z << '\n';
    sim.save("experiment.flsave");
    std::cout << std::hex << sim.state_hash() << '\n';
}
```

The [native consumer](../../tests/native_consumer/main.cpp) can also be built
as its own CMake project from the repository root.

```sh
cmake -S tests/native_consumer -B build-consumer -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-consumer --config Release
ctest --test-dir build-consumer -C Release --output-on-failure --no-tests=error
```

## Session contract

- `reset` accepts any unsigned 64-bit seed. `standard` uses the game defaults.
  `compact` generates a 512 × 512 m land-only world centered at the origin.
  Both place a player at the center of the buildable land on the road grid.
- `observe` returns completed ticks, position in global X/Z meters, health and
  status. `road_map` projects generated geometry as points and undirected edges
  indexed into the point vector. Both return detached values.
- `step` holds one action for 1–256 physical ticks. East/west move on X and
  south/north on Z. `advance` applies idle. A free cardinal tick moves 0.25 m.
  The engine owns collision, health and world updates. A call stops early on
  incapacitation and reports the actual ticks simulated. Sessions stop by tick
  100,000. Requests to step beyond the limit are rejected before mutation.
- Each step returns up to 256 faction operation and territory transition events
  in processed tick order, then by event family and order within the engine's
  family buffer. `events_truncated` reports overflow. `observe` and snapshots
  do not include historical events.
- Invalid actions, profiles and tick counts throw `std::invalid_argument`.
  A terminal session rejects advancement, and a closed session rejects reads
  and advancement with `std::logic_error`. `close` is idempotent. `reset`,
  `load` and `restore` can reopen a session.
- `snapshot`/`restore` handle canonical bytes and `save`/`load` handle trusted
  host paths. Snapshot operations impose an 8 MiB byte limit and surface format,
  compatibility and I/O failures as `SnapshotError`. The SDK requires a generated
  world in one of its supported size/origin/coastline profiles, an on-foot player,
  and an eligible runtime boundary within its tick limit. Game tools use their
  own compatibility and size contracts. A rejected restore keeps the current
  session. A failed tick invalidates the session when the engine may have made
  a partial update.
- An application can interleave independent sessions on one thread. Treat a
  single instance as thread-confined. Access to `state_hash`, snapshots and
  file paths belongs to the host. An agent only needs its observation and action
  values. See the [engine determinism contract](../../FAULTLINE_GAME_V1/docs/DETERMINISM.md).
