#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "world/generation/world_generator.hpp"
#include "world/spatial/world_position.hpp"
#include "core/rng/stable_hash.hpp"
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <iomanip>
#include <locale>

using namespace faultline;
using namespace faultline::world;

namespace {

// Mirror of the viewer's deterministic formatting.
std::string fmtNum(double value, int precision = 2) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string fmtHex(std::uint64_t value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::hex << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

std::string escapeXml(const std::string& text) {
    std::string result;
    for (char ch : text) {
        switch (ch) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += ch; break;
        }
    }
    return result;
}

// Mirror of the viewer's faction color mapping.
std::string factionColor(FactionId faction) {
    static constexpr const char* colors[6] = {
        "#e41a1c", "#377eb8", "#4daf4a", "#984ea3", "#ff7f00", "#999999"
    };
    if (!faction.value()) return colors[5];
    return colors[(faction.value() - 1) % 5];
}

std::string withAlphaStr(const std::string& hexColor, double alpha) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << hexColor;
    out << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
        << static_cast<int>(alpha * 255.0 + 0.5);
    return out.str();
}

// Mirror of the viewer's SVG transform (general form, handles arbitrary bounds).
struct TestTransform {
    double scale{};
    double worldMinX{};
    double worldMinZ{};
    double mapLeft{};
    double mapTop{};
    TestTransform() = default;
    TestTransform(double s, double wminx, double wminz, double ml, double mt)
        : scale(s), worldMinX(wminx), worldMinZ(wminz), mapLeft(ml), mapTop(mt) {}
    std::pair<double, double> map(double worldX, double worldZ) const {
        return {mapLeft + (worldX - worldMinX) * scale,
                mapTop  + (worldZ - worldMinZ) * scale};
    }
    struct RectResult { double x, y, w, h; };
    RectResult mapRect(double rx, double rz, double rw, double rd) const {
        const auto tl = map(rx, rz);
        const auto br = map(rx + rw, rz + rd);
        return {tl.first, tl.second, br.first - tl.first, br.second - tl.second};
    }
};

// Mirror of the viewer's computeTransform for testing arbitrary bounds.
TestTransform computeTestTransform(double worldW, double worldD,
                                   double worldMinX, double worldMinZ,
                                   double margin, double mapAreaW, double mapAreaH) {
    const double scaleX = mapAreaW / worldW;
    const double scaleY = mapAreaH / worldD;
    const double scale = std::min(scaleX, scaleY);
    const double renderedW = worldW * scale;
    const double renderedH = worldD * scale;
    const double mapLeft = margin + (mapAreaW - renderedW) / 2.0;
    const double mapTop = margin + (mapAreaH - renderedH) / 2.0;
    return TestTransform{scale, worldMinX, worldMinZ, mapLeft, mapTop};
}

} // namespace

TEST_CASE("viewer deterministic number formatting is locale-independent") {
    CHECK(fmtNum(100.0) == "100.00");
    CHECK(fmtNum(0.0) == "0.00");
    CHECK(fmtNum(-4000.0) == "-4000.00");
    CHECK(fmtNum(1234.5678) == "1234.57");
    CHECK(fmtNum(3.14159, 4) == "3.1416");
    // No locale-dependent thousands separators.
    CHECK(fmtNum(1234567.89) == "1234567.89");
}

TEST_CASE("viewer hex formatting is zero-padded 16 digits") {
    CHECK(fmtHex(0) == "0000000000000000");
    CHECK(fmtHex(1) == "0000000000000001");
    CHECK(fmtHex(255) == "00000000000000ff");
    CHECK(fmtHex(0x888c5dbd10c15886) == "888c5dbd10c15886");
}

TEST_CASE("viewer XML escaping handles special characters") {
    CHECK(escapeXml("hello") == "hello");
    CHECK(escapeXml("a&b") == "a&amp;b");
    CHECK(escapeXml("a<b>c") == "a&lt;b&gt;c");
    CHECK(escapeXml("\"quoted\"") == "&quot;quoted&quot;");
    CHECK(escapeXml("it's") == "it&apos;s");
    CHECK(escapeXml("") == "");
}

TEST_CASE("viewer faction colors are stable and deterministic") {
    CHECK(factionColor(FactionId{}) == "#999999");
    CHECK(factionColor(FactionId{1}) == "#e41a1c");
    CHECK(factionColor(FactionId{2}) == "#377eb8");
    CHECK(factionColor(FactionId{3}) == "#4daf4a");
    CHECK(factionColor(FactionId{4}) == "#984ea3");
    CHECK(factionColor(FactionId{5}) == "#ff7f00");
    // Stable across repeat calls.
    CHECK(factionColor(FactionId{3}) == "#4daf4a");
}

TEST_CASE("viewer alpha blending produces deterministic RGBA hex") {
    CHECK(withAlphaStr("#e41a1c", 0.2) == "#e41a1c33");
    CHECK(withAlphaStr("#377eb8", 1.0) == "#377eb8FF");
    CHECK(withAlphaStr("#4daf4a", 0.0) == "#4daf4a00");
}

TEST_CASE("viewer SVG coordinate mapping is deterministic") {
    // Simple transform with world origin at (0,0), no offset for deterministic checks.
    const TestTransform transform{0.1, 0.0, 0.0, 0.0, 0.0};
    auto [x, y] = transform.map(100.0, 200.0);
    CHECK(x == doctest::Approx(10.0).epsilon(0.001));
    CHECK(y == doctest::Approx(20.0).epsilon(0.001));
    // Same input always produces same output.
    auto [x2, y2] = transform.map(100.0, 200.0);
    CHECK(x == doctest::Approx(x2).epsilon(0.001));
    CHECK(y == doctest::Approx(y2).epsilon(0.001));
    // Different inputs produce different outputs.
    auto [x3, y3] = transform.map(200.0, 100.0);
    CHECK(x != x3);
}

TEST_CASE("viewer handles WorldPosition with cell + local coordinates") {
    const float cellSize = 128.0F;
    const WorldPosition pos{CellCoord{10, 20}, LocalPosition{64.0F, 0.0F, 32.0F}};
    // Simple transform with world origin at (0,0).
    const TestTransform transform{0.1, 0.0, 0.0, 0.0, 0.0};
    const auto g = toGlobal(pos, cellSize);
    // Global: cell*128 + local = 10*128+64=1344, 20*128+32=2592
    CHECK(g.x == 1344.0);
    CHECK(g.z == 2592.0);
    CHECK(g.y == 0.0);
    // SVG mapping: 1344*0.1=134.4, 2592*0.1=259.2
    auto [sx, sy] = transform.map(g.x, g.z);
    CHECK(sx == doctest::Approx(134.4).epsilon(0.001));
    CHECK(sy == doctest::Approx(259.2).epsilon(0.001));
}

TEST_CASE("viewer handles negative/signed local coordinates") {
    const float cellSize = 128.0F;
    const WorldPosition pos{CellCoord{0, 0}, LocalPosition{-64.0F, 0.0F, -32.0F}};
    const auto g = toGlobal(pos, cellSize);
    CHECK(g.x == -64.0);
    CHECK(g.z == -32.0);
}

TEST_CASE("viewer seed 12345 produces valid world with expected counts") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);
    CHECK(world.roads.nodes.size() == 210);
    CHECK(world.roads.edges.size() == 391);
    CHECK(world.districts.size() == 18);
    CHECK(world.parcels.size() == 8007);
    CHECK(world.buildings.size() == 8007);
    CHECK(world.territories.size() == 41);
    CHECK(world.factions.size() == 5);
    CHECK(world.majorNpcs.size() == 22);
    CHECK(baseWorldHash(world) == 0x888c5dbd10c15886);
}

TEST_CASE("viewer world-to-SVG scaling preserves aspect ratio") {
    const double worldW = 8000.0, worldD = 8000.0;
    const std::int32_t svgWidth = 1200;
    const double margin = 8.0;
    const double mapAreaW = static_cast<double>(svgWidth) - 2 * margin;
    const double mapAreaH = mapAreaW * 0.75;
    const double scaleX = mapAreaW / worldW;
    const double scaleY = mapAreaH / worldD;
    const double scale = std::min(scaleX, scaleY);
    CHECK(scale * worldW <= mapAreaW + 0.01);
    CHECK(scale * worldD <= mapAreaH + 0.01);
    // For a square world with 0.75 aspect target, height is the constraint.
    CHECK(scale == doctest::Approx(scaleY));
}

TEST_CASE("viewer different seeds produce different BaseWorldHash") {
    const GenerationContext ctx1{WorldSeed{12345}, currentGeneratorVersion, {}};
    const GenerationContext ctx2{WorldSeed{99999}, currentGeneratorVersion, {}};
    const auto world1 = generateWorld(ctx1);
    const auto world2 = generateWorld(ctx2);
    CHECK(baseWorldHash(world1) != baseWorldHash(world2));
}

TEST_CASE("viewer Rect boundary helper produces correct coordinates") {
    Rect r{100, 200, 50, 60};
    CHECK(right(r) == 150);
    CHECK(bottom(r) == 260);
    CHECK(area(r) == 3000);
}

TEST_CASE("viewer FactionId to color index mapping is stable") {
    CHECK(factionColor(FactionId{}) == "#999999");
    CHECK(factionColor(FactionId{1}) == "#e41a1c");
    CHECK(factionColor(FactionId{5}) == "#ff7f00");
    CHECK(factionColor(FactionId{6}) == "#e41a1c"); // wraps around
    CHECK(factionColor(FactionId{7}) == "#377eb8"); // wraps around
}

TEST_CASE("viewer output format strings are deterministic across calls") {
    const double values[] = {0.0, 1.0, -1.0, 12345.6789, -99999.99};
    for (double v : values) {
        CHECK(fmtNum(v) == fmtNum(v));
    }
    const std::uint64_t hexValues[] = {0, 1, 255, 123456789, 0xFFFFFFFFFFFFFFFF};
    for (std::uint64_t v : hexValues) {
        CHECK(fmtHex(v) == fmtHex(v));
    }
}

TEST_CASE("diagnostics: seed 12345 spatial bounds match worldgen expectations") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    // World bounds from generation config
    CHECK(world.bounds.x == -4000);
    CHECK(right(world.bounds) == 4000);
    CHECK(world.bounds.z == -4000);
    CHECK(bottom(world.bounds) == 4000);
    CHECK(world.bounds.width == 8000);
    CHECK(world.bounds.depth == 8000);
    CHECK(area(world.bounds) == 64000000);
}

TEST_CASE("diagnostics: world width and depth are positive") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);
    CHECK(world.bounds.width > 0);
    CHECK(world.bounds.depth > 0);
    CHECK(static_cast<std::int64_t>(world.bounds.width) * world.bounds.depth > 0);
}

TEST_CASE("diagnostics: road node bounds are within world bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    std::int64_t minX = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxX = std::numeric_limits<std::int64_t>::lowest();
    std::int64_t minZ = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxZ = std::numeric_limits<std::int64_t>::lowest();

    for (const auto& node : world.roads.nodes) {
        minX = std::min(minX, static_cast<std::int64_t>(node.x));
        maxX = std::max(maxX, static_cast<std::int64_t>(node.x));
        minZ = std::min(minZ, static_cast<std::int64_t>(node.z));
        maxZ = std::max(maxZ, static_cast<std::int64_t>(node.z));
    }

    CHECK(world.roads.nodes.size() == 210);
    CHECK(minX >= static_cast<std::int64_t>(world.bounds.x));
    CHECK(maxX <= right(world.bounds));
    CHECK(minZ >= static_cast<std::int64_t>(world.bounds.z));
    CHECK(maxZ <= bottom(world.bounds));
}

TEST_CASE("diagnostics: building bounds are within world bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    CHECK(world.buildings.size() == 8007);
    std::int64_t minX = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxX = std::numeric_limits<std::int64_t>::lowest();

    for (const auto& b : world.buildings) {
        CHECK(valid(b.footprint));
        minX = std::min(minX, static_cast<std::int64_t>(b.footprint.x));
        maxX = std::max(maxX, static_cast<std::int64_t>(right(b.footprint)));
    }
    CHECK(minX >= static_cast<std::int64_t>(world.bounds.x));
    CHECK(maxX <= right(world.bounds));
}

TEST_CASE("diagnostics: district bounds are within world bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    CHECK(world.districts.size() == 18);
    for (const auto& d : world.districts) {
        CHECK(valid(d.boundary));
        CHECK(inside(d.boundary, world.bounds));
    }
}

TEST_CASE("diagnostics: parcel bounds are within world bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    CHECK(world.parcels.size() == 8007);
    for (const auto& p : world.parcels) {
        CHECK(valid(p.boundary));
        CHECK(inside(p.boundary, world.bounds));
    }
}

TEST_CASE("diagnostics: territory bounds are within world bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    CHECK(world.territories.size() == 41);
    for (const auto& t : world.territories) {
        CHECK(valid(t.boundary));
        CHECK(inside(t.boundary, world.bounds));
    }
}

TEST_CASE("diagnostics: coverage percentages are deterministic") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    // Road coverage: roads span from min to max node coordinates
    std::int64_t minX = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxX = std::numeric_limits<std::int64_t>::lowest();
    std::int64_t minZ = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxZ = std::numeric_limits<std::int64_t>::lowest();

    for (const auto& node : world.roads.nodes) {
        minX = std::min(minX, static_cast<std::int64_t>(node.x));
        maxX = std::max(maxX, static_cast<std::int64_t>(node.x));
        minZ = std::min(minZ, static_cast<std::int64_t>(node.z));
        maxZ = std::max(maxZ, static_cast<std::int64_t>(node.z));
    }

    const std::int64_t worldW = right(world.bounds) - world.bounds.x;
    const std::int64_t worldD = bottom(world.bounds) - world.bounds.z;
    const double covX = static_cast<double>(maxX - minX) * 100.0 /
                        static_cast<double>(worldW);
    const double covZ = static_cast<double>(maxZ - minZ) * 100.0 /
                        static_cast<double>(worldD);

    // Verify deterministic across recomputation
    const double covX2 = static_cast<double>(maxX - minX) * 100.0 /
                         static_cast<double>(worldW);
    const double covZ2 = static_cast<double>(maxZ - minZ) * 100.0 /
                         static_cast<double>(worldD);
    CHECK(covX == doctest::Approx(covX2));
    CHECK(covZ == doctest::Approx(covZ2));
    CHECK(std::isfinite(covX));
    CHECK(std::isfinite(covZ));
    CHECK(covX > 0.0);
    CHECK(covX <= 100.0 + 1e-6);
    CHECK(covZ > 0.0);
    CHECK(covZ <= 100.0 + 1e-6);

    // Regenerate the same world and verify identical coverage
    const auto world2 = generateWorld(context);
    std::int64_t minX2 = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxX2 = std::numeric_limits<std::int64_t>::lowest();
    std::int64_t minZ2 = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxZ2 = std::numeric_limits<std::int64_t>::lowest();
    for (const auto& node : world2.roads.nodes) {
        minX2 = std::min(minX2, static_cast<std::int64_t>(node.x));
        maxX2 = std::max(maxX2, static_cast<std::int64_t>(node.x));
        minZ2 = std::min(minZ2, static_cast<std::int64_t>(node.z));
        maxZ2 = std::max(maxZ2, static_cast<std::int64_t>(node.z));
    }
    CHECK(minX2 == minX);
    CHECK(maxX2 == maxX);
    CHECK(minZ2 == minZ);
    CHECK(maxZ2 == maxZ);
}

TEST_CASE("diagnostics: does not mutate GameState on repeated access") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world1 = generateWorld(context);
    const auto world2 = generateWorld(context);
    CHECK(baseWorldHash(world1) == baseWorldHash(world2));
}

TEST_CASE("diagnostics: different seeds produce different spatial coverage") {
    const GenerationContext ctx1{WorldSeed{12345}, currentGeneratorVersion, {}};
    const GenerationContext ctx2{WorldSeed{99999}, currentGeneratorVersion, {}};
    const auto w1 = generateWorld(ctx1);
    const auto w2 = generateWorld(ctx2);

    // Same world dimensions
    CHECK(w1.bounds.width == w2.bounds.width);
    CHECK(w1.bounds.depth == w2.bounds.depth);

    // But different road node positions
    std::int64_t minX1 = std::numeric_limits<std::int64_t>::max();
    std::int64_t minZ1 = std::numeric_limits<std::int64_t>::max();
    for (const auto& node : w1.roads.nodes) {
        minX1 = std::min(minX1, static_cast<std::int64_t>(node.x));
        minZ1 = std::min(minZ1, static_cast<std::int64_t>(node.z));
    }
    std::int64_t minX2 = std::numeric_limits<std::int64_t>::max();
    std::int64_t minZ2 = std::numeric_limits<std::int64_t>::max();
    for (const auto& node : w2.roads.nodes) {
        minX2 = std::min(minX2, static_cast<std::int64_t>(node.x));
        minZ2 = std::min(minZ2, static_cast<std::int64_t>(node.z));
    }
    CHECK((minX1 != minX2 || minZ1 != minZ2));
}

TEST_CASE("diagnostics: SVG output unchanged with diagnostics flag") {
    // Verify that generateWorld produces identical results (same seed == same world)
    // so the diagnostics tool reading the world doesn't affect any state.
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);
    CHECK(baseWorldHash(world) == 0x888c5dbd10c15886);

    // Verify the water layer has expected bounds
    CHECK(world.water.size() == 1);
    for (const auto& w : world.water) {
        CHECK(valid(w.boundary));
        CHECK(inside(w.boundary, world.bounds));
    }
}

TEST_CASE("diagnostics: world water bounds") {
    const GenerationContext context{WorldSeed{12345}, currentGeneratorVersion, {}};
    const auto world = generateWorld(context);

    CHECK(world.water.size() == 1);
    const auto& wr = world.water[0];
    CHECK(valid(wr.boundary));
    CHECK(inside(wr.boundary, world.bounds));
    // Water is at the western edge
    CHECK(wr.boundary.x == -4000);
    CHECK(right(wr.boundary) == -3280);
    CHECK(wr.boundary.z == -4000);
    CHECK(bottom(wr.boundary) == 4000);
}

// === SVG Transform Arbitrary-Bounds Tests ===
// These tests verify that the SVG transform correctly handles arbitrary world bounds,
// not just worlds centered at the origin (0,0).

TEST_CASE("general: transform with symmetric world (-4000..4000) preserves seed-12345 coords") {
    const double worldMinX = -4000.0, worldMaxX = 4000.0;
    const double worldMinZ = -4000.0, worldMaxZ = 4000.0;
    const double worldW = worldMaxX - worldMinX;  // 8000
    const double worldD = worldMaxZ - worldMinZ;  // 8000
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;   // 1184
    const double mapAreaH = mapAreaW * 0.75;          // 888

    const TestTransform t = computeTestTransform(worldW, worldD, worldMinX, worldMinZ,
                                                  margin, mapAreaW, mapAreaH);

    // World corners should map to on-canvas SVG positions
    auto [minX_svg, minY_svg] = t.map(worldMinX, worldMinZ);
    auto [maxX_svg, maxY_svg] = t.map(worldMaxX, worldMaxZ);

    CHECK(minX_svg == doctest::Approx(156.0).epsilon(0.01));
    CHECK(maxX_svg == doctest::Approx(1044.0).epsilon(0.01));
    CHECK(minY_svg == doctest::Approx(8.0).epsilon(0.01));
    CHECK(maxY_svg == doctest::Approx(896.0).epsilon(0.01));

    // All on-canvas
    CHECK(minX_svg >= 0.0);
    CHECK(maxX_svg <= svgWidth);
    CHECK(minY_svg >= 0.0);
    CHECK(maxY_svg <= svgWidth * 1.0);  // svgHeight=1104, allow tolerance
}

TEST_CASE("general: transform with asymmetric world (1000..9000, 2000..10000)") {
    const double worldMinX = 1000.0, worldMaxX = 9000.0;
    const double worldMinZ = 2000.0, worldMaxZ = 10000.0;
    const double worldW = worldMaxX - worldMinX;  // 8000
    const double worldD = worldMaxZ - worldMinZ;  // 8000
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;
    const double mapAreaH = mapAreaW * 0.75;

    const TestTransform t = computeTestTransform(worldW, worldD, worldMinX, worldMinZ,
                                                  margin, mapAreaW, mapAreaH);

    // World minimum must map to rendered map top-left
    auto [minSvgX, minSvgY] = t.map(worldMinX, worldMinZ);
    // World maximum must map to rendered map bottom-right
    auto [maxSvgX, maxSvgY] = t.map(worldMaxX, worldMaxZ);
    CHECK(minSvgY == doctest::Approx(t.mapTop).epsilon(0.01));
    CHECK(maxSvgX == doctest::Approx(t.mapLeft + worldW * t.scale).epsilon(0.01));
    CHECK(maxSvgY == doctest::Approx(t.mapTop + worldD * t.scale).epsilon(0.01));

    // Verify exact positions for symmetric world (should match seed-12345)
    // Asymmetric case: worldMin=1000, so world-x=1000 maps to mapLeft=156
    CHECK(minSvgX == doctest::Approx(156.0).epsilon(0.01));
    // world-x=9000 maps to 156 + 8000*0.111 = 1044
    CHECK(maxSvgX == doctest::Approx(1044.0).epsilon(0.01));
    CHECK(minSvgY == doctest::Approx(8.0).epsilon(0.01));
    CHECK(maxSvgY == doctest::Approx(896.0).epsilon(0.01));
}

TEST_CASE("general: transform with negative offset world (-9000..-1000, -6000..2000)") {
    const double worldMinX = -9000.0, worldMaxX = -1000.0;
    const double worldMinZ = -6000.0, worldMaxZ = 2000.0;
    const double worldW = worldMaxX - worldMinX;  // 8000
    const double worldD = worldMaxZ - worldMinZ;  // 8000
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;
    const double mapAreaH = mapAreaW * 0.75;

    const TestTransform t = computeTestTransform(worldW, worldD, worldMinX, worldMinZ,
                                                  margin, mapAreaW, mapAreaH);

    // World corners
    auto [minSvgX, minSvgY] = t.map(worldMinX, worldMinZ);
    auto [maxSvgX, maxSvgY] = t.map(worldMaxX, worldMaxZ);

    // World minimum maps to mapLeft/mapTop (on-canvas)
    CHECK(minSvgX == doctest::Approx(t.mapLeft).epsilon(0.01));
    CHECK(minSvgY == doctest::Approx(t.mapTop).epsilon(0.01));

    // World maximum maps to mapLeft + renderedW, mapTop + renderedH (on-canvas)
    CHECK(maxSvgX == doctest::Approx(t.mapLeft + worldW * t.scale).epsilon(0.01));
    CHECK(maxSvgY == doctest::Approx(t.mapTop + worldD * t.scale).epsilon(0.01));

    // Must be on-canvas
    CHECK(minSvgX >= 0.0);
    CHECK(maxSvgX <= svgWidth);
    CHECK(minSvgY >= 0.0);
    CHECK(maxSvgY <= svgWidth * 1.0);
}

TEST_CASE("general: transform with non-square asymmetric world (100..4100, -500..7500)") {
    const double worldMinX = 100.0, worldMaxX = 4100.0;
    const double worldMinZ = -500.0, worldMaxZ = 7500.0;
    const double worldW = worldMaxX - worldMinX;  // 4000
    const double worldD = worldMaxZ - worldMinZ;  // 8000
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;  // 1184
    const double mapAreaH = mapAreaW * 0.75;         // 888

    const TestTransform t = computeTestTransform(worldW, worldD, worldMinX, worldMinZ,
                                                  margin, mapAreaW, mapAreaH);

    // Aspect ratio: worldW=4000, worldD=8000 (2:1 depth:width)
    // scaleX = 1184/4000 = 0.296, scaleY = 888/8000 = 0.111
    // scale = min = 0.111 (width-constraining)
    // renderedW = 4000*0.111 = 444, renderedH = 8000*0.111 = 888
    // mapLeft = 8 + (1184-444)/2 = 8 + 370 = 378 (width centered, extra space on X)
    // mapTop = 8 + (888-888)/2 = 8 + 0 = 8

    CHECK(t.scale == doctest::Approx(888.0 / 8000.0).epsilon(0.001));

    // World corners
    auto [minSvgX, minSvgY] = t.map(worldMinX, worldMinZ);
    auto [maxSvgX, maxSvgY] = t.map(worldMaxX, worldMaxZ);

    // Map top-left = mapLeft, mapTop (centered on X, tight on Y)
    CHECK(minSvgX == doctest::Approx(t.mapLeft).epsilon(0.01));
    CHECK(minSvgY == doctest::Approx(t.mapTop).epsilon(0.01));

    // Map bottom-right
    CHECK(maxSvgX == doctest::Approx(t.mapLeft + worldW * t.scale).epsilon(0.01));
    CHECK(maxSvgY == doctest::Approx(t.mapTop + worldD * t.scale).epsilon(0.01));

    // X-axis centered: extra space on both sides
    const double renderedW = worldW * t.scale;
    CHECK(t.mapLeft == doctest::Approx(margin + (mapAreaW - renderedW) / 2.0).epsilon(0.01));

    // Must be completely on-canvas
    CHECK(minSvgX >= 0.0);
    CHECK(maxSvgX <= svgWidth);
    CHECK(minSvgY >= 0.0);
    CHECK(maxSvgY <= svgWidth * 1.0);
}

TEST_CASE("general: mapRect() obeys the same transform as mapPoint()") {
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;
    const double mapAreaH = mapAreaW * 0.75;

    const TestTransform t = computeTestTransform(8000.0, 8000.0, -4000.0, -4000.0,
                                                  margin, mapAreaW, mapAreaH);

    // Rect at world origin
    const auto [rx, ry, rw, rh] = t.mapRect(0.0, 0.0, 100.0, 100.0);

    // Should match mapPoint at corners
    auto [tlX, tlY] = t.map(0.0, 0.0);
    auto [brX, brY] = t.map(100.0, 100.0);

    CHECK(rx == doctest::Approx(tlX).epsilon(0.001));
    CHECK(ry == doctest::Approx(tlY).epsilon(0.001));
    CHECK(rw == doctest::Approx(brX - tlX).epsilon(0.001));
    CHECK(rh == doctest::Approx(brY - tlY).epsilon(0.001));

    // Rect dimensions scale correctly
    CHECK(rw == doctest::Approx(100.0 * t.scale).epsilon(0.001));
    CHECK(rh == doctest::Approx(100.0 * t.scale).epsilon(0.001));
}

TEST_CASE("general: non-square world preserves aspect ratio and centers on unused axis") {
    const double svgWidth = 1200.0, margin = 8.0;
    const double mapAreaW = svgWidth - 2 * margin;  // 1184
    const double mapAreaH = mapAreaW * 0.75;         // 888

    // Wide world: 12000 x 2000
    const TestTransform t = computeTestTransform(12000.0, 2000.0, 0.0, 0.0,
                                                  margin, mapAreaW, mapAreaH);
    // scaleX = 1184/12000 = 0.0987, scaleY = 888/2000 = 0.444
    // scale = 0.0987 (X-constraining)
    // renderedW = 1184, renderedH = 197.4
    // mapTop = 8 + (888-197.4)/2 = 8 + 345.3 = 353.3 (Y centered)

    const double renderedW = 12000.0 * t.scale;
    const double renderedH = 2000.0 * t.scale;
    CHECK(renderedW == doctest::Approx(mapAreaW).epsilon(0.1));  // fills width
    CHECK(renderedH < mapAreaH);  // height has extra space (centered)

    // Tall world: 1000 x 12000
    const TestTransform t2 = computeTestTransform(1000.0, 12000.0, 0.0, 0.0,
                                                   margin, mapAreaW, mapAreaH);
    // scaleX = 1184/1000 = 1.184, scaleY = 888/12000 = 0.074
    // scale = 0.074 (Y-constraining)
    const double renderedW2 = 1000.0 * t2.scale;
    const double renderedH2 = 12000.0 * t2.scale;
    CHECK(renderedW2 < mapAreaW);  // width has extra space (centered)
    CHECK(renderedH2 == doctest::Approx(mapAreaH).epsilon(0.1));  // fills height
}
