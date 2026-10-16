#include "persistence/schema_v4.hpp"
#include "simulation/factions/faction_system.hpp"
#include "simulation/npc/npc_system.hpp"
#include "simulation/territory/territory_system.hpp"
#include "world/spatial/streaming.hpp"
#include <fstream>
#include <sstream>

namespace faultline::persistence {
SaveError::SaveError(SaveErrorCode errorCode, std::string errorSection, std::size_t errorOffset,
                     std::string message, SaveHeader saveHeader)
    : std::runtime_error("save v"+std::to_string(saveHeader.saveFormatVersion.value)+" generator v"+
        std::to_string(saveHeader.generatorVersion.value)+" section="+errorSection+" offset="+
        std::to_string(errorOffset)+": "+message), code(errorCode), section(std::move(errorSection)),
        offset(errorOffset), header(saveHeader) {}
namespace {
using detail::Writer;
using detail::Reader;
enum class Section : std::uint32_t { Runtime=1, Generated, Entities, Districts, Territories, Factions, Npcs, Vehicles, Missions, Relationships, FactionSimulation, NpcSimulation, TerritorySimulation };
constexpr std::uint32_t legacySectionCount=10;
constexpr std::uint32_t sectionCount=13;
void header(Reader& r) {
    r.need(saveMagic.size());
    for (auto expected : saveMagic) {
        std::uint8_t value{}; r(value); if (value!=expected) r.fail(SaveErrorCode::InvalidMagic,"Not a FAULTLINE save");
    }
    // Check each version immediately, before interpreting later fields.
    r(r.header.saveFormatVersion);
    if (r.header.saveFormatVersion.value<1 || r.header.saveFormatVersion.value>currentSaveFormatVersion.value)
        r.fail(SaveErrorCode::UnsupportedSaveVersion,"Unsupported save version");
    r(r.header.gameVersion,r.header.generatorVersion,r.header.worldSeed);
    if (r.header.generatorVersion.value<1 || r.header.generatorVersion.value>currentGeneratorVersion.value)
        r.fail(SaveErrorCode::UnsupportedGeneratorVersion,"Unsupported generator version");
    if (r.header.gameVersion!=currentGameVersion) r.fail(SaveErrorCode::UnsupportedGameVersion,"Unsupported game version (no migration defined)");
}
template<class Fn> void section(Writer& w, Section id, Fn fn) {
    Writer body; fn(body); w(id,static_cast<std::uint32_t>(body.bytes.size()));
    if (body.objects>300000-w.objects) w.fail("Global collection resource limit");
    w.objects+=body.objects;
    w.bytes.insert(w.bytes.end(),body.bytes.begin(),body.bytes.end());
    if (w.bytes.size()>maxSaveBytes) w.fail("Save resource limit");
}
template<class Fn> void section(Reader& r, Section expected, const char* name, Fn fn) {
    r.section=name; Section id{}; std::uint32_t length{}; r(id,length);
    if (id!=expected) r.fail(SaveErrorCode::InvalidData,"Missing, duplicate or out-of-order section");
    r.need(length);
    // Restrict every read to this section without copying input or overflowing offsets.
    const auto whole=r.bytes; const auto end=r.offset+length; r.bytes=whole.first(end);
    fn(r);
    if (r.offset!=end) r.fail(SaveErrorCode::InvalidData,"Unexpected section payload/trailing fields");
    r.bytes=whole;
}
template<class Store> void writeStore(Writer& w, const Store& store) {
    w(store.nextId()); w.collection(store.entries().size());
    for (const auto& [id,value] : store.entries()) w(id,value);
}
template<class Store> void readStore(Reader& r, Store& store) {
    std::uint64_t next{}; r(next); const auto count=r.count();
    using Map=std::remove_cvref_t<decltype(store.entries())>;
    Map values; typename Map::key_type previous{};
    for (std::uint32_t i=0; i<count; ++i) {
        typename Map::key_type id{}; typename Map::mapped_type value{}; r(id,value);
        if (!id || id<=previous) r.fail(SaveErrorCode::InvalidData,"Null, duplicate or unordered persistent ID");
        previous=id; values.emplace(id,std::move(value));
    }
    store=Store::restore(next,std::move(values));
}
template<class T> void writeComponents(Writer& w, const world::ComponentStore<T>& store) {
    w.collection(store.entries().size());
    for (const auto& [id,value] : store.entries()) w(id,value);
}
template<class T> void readComponents(Reader& r, world::WorldState& world, world::ComponentStore<T>& store) {
    const auto count=r.count(); EntityId previous{};
    for (std::uint32_t i=0; i<count; ++i) {
        EntityId id{}; T value{}; r(id,value);
        if (!id || id<=previous) r.fail(SaveErrorCode::InvalidData,"Null, duplicate or unordered component ID");
        previous=id; store.set(world.entities,id,std::move(value));
    }
}
void validateGeneratedOrder(const world::WorldState& world) {
    // Worldgen validation assumes generated vectors retain generation order. Reject
    // alternate permutations instead of changing historical BaseWorldHash contracts.
    if (world.base) {
        const auto ordered = [&](const auto& values) {
            std::uint64_t previous=0;
            for (const auto& value : values) {
                core::require(value.id.value()>previous,"Generated IDs must be strictly ascending");
                previous=value.id.value();
            }
        };
        const auto& b=*world.base;
        ordered(b.water); ordered(b.macroZones); ordered(b.roads.nodes); ordered(b.roads.edges); ordered(b.districts);
        ordered(b.parcels); ordered(b.buildings); ordered(b.territories); ordered(b.factions); ordered(b.majorNpcs);
    }
}
void validateLoaded(GameState& state, Reader& r) {
    validateGeneratedOrder(state.world);
    validateState(state);
    // Only abstract NPC representations belong to this format. The loader never
    // repairs invalid physical links or silently chooses a replacement identity.
    if (!state.world.npcLinks.entries().empty()) r.fail(SaveErrorCode::InvalidData,"NPC bodies are transient");
    for (const auto& [id,npc] : state.world.npcs.entries()) {
        (void)id; if (npc.activeEntity) r.fail(SaveErrorCode::InvalidData,"Saved NPC must be abstract");
    }
    world::rebuildSpatialIndex(state.world);
}
}
SaveHeader readSaveHeader(std::span<const std::uint8_t> bytes) {
    Reader r{bytes}; header(r); return r.header;
}
SaveBytes saveGameState(const GameState& state) {
    try {
        validateState(state);
        validateGeneratedOrder(state.world);
        auto world=world::persistentSnapshot(state.world);
        Writer w; for (auto byte : saveMagic) w(byte);
        w(SaveHeader{currentSaveFormatVersion,currentGameVersion,state.metadata.generatorVersion,state.metadata.seed},sectionCount);
        section(w,Section::Runtime,[&](Writer& a) {
            a(state.clock.tick(),state.clock.gameTime().milliseconds,world.spatial.cellSize());
            a(static_cast<std::uint32_t>(state.randomness.streams.size()));
            for (const auto& rng : state.randomness.streams) a(rng.state());
        });
        section(w,Section::Generated,[&](Writer& a){ a(world.base); });
        section(w,Section::Entities,[&](Writer& a){
            a(world.entities.nextId()); a.collection(world.entities.entries().size());
            for (auto id : world.entities.entries()) a(id);
            writeComponents(a,world.transforms); writeComponents(a,world.health);
            writeComponents(a,world.vehicleLinks);
        });
        section(w,Section::Districts,[&](Writer& a){ writeStore(a,world.districts); });
        section(w,Section::Territories,[&](Writer& a){ writeStore(a,world.territories); });
        section(w,Section::Factions,[&](Writer& a){ writeStore(a,world.factions); });
        section(w,Section::Npcs,[&](Writer& a){ writeStore(a,world.npcs); });
        section(w,Section::Vehicles,[&](Writer& a){ writeStore(a,world.vehicles); });
        section(w,Section::Missions,[&](Writer& a){ writeStore(a,world.missions); });
        section(w,Section::Relationships,[&](Writer& a){ a(world.factionRelationships,world.npcRelationships); });
        section(w,Section::FactionSimulation,[&](Writer& a){
            a(state.factionSimulation.config,state.factionSimulation.nextEvaluationTick);
            writeStore(a,state.factionSimulation.operations);
            a.collection(world.factions.entries().size());
            for (const auto& [id,faction] : world.factions.entries()) a(id,faction.strategy);
        });
        section(w,Section::NpcSimulation,[&](Writer& a){
            a(state.npcSimulation.config,state.npcSimulation.statistics);
            a.collection(world.npcs.entries().size());
            for (const auto& [id,npc] : world.npcs.entries()) a(id,npc.semantic);
        });
        section(w,Section::TerritorySimulation,[&](Writer& a){
            const auto& s=state.territorySimulation; a(s.rules,s.statistics); a.collection(s.territories.size());
            for (const auto& [id,t] : s.territories) {
                a(id,t.garrisonFaction,t.incumbent); a.collection(t.entrenchment.size());
                for (const auto& [f,value] : t.entrenchment) a(f,value);
            }
            a.collection(s.appliedOperations.size()); for (auto id : s.appliedOperations) a(id);
        });
        return std::move(w.bytes);
    } catch (const SaveError&) { throw; }
    catch (const std::exception& e) {
        throw SaveError{SaveErrorCode::ValidationFailure,"snapshot",0,e.what(),
            {currentSaveFormatVersion,currentGameVersion,state.metadata.generatorVersion,state.metadata.seed}};
    }
}
GameState loadGameState(std::span<const std::uint8_t> bytes) {
    Reader r{bytes};
    if (bytes.size()>maxSaveBytes) r.fail(SaveErrorCode::InvalidData,"Save resource limit");
    try {
        header(r); std::uint32_t count{}; r(count);
        if (count!=(r.header.saveFormatVersion.value==1 ? legacySectionCount : r.header.saveFormatVersion.value==2 ? 11U : r.header.saveFormatVersion.value==3 ? 12U : sectionCount))
            r.fail(SaveErrorCode::InvalidData,"Incorrect section count");
        world::WorldState world; RuntimeRandomState random{r.header.worldSeed}; core::SimulationClock clock;
        section(r,Section::Runtime,"runtime",[&](Reader& a){
            std::uint64_t tick{},milliseconds{}; float cellSize{}; std::uint32_t streams{}; a(tick,milliseconds,cellSize,streams);
            clock=core::SimulationClock{tick};
            if (clock.gameTime().milliseconds!=milliseconds) a.fail(SaveErrorCode::InvalidData,"Tick/game-time mismatch");
            world.spatial=world::SpatialGrid{cellSize};
            if (streams!=random.streams.size()) a.fail(SaveErrorCode::InvalidData,"Incorrect RNG stream count");
            for (auto& rng : random.streams) { core::DeterministicRng::State words{}; a(words); rng.restore(words); }
        });
        section(r,Section::Generated,"generated",[&](Reader& a){ a(world.base); });
        section(r,Section::Entities,"entities",[&](Reader& a){
            std::uint64_t next{}; a(next); const auto size=a.count(); std::set<EntityId> live; EntityId previous{};
            for (std::uint32_t i=0; i<size; ++i) {
                EntityId id{}; a(id);
                if (!id || id<=previous) a.fail(SaveErrorCode::InvalidData,"Null, duplicate or unordered entity ID");
                previous=id; live.insert(id);
            }
            world.entities=world::EntityRegistry::restore(next,std::move(live));
            readComponents(a,world,world.transforms); readComponents(a,world,world.health); readComponents(a,world,world.vehicleLinks);
        });
        section(r,Section::Districts,"districts",[&](Reader& a){ readStore(a,world.districts); });
        section(r,Section::Territories,"territories",[&](Reader& a){ readStore(a,world.territories); });
        section(r,Section::Factions,"factions",[&](Reader& a){ readStore(a,world.factions); });
        section(r,Section::Npcs,"npcs",[&](Reader& a){ readStore(a,world.npcs); });
        section(r,Section::Vehicles,"vehicles",[&](Reader& a){ readStore(a,world.vehicles); });
        section(r,Section::Missions,"missions",[&](Reader& a){ readStore(a,world.missions); });
        section(r,Section::Relationships,"relationships",[&](Reader& a){ a(world.factionRelationships,world.npcRelationships); });
        simulation::FactionSimulationState factionSimulation;
        if (r.header.saveFormatVersion.value>=2) {
            section(r,Section::FactionSimulation,"faction simulation",[&](Reader& a){
                a(factionSimulation.config,factionSimulation.nextEvaluationTick); readStore(a,factionSimulation.operations);
                const auto size=a.count();
                if (size!=world.factions.entries().size()) a.fail(SaveErrorCode::InvalidData,"Faction strategy count mismatch");
                FactionId previous{};
                for (std::uint32_t i=0;i<size;++i) {
                    FactionId id{}; world::FactionStrategicState strategy; a(id,strategy);
                    if (!id || id<=previous || !world.factions.exists(id)) a.fail(SaveErrorCode::InvalidData,"Invalid faction strategy key");
                    previous=id; world.factions.get(id).strategy=strategy;
                }
            });
        }
        simulation::NpcSimulationState npcSimulation;
        std::map<NpcId,world::NpcSemanticState> npcSemantics;
        if (r.header.saveFormatVersion.value>=3) {
            section(r,Section::NpcSimulation,"NPC simulation",[&](Reader& a){
                a(npcSimulation.config,npcSimulation.statistics); const auto size=a.count();
                if (size!=world.npcs.entries().size()) a.fail(SaveErrorCode::InvalidData,"NPC runtime count mismatch");
                NpcId previous{};
                for (std::uint32_t i=0;i<size;++i) {
                    NpcId id{}; world::NpcSemanticState semantic; a(id,semantic);
                    if (!id || id<=previous || !world.npcs.exists(id)) a.fail(SaveErrorCode::InvalidData,"Invalid NPC runtime key");
                    previous=id; npcSemantics.emplace(id,std::move(semantic));
                }
            });
        }
        simulation::TerritorySimulationState territorySimulation;
        if (r.header.saveFormatVersion.value>=4) {
            section(r,Section::TerritorySimulation,"territory simulation",[&](Reader& a){
                a(territorySimulation.rules,territorySimulation.statistics); const auto size=a.count();
                if (size!=world.territories.entries().size()) a.fail(SaveErrorCode::InvalidData,"Territory runtime count mismatch");
                TerritoryId previous{};
                for (std::uint32_t i=0;i<size;++i) {
                    TerritoryId id{}; simulation::TerritoryRuntimeState t; a(id,t.garrisonFaction,t.incumbent);
                    if (!id || id<=previous || !world.territories.exists(id)) a.fail(SaveErrorCode::InvalidData,"Invalid territory runtime key");
                    previous=id; const auto n=a.count(); FactionId prior{};
                    for (std::uint32_t j=0;j<n;++j) {
                        FactionId f{}; std::uint32_t value{}; a(f,value);
                        if (!f || f<=prior) a.fail(SaveErrorCode::InvalidData,"Duplicate/unordered territorial presence");
                        prior=f; t.entrenchment.emplace(f,value);
                    }
                    territorySimulation.territories.emplace(id,std::move(t));
                }
                const auto n=a.count(); OperationId prior{};
                for (std::uint32_t i=0;i<n;++i) { OperationId id{}; a(id);
                    if (!id || id<=prior) a.fail(SaveErrorCode::InvalidData,"Duplicate/unordered consumed operation");
                    prior=id; territorySimulation.appliedOperations.insert(id);
                }
            });
        }
        if (r.remaining()!=0) r.fail(SaveErrorCode::InvalidData,"Trailing save data");
        r.section="semantic validation";
        GameState state{{r.header.worldSeed,r.header.generatorVersion},clock,std::move(random),std::move(world)};
        if (r.header.saveFormatVersion.value>=2) {
            state.factionSimulation=std::move(factionSimulation);
            simulation::rebuildFactionIndexes(state);
        }
        // A real v1 migration: constructor defaults create no operations, default
        // confidence, allocator 1, and the first evaluation one interval after the
        // saved tick. Generated identity, old resources, RNG and clock are untouched.
        if (r.header.saveFormatVersion.value>=3) {
            state.npcSimulation=std::move(npcSimulation);
            for (auto& [id,semantic] : npcSemantics) state.world.npcs.get(id).semantic=std::move(semantic);
            simulation::rebuildNpcSchedule(state);
        }
        // v2 -> v3 defaults are initialized by GameState: role-safe idle/restricted
        // activity, current location as home, no travel/assignment, next update
        // savedTick+30 game minutes. Migration consumes no random numbers.
        if (r.header.saveFormatVersion.value>=4) {
            state.territorySimulation=std::move(territorySimulation);
            simulation::rebuildTerritoryControlCache(state);
        } else simulation::markHistoricalTerritoryResults(state);
        validateLoaded(state,r); return state;
    } catch (const SaveError&) { throw; }
    catch (const std::exception& e) { r.fail(SaveErrorCode::ValidationFailure,e.what()); }
}
void saveGameStateFile(const GameState& state, const std::filesystem::path& path) {
    const auto bytes=saveGameState(state); // Validation must precede truncating an existing save.
    std::ofstream output{path,std::ios::binary|std::ios::trunc};
    if (!output) throw SaveError{SaveErrorCode::IoError,"file",0,"Cannot open output"};
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); output.flush();
    if (!output) throw SaveError{SaveErrorCode::IoError,"file",0,"Cannot write/flush save"};
    output.close(); if (!output) throw SaveError{SaveErrorCode::IoError,"file",0,"Cannot close save"};
}
GameState loadGameStateFile(const std::filesystem::path& path) {
    std::ifstream input{path,std::ios::binary|std::ios::ate};
    if (!input) throw SaveError{SaveErrorCode::IoError,"file",0,"Cannot open input"};
    const auto size=input.tellg();
    if (size<0 || static_cast<std::uint64_t>(size)>maxSaveBytes) throw SaveError{SaveErrorCode::InvalidData,"file",0,"Invalid file size"};
    SaveBytes bytes(static_cast<std::size_t>(size)); input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!input) throw SaveError{SaveErrorCode::IoError,"file",0,"Cannot read save"};
    return loadGameState(bytes);
}
}
