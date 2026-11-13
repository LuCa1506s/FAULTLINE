#include "doctest.h"
#include "world/generation/urban_generation.hpp"
#include "world/geometry/rect_index.hpp"
#include "core/rng/deterministic_rng.hpp"
#include "simulation/runtime/game.hpp"
#include <map>
#include <set>
#include <type_traits>

using namespace faultline;
using namespace faultline::world;
static_assert(!std::is_convertible_v<ParcelId, BuildingId>);

TEST_CASE("integer rectangle polygons have exact area containment and intersections") {
    const Rect a{-20,-10,30,20}, b{0,0,20,10};
    CHECK(area(a) == 600); CHECK(right(a) == 10); CHECK(bottom(a) == 10);
    CHECK(intersection(a,b) == Rect{0,0,10,10}); CHECK(overlap(a,b));
    CHECK_FALSE(overlap(a,Rect{10,-10,10,20})); CHECK(adjacent(a,Rect{10,-10,10,20}));
    CHECK(inside(inset(a,2),a)); CHECK_FALSE(valid(Rect{0,0,0,10}));
    CHECK_FALSE(valid(Rect{std::numeric_limits<std::int32_t>::max(),0,10,10}));
    CHECK_FALSE(intersection(a,Rect{100,100,10,10}).has_value());
    CHECK_THROWS((void)inset(a,10));
}
TEST_CASE("AABB index matches brute force overlap queries") {
    core::DeterministicRng rng{42}; std::vector<Rect> rectangles;
    for (int i = 0; i < 200; ++i) rectangles.push_back({static_cast<std::int32_t>(rng.uniform(2000))-1000,
        static_cast<std::int32_t>(rng.uniform(2000))-1000, static_cast<std::int32_t>(rng.uniform(1,100)), static_cast<std::int32_t>(rng.uniform(1,100))});
    RectIndex index{rectangles};
    for (int i = 0; i < 100; ++i) {
        const Rect q{static_cast<std::int32_t>(rng.uniform(2000))-1000,static_cast<std::int32_t>(rng.uniform(2000))-1000,100,100};
        std::vector<std::size_t> expected;
        for (std::size_t j = 0; j < rectangles.size(); ++j) if (overlap(q,rectangles[j])) expected.push_back(j);
        CHECK(index.query(q) == expected);
    }
    CHECK(RectIndex{std::span<const Rect>{}}.query({0,0,1,1}).empty());
}
TEST_CASE("M2 independently reproduces parcels buildings territories and complete hash") {
    const GenerationContext ctx{{12345},{2},{}};
    const auto a = generateWorld(ctx), b = generateWorld(ctx);
    CHECK(a.parcels == b.parcels); CHECK(a.buildings == b.buildings); CHECK(a.territories == b.territories);
    CHECK(parcelLayoutHash(a.parcels) == parcelLayoutHash(b.parcels));
    CHECK(buildingLayoutHash(a.buildings) == buildingLayoutHash(b.buildings));
    CHECK(territoryLayoutHash(a.territories) == territoryLayoutHash(b.territories));
    CHECK(baseWorldHash(a) == baseWorldHash(b)); CHECK(validateGeneratedWorld(a).passed());
    CHECK(baseWorldHash(a) != baseWorldHash(generateWorld({{54321},{2},{}})));
    CHECK_FALSE(a.parcels.empty()); CHECK_FALSE(a.buildings.empty()); CHECK_FALSE(a.territories.empty());
    CHECK(generateParcels(ctx,a).parcels == a.parcels);
    CHECK(generateBuildings(ctx,a.districts,a.parcels).buildings == a.buildings);
    CHECK(generateTerritories(ctx,a,a.parcels,a.buildings).territories == a.territories);
}
TEST_CASE("M2 preserves version 1 canonical topology and hash") {
    const auto m1 = generateWorld({{12345},{1},{}}); auto m2 = generateWorld({{12345},{2},{}});
    m2.metadata.generatorVersion = {1}; m2.parcels.clear(); m2.buildings.clear(); m2.territories.clear();
    CHECK(baseWorldHash(m1) == 0x2938f0ca42162bc9ULL); CHECK(baseWorldHash(m2) == baseWorldHash(m1));
    CHECK(validateGeneratedWorld(m2).passed());
}
TEST_CASE("every urban object has stable domain references and valid integer geometry") {
    const auto world = generateWorld({{12345},{2},{}});
    std::map<DistrictId,Rect> districts; for (const auto& d : world.districts) districts.emplace(d.id,d.boundary);
    std::map<ParcelId,Parcel> parcels;
    for (const auto& p : world.parcels) {
        CHECK(static_cast<bool>(p.id)); CHECK(parcels.emplace(p.id,p).second); REQUIRE(districts.contains(p.district));
        CHECK(inside(p.boundary,districts.at(p.district))); CHECK(p.areaSquareMeters == static_cast<std::uint64_t>(area(p.boundary)));
        CHECK(p.areaSquareMeters > 0); CHECK(static_cast<bool>(p.frontage.road)); CHECK(p.frontage.length >= world.config.urban.minParcelSpan);
    }
    std::set<BuildingId> buildingIds;
    for (const auto& b : world.buildings) {
        CHECK(static_cast<bool>(b.id)); CHECK(buildingIds.insert(b.id).second); REQUIRE(parcels.contains(b.parcel));
        const auto& p = parcels.at(b.parcel); CHECK(b.district == p.district); CHECK(inside(b.footprint,p.boundary));
        CHECK(area(b.footprint) > 0); CHECK(b.transform.front == p.frontage.side);
    }
    std::set<TerritoryId> territoryIds; std::map<DistrictId,std::int64_t> coverage;
    for (const auto& t : world.territories) {
        CHECK(static_cast<bool>(t.id)); CHECK(territoryIds.insert(t.id).second); REQUIRE(districts.contains(t.district));
        CHECK(inside(t.boundary,districts.at(t.district))); CHECK(area(t.boundary) > 0); coverage[t.district] += area(t.boundary);
    }
    for (const auto& [id,bounds] : districts) CHECK(coverage.at(id) == area(bounds));
}
TEST_CASE("building rule changes and stream consumption cannot move topology or parcels") {
    GenerationContext ctx{{12345},{2},{}}; const auto original = generateWorld(ctx);
    core::DeterministicRng buildingNoise{ctx.stageSeed("buildings")};
    for (int i = 0; i < 1000; ++i) (void)buildingNoise.nextU64();
    ctx.config.urban.residential = ctx.config.urban.commercial = ctx.config.urban.industrial = ctx.config.urban.park = {0,0,0,0,0,0,100};
    auto changed = generateWorld(ctx);
    CHECK(changed.parcels == original.parcels);
    CHECK(buildingLayoutHash(changed.buildings) != buildingLayoutHash(original.buildings));
    for (const auto& b : changed.buildings) CHECK(b.archetype == BuildingArchetype::Garage);
    changed.metadata.generatorVersion = {1}; changed.parcels.clear(); changed.buildings.clear(); changed.territories.clear();
    CHECK(baseWorldHash(changed) == baseWorldHash(generateWorld({{12345},{1},{}})));
}
TEST_CASE("M2 accepts micro worlds coast variants large maps and extreme origins") {
    for (const auto size : {512,8000,16000}) for (const auto water : {0U,100U}) {
        WorldGenerationConfig config; config.width = config.depth = size; config.waterPercent = water;
        config.originX = -120; config.originZ = 234;
        CHECK(validateGeneratedWorld(generateWorld({{42},{2},config})).passed());
    }
    WorldGenerationConfig config; config.width = config.depth = 512;
    config.originX = std::numeric_limits<std::int32_t>::min();
    config.originZ = std::numeric_limits<std::int32_t>::max() - 512;
    CHECK(validateGeneratedWorld(generateWorld({{7},{2},config})).passed());
}
TEST_CASE("M2 config and finite resource limits fail explicitly") {
    GenerationContext ctx{{12345},{2},{}}; ctx.config.urban.maxParcels = 1;
    CHECK_THROWS((void)generateWorld(ctx));
    ctx.config.urban = {}; ctx.config.urban.roadHalfWidths[0] = 0; CHECK_THROWS((void)generateWorld(ctx));
    ctx.config.urban = {}; ctx.config.urban.buildingSetback = 100; CHECK_THROWS((void)generateWorld(ctx));
    ctx.config.urban = {}; ctx.config.urban.residential = {}; CHECK_THROWS((void)generateWorld(ctx));
}
TEST_CASE("urban validator rejects corrupt parcel building and territory records") {
    const auto base = generateWorld({{12345},{2},{}});
    const auto rejected = [](const GeneratedWorld& world) { CHECK_FALSE(validateGeneratedWorld(world).passed()); };
    auto bad = base; bad.parcels[0].district = DistrictId{99999}; rejected(bad);
    bad = base; bad.parcels[1].id = bad.parcels[0].id; rejected(bad);
    bad = base; bad.parcels[0].boundary.width = 0; rejected(bad);
    bad = base; ++bad.parcels[0].areaSquareMeters; rejected(bad);
    bad = base; bad.parcels[0].frontage.road = RoadEdgeId{999999}; rejected(bad);
    bad = base; ++bad.parcels[0].frontage.start; rejected(bad);
    bad = base; bad.parcels[0].boundary = base.bounds; bad.parcels[0].areaSquareMeters = static_cast<std::uint64_t>(area(base.bounds)); rejected(bad);
    bad = base; auto p = bad.parcels[0]; p.id = ParcelId{bad.parcels.size()+1}; bad.parcels.push_back(p); rejected(bad);
    bad = base; bad.buildings[0].parcel = ParcelId{999999}; rejected(bad);
    bad = base; bad.buildings[1].id = bad.buildings[0].id; rejected(bad);
    bad = base; bad.buildings[0].archetype = static_cast<BuildingArchetype>(255); rejected(bad);
    bad = base; ++bad.buildings[0].transform.centerX2; rejected(bad);
    bad = base; bad.buildings[0].footprint = base.bounds; rejected(bad);
    bad = base; bad.territories[0].district = DistrictId{999999}; rejected(bad);
    bad = base; bad.territories[1].id = bad.territories[0].id; rejected(bad);
    bad = base; --bad.territories[0].boundary.width; rejected(bad);
    bad = base; ++bad.territories[0].buildingCount; rejected(bad);
    bad = base; ++bad.territories[0].buildableArea; rejected(bad);
    bad = base; bad.territories.pop_back(); rejected(bad);
}
TEST_CASE("M2 buildings stay semantic and generated territories initialize runtime store") {
    Game game{WorldSeed{12345}};
    REQUIRE(game.state().world.base.has_value());
    CHECK(game.state().world.entities.entries().empty());
    CHECK(game.state().world.base->metadata.generatorVersion == currentGeneratorVersion);
    CHECK(game.state().world.territories.entries().size() == game.state().world.base->territories.size());
    game.requestValidation(0); CHECK_NOTHROW(game.tick());
}
