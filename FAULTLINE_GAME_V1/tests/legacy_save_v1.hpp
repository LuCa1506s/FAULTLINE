#pragma once
#include "persistence/schema_v1.hpp"

namespace faultline::test {
// Frozen Phase 8 encoder, intentionally independent of the v2 section dispatcher.
// Only test code can write the historical format; production always writes current.
inline persistence::SaveBytes legacyV1(const GameState& state) {
    using persistence::detail::Writer;
    auto world=world::persistentSnapshot(state.world); Writer output;
    output(persistence::saveMagic,persistence::SaveHeader{{1},persistence::currentGameVersion,state.metadata.generatorVersion,state.metadata.seed},std::uint32_t{10});
    const auto section=[&](std::uint32_t id,auto body) {
        Writer payload; body(payload); output(id,static_cast<std::uint32_t>(payload.bytes.size()));
        output.bytes.insert(output.bytes.end(),payload.bytes.begin(),payload.bytes.end());
    };
    const auto store=[](Writer& w,const auto& values) {
        w(values.nextId()); w.collection(values.entries().size()); for (const auto& [id,record] : values.entries()) w(id,record);
    };
    const auto components=[](Writer& w,const auto& values) {
        w.collection(values.entries().size()); for (const auto& [id,record] : values.entries()) w(id,record);
    };
    section(1,[&](Writer& w){
        w(state.clock.tick(),state.clock.gameTime().milliseconds,world.spatial.cellSize(),std::uint32_t{7});
        for (const auto& rng : state.randomness.streams) w(rng.state());
    });
    section(2,[&](Writer& w){w(world.base);});
    section(3,[&](Writer& w){
        w(world.entities.nextId()); w.collection(world.entities.entries().size()); for (auto id : world.entities.entries()) w(id);
        components(w,world.transforms); components(w,world.health); components(w,world.vehicleLinks);
    });
    section(4,[&](Writer& w){store(w,world.districts);});
    section(5,[&](Writer& w){store(w,world.territories);});
    section(6,[&](Writer& w){store(w,world.factions);});
    section(7,[&](Writer& w){store(w,world.npcs);});
    section(8,[&](Writer& w){store(w,world.vehicles);});
    section(9,[&](Writer& w){store(w,world.missions);});
    section(10,[&](Writer& w){w(world.factionRelationships,world.npcRelationships);});
    return std::move(output.bytes);
}
}
