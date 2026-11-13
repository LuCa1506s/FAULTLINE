#include "simulation/runtime/game.hpp"
#include "world/generation/world_generator.hpp"
#include "world/spatial/world_position.hpp"
#include "simulation/territory/territory_system.hpp"
#include "simulation/player/player_system.hpp"
#include "world/spatial/streaming.hpp"
#include "persistence/save.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>

using namespace faultline;
using namespace faultline::world;
using namespace faultline::simulation;

namespace {

std::uint64_t toNumber(std::string_view text) {
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::invalid_argument("Invalid unsigned integer");
    return value;
}

// Deterministic number formatting with fixed precision, classic locale.
std::string fmtNum(double value, int precision = 2) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string fmtU(std::uint64_t value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << value;
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

struct SvgOptions {
    std::uint64_t seed{};
    std::filesystem::path output;
    std::filesystem::path load;
    bool noBuildings = false;
    bool noParcels = false;
    bool noTerritories = false;
    bool noNpcs = false;
    bool labels = false;
    bool diagnostics = false;
    bool runtime = false;
    std::int32_t width = 1200;
};

struct WorldView {
    const GeneratedWorld* base = nullptr;
    GeneratedWorld ownedWorld;
    std::optional<GameState> loadedState;
    const GameState* gameState = nullptr;
    bool hasGameState = false;
    std::string mode{"Generated world"};
    std::optional<std::uint32_t> saveFormatVersion;
    WorldPosition playerPos{};
    bool playerExists = false;
    float playerYaw = 0;
    float cellSize = 128.0F;
    struct NpcMarker { NpcId id; WorldPosition position; world::NpcLifecycle lifecycle{}; FactionId faction; EntityId entity; };
    std::vector<NpcMarker> activeNpcs;
    std::vector<NpcMarker> semanticMajorNpcs;
};

struct SvgLayerCounts {
    std::uint64_t roadNodes = 0;
    std::uint64_t roadEdges = 0;
    std::uint64_t districts = 0;
    std::uint64_t parcels = 0;
    std::uint64_t buildings = 0;
    std::uint64_t territories = 0;
    std::uint64_t factions = 0;
    std::uint64_t water = 0;
    std::uint64_t activeNpcs = 0;
    std::uint64_t semanticNpcs = 0;
    std::uint64_t player = 0;
};

const char* roadStrokes[4] = {"#444444", "#555555", "#666666", "#777777"};
const double roadWidths[4] = {1.2, 0.8, 0.4, 0.2};
const char* roadClasses[4] = {"road-arterial", "road-major", "road-local", "road-service"};

// Five stable deterministic faction colors (well-spaced hues).
// Debug UI concern only, NOT canonical game state.
struct FactionColor { const char* filled; const char* outline; };
inline constexpr std::array<FactionColor, 6> factionColors = {{
    {"#e41a1c", "#880000"}, // red
    {"#377eb8", "#003366"}, // blue
    {"#4daf4a", "#006600"}, // green
    {"#984ea3", "#4d0066"}, // purple
    {"#ff7f00", "#804000"}, // orange
    {"#999999", "#444444"}, // gray (unowned/contested)
}};

// Returns the fill color for a faction, using stable deterministic mapping.
// FactionId 0 means unowned/contested -> gray.
std::string factionFillColor(FactionId faction) {
    if (!faction.value()) return factionColors[5].filled;
    const std::size_t idx = static_cast<std::size_t>((faction.value() - 1) % 5);
    return factionColors[idx].filled;
}

// Convert SVG/CSS color "#RRGGBB" to standard "#RRGGBBAA".
std::string withAlpha(const std::string& hexColor, double alpha) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << hexColor;
    out << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
        << static_cast<int>(std::clamp(alpha,0.0,1.0) * 255.0 + 0.5);
    return out.str();
}

// SVG coordinate transform: world meters -> SVG pixels with scaling and centering.
// Maps world coordinates so the world bounding rectangle is centered within the
// SVG map viewport, with uniform aspect-ratio-preserving scale.
//
// General form — works for arbitrary world bounds (not just origin-centered):
//   svgX = mapLeft + (worldX - worldMinX) * scale
//   svgY = mapTop  + (worldZ - worldMinZ) * scale
// where mapLeft/mapTop center the rendered world within the available map area.
struct SvgTransform {
    double scale{};       // uniform scale factor (world meters -> SVG pixels)
    double worldMinX{};   // world X minimum (subtracted before scaling)
    double worldMinZ{};   // world Z minimum (subtracted before scaling)
    double mapLeft{};     // SVG x of the mapped world-min corner (includes centering + margin)
    double mapTop{};      // SVG y of the mapped world-min corner (includes centering + margin)

    std::pair<double, double> mapPoint(double worldX, double worldZ) const {
        return {mapLeft + (worldX - worldMinX) * scale,
                mapTop  + (worldZ - worldMinZ) * scale};
    }
    struct RectResult { double x, y, w, h; };
    RectResult mapRect(const world::Rect& r) const {
        const auto tl = mapPoint(static_cast<double>(r.x), static_cast<double>(r.z));
        const auto br = mapPoint(static_cast<double>(right(r)), static_cast<double>(bottom(r)));
        return {tl.first, tl.second, br.first - tl.first, br.second - tl.second};
    }
    std::pair<double, double> mapPosition(WorldPosition pos, float cellSize) const {
        const auto g = world::toGlobal(pos, cellSize);
        return {mapLeft + (g.x - worldMinX) * scale,
                mapTop  + (g.z - worldMinZ) * scale};
    }
};

// Compute the transform to fit a world of given dimensions into the SVG map area.
// Centers the world bounding rectangle within the available map area, with uniform
// aspect-ratio-preserving scale. Works for arbitrary world bounds.
SvgTransform computeTransform(double worldW, double worldD,
                              double worldMinX, double worldMinZ,
                              double margin, double mapAreaW, double mapAreaH) {
    const double scaleX = mapAreaW / worldW;
    const double scaleY = mapAreaH / worldD;
    const double scale = std::min(scaleX, scaleY);
    // Center the rendered world within the available map area (extra space on unused axis)
    const double renderedW = worldW * scale;
    const double renderedH = worldD * scale;
    const double mapLeft = margin + (mapAreaW - renderedW) / 2.0;
    const double mapTop = margin + (mapAreaH - renderedH) / 2.0;
    return SvgTransform{scale, worldMinX, worldMinZ, mapLeft, mapTop};
}

void writeRect(std::ostream& out, double x, double y, double w, double h,
               const std::string& fill, const std::string& stroke, double strokeWidth,
               const std::string& id = "", const std::string& extraClass = "") {
    out << "<rect x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" width=\"" << fmtNum(w) << "\" height=\"" << fmtNum(h) << "\"";
    if (!fill.empty()) out << " fill=\"" << fill << "\"";
    if (!stroke.empty()) out << " stroke=\"" << stroke << "\" stroke-width=\"" << fmtNum(strokeWidth) << "\"";
    if (!extraClass.empty()) out << " class=\"" << extraClass << "\"";
    if (!id.empty()) out << " id=\"" << id << "\"";
    out << "/>\n";
}

void writeRectRounded(std::ostream& out, double x, double y, double w, double h, double rx,
               const std::string& fill, const std::string& stroke, double strokeWidth,
               const std::string& id="", const std::string& css="") {
    out << "<rect x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" width=\"" << fmtNum(w) << "\" height=\"" << fmtNum(h)
        << "\" rx=\"" << fmtNum(rx) << "\" ry=\"" << fmtNum(rx) << "\"";
    if (!fill.empty()) out << " fill=\"" << fill << "\"";
    if (!stroke.empty()) out << " stroke=\"" << stroke << "\" stroke-width=\"" << fmtNum(strokeWidth) << "\"";
    if(!id.empty()) out<<" id=\""<<escapeXml(id)<<"\"";
    if(!css.empty()) out<<" class=\""<<escapeXml(css)<<"\"";
    out << "/>\n";
}

void writeLine(std::ostream& out, double x1, double y1, double x2, double y2,
               const std::string& stroke, double strokeWidth, const std::string& id = "") {
    out << "<line x1=\"" << fmtNum(x1) << "\" y1=\"" << fmtNum(y1) << "\" x2=\"" << fmtNum(x2) << "\" y2=\"" << fmtNum(y2) << "\"";
    if (!stroke.empty()) out << " stroke=\"" << stroke << "\" stroke-width=\"" << fmtNum(strokeWidth) << "\"";
    if (!id.empty()) out << " id=\"" << id << "\"";
    out << "/>\n";
}

void writeCircle(std::ostream& out, double cx, double cy, double r,
                 const std::string& fill, const std::string& stroke, double strokeWidth,
                 const std::string& id = "", const std::string& css = "", const std::string& title = "") {
    out << "<circle cx=\"" << fmtNum(cx) << "\" cy=\"" << fmtNum(cy) << "\" r=\"" << fmtNum(r) << "\"";
    if (!fill.empty()) out << " fill=\"" << fill << "\"";
    if (!stroke.empty()) out << " stroke=\"" << stroke << "\" stroke-width=\"" << fmtNum(strokeWidth) << "\"";
    if (!id.empty()) out << " id=\"" << escapeXml(id) << "\"";
    if (!css.empty()) out << " class=\"" << escapeXml(css) << "\"";
    if (title.empty()) out << "/>\n";
    else out << "><title>" << escapeXml(title) << "</title></circle>\n";
}

struct LabelBox { double x{}, y{}, w{}, h{}; };
struct LabelPlacer {
    double left{}, top{}, right{}, bottom{};
    std::vector<LabelBox> placed;
    bool intersects(const LabelBox& a) const {
        for (const auto& b : placed)
            if (a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y) return true;
        return false;
    }
    LabelBox place(double x,double y,std::string_view text,double size) {
        const double w=std::max(4.0,static_cast<double>(text.size())*size*0.62), h=size+3;
        const std::array<std::pair<double,double>,6> offsets{{{8,-h},{8,4},{-w-8,-h},{-w-8,4},{-w/2,-h-7},{-w/2,8}}};
        for (const auto [dx,dy] : offsets) {
            LabelBox b{std::clamp(x+dx,left,right-w),std::clamp(y+dy,top,bottom-h),w,h};
            if (!intersects(b)) { placed.push_back(b); return b; }
        }
        LabelBox b{std::clamp(x+offsets[0].first,left,right-w),std::clamp(y+offsets[0].second,top,bottom-h),w,h};
        placed.push_back(b); return b;
    }
};

void writeText(std::ostream& out, double x, double y, const std::string& text,
               const std::string& fill = "#000000", double size = 10.0, const std::string& id = "") {
    out << "<text x=\"" << fmtNum(x) << "\" y=\"" << fmtNum(y) << "\" font-family=\"monospace\" font-size=\""
        << fmtNum(size) << "\" fill=\"" << fill << "\"";
    if (!id.empty()) out << " id=\"" << id << "\"";
    out << ">" << escapeXml(text) << "</text>\n";
}

void placedText(std::ostream& out,LabelPlacer& labels,double x,double y,const std::string& text,
                const std::string& fill,double size,const std::string& id="") {
    const auto b=labels.place(x,y,text,size); writeText(out,b.x,b.y+b.h-3,text,fill,size,id);
}

void renderBackground(std::ostream& out, const SvgTransform& transform,
                      double mapW, double mapH) {
    out << "<g id=\"background\">\n";
    const auto bgOrigin = transform.mapPoint(transform.worldMinX, transform.worldMinZ);
    writeRect(out, bgOrigin.first, bgOrigin.second, mapW, mapH,
              "#f0f0f0", "#cccccc", 1, "", "background");
    out << "</g>\n";
}

void renderBounds(std::ostream& out, const SvgTransform& transform,
                  double worldMinX, double worldMinZ, double worldMaxX, double worldMaxZ) {
    out << "<g id=\"bounds\">\n";
    const auto tl = transform.mapPoint(worldMinX, worldMinZ);
    const auto br = transform.mapPoint(worldMaxX, worldMaxZ);
    writeRect(out, tl.first, tl.second, br.first - tl.first, br.second - tl.second,
              "", "#000000", 2, "world-bounds", "boundary");
    out << "</g>\n";
}

void renderWater(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                 SvgLayerCounts& counts) {
    out << "<g id=\"water\">\n";
    if (view.base) {
        for (const auto& wr : view.base->water) {
            const auto sr = transform.mapRect(wr.boundary);
            writeRect(out, sr.x, sr.y, sr.w, sr.h,
                "#a0c8f0", "#6090c0", 0.5, "water-" + fmtU(wr.id.value()), "water");
            ++counts.water;
        }
    }
    out << "</g>\n";
}

void renderTerritories(std::ostream& out, const WorldView& view, const SvgTransform& transform) {
    out << "<g id=\"territories\">\n";
    if (view.base) {
        std::map<TerritoryId, FactionId> territoryOwner;
        if (view.hasGameState) {
            for (const auto& t : view.base->territories) {
                TerritoryControl control = territoryControl(*view.gameState, t.id);
                territoryOwner[t.id] = control.controller;
            }
        }
        for (const auto& t : view.base->territories) {
            FactionId owner; TerritoryControlState state=TerritoryControlState::Uncontrolled;
            if (view.hasGameState) {
                const auto it = territoryOwner.find(t.id);
                if (it != territoryOwner.end()) owner = it->second;
                state=territoryControl(*view.gameState,t.id).state;
            } else {
                for (const auto& a : view.base->territoryAssignments)
                    if (a.territory == t.id) { owner = a.owner; state=owner ? TerritoryControlState::Controlled : TerritoryControlState::Uncontrolled; break; }
            }
            const std::string fill = state==TerritoryControlState::Contested ? "#55999999" :
                state==TerritoryControlState::Uncontrolled ? "#22999999" : withAlpha(factionFillColor(owner), 0.28);
            const auto sr = transform.mapRect(t.boundary);
            out<<"<rect x=\""<<fmtNum(sr.x)<<"\" y=\""<<fmtNum(sr.y)<<"\" width=\""<<fmtNum(sr.w)<<"\" height=\""<<fmtNum(sr.h)
               <<"\" fill=\""<<fill<<"\" stroke=\""<<(state==TerritoryControlState::Contested?"#222222":"#444444")
               <<"\" stroke-width=\""<<fmtNum(state==TerritoryControlState::Contested?0.8:0.3)<<"\" id=\"territory-"<<t.id.value()
               <<"\" data-controller=\""<<owner.value()<<"\" data-control=\""<<(state==TerritoryControlState::Controlled?"controlled":state==TerritoryControlState::Contested?"contested":"uncontrolled")
               <<"\" class=\"territory territory-"<<(state==TerritoryControlState::Controlled?"controlled":state==TerritoryControlState::Contested?"contested":"uncontrolled")<<"\"/>\n";
        }
    }
    out << "</g>\n";
}

void renderDistricts(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                     bool showLabels, LabelPlacer& labels) {
    out << "<g id=\"districts\">\n";
    if (view.base) {
        for (const auto& d : view.base->districts) {
            const auto sr = transform.mapRect(d.boundary);
            std::string id = "district-" + fmtU(d.id.value());
            writeRect(out, sr.x, sr.y, sr.w, sr.h, "", "#888888", 0.8, id, "district");
            if (showLabels) {
                const auto center = transform.mapPoint(
                    static_cast<double>(d.boundary.x) + d.boundary.width / 2.0,
                    static_cast<double>(d.boundary.z) + d.boundary.depth / 2.0);
                placedText(out,labels,center.first,center.second,"D"+fmtU(d.id.value()),"#444444",8,"district-label-"+fmtU(d.id.value()));
            }
        }
    }
    out << "</g>\n";
}

void renderRoads(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                 SvgLayerCounts& counts) {
    out << "<g id=\"roads\">\n";
    if (view.base) {
        std::map<RoadNodeId,const RoadNode*> nodes;
        for(const auto& node:view.base->roads.nodes) nodes.emplace(node.id,&node);
        for (const auto& edge : view.base->roads.edges) {
            core::require(nodes.contains(edge.a)&&nodes.contains(edge.b),"Road edge references missing node");
            const auto* na=nodes.at(edge.a); const auto* nb=nodes.at(edge.b);
            const auto [sx, sy] = transform.mapPoint(static_cast<double>(na->x), static_cast<double>(na->z));
            const auto [ex, ey] = transform.mapPoint(static_cast<double>(nb->x), static_cast<double>(nb->z));
            const std::size_t idx = static_cast<std::size_t>(edge.category); core::require(idx<4,"Invalid road category");
            out << "<line x1=\"" << fmtNum(sx) << "\" y1=\"" << fmtNum(sy)
                << "\" x2=\"" << fmtNum(ex) << "\" y2=\"" << fmtNum(ey) << "\"";
            out << " stroke=\"" << roadStrokes[idx] << "\" stroke-width=\"" << fmtNum(roadWidths[idx])
                << "\" id=\"road-edge-"<<edge.id.value()<<"\" class=\"" << roadClasses[idx] << "\"/>\n";
            ++counts.roadEdges;
        }
    }
    out << "</g>\n";
}

void renderParcels(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                   SvgLayerCounts& counts, bool noParcels) {
    out << "<g id=\"parcels\">\n";
    if (view.base && !noParcels) {
        for (const auto& p : view.base->parcels) {
            const auto sr = transform.mapRect(p.boundary);
            writeRect(out, sr.x, sr.y, sr.w, sr.h, "", "#dddddd", 0.2,
                "parcel-" + fmtU(p.id.value()), "parcel");
            ++counts.parcels;
        }
    }
    out << "</g>\n";
}

void renderBuildings(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                     SvgLayerCounts& counts, bool noBuildings) {
    out << "<g id=\"buildings\">\n";
    if (view.base && !noBuildings) {
        for (const auto& b : view.base->buildings) {
            const auto sr = transform.mapRect(b.footprint);
            writeRectRounded(out, sr.x, sr.y, sr.w, sr.h,
                std::max(1.0, std::min(sr.w, sr.h) * 0.1),
                "#333333", "#222222", 0.2,"building-"+fmtU(b.id.value()),"building");
            ++counts.buildings;
        }
    }
    out << "</g>\n";
}

void renderFactions(std::ostream& out, const WorldView&, const SvgTransform&, bool, LabelPlacer&) {
    out << "<g id=\"factions\">\n";
    out << "<!-- No canonical faction-HQ position exists; home territories are territory data, not HQs. -->\n";
    out << "</g>\n";
}

void renderNpcs(std::ostream& out, const WorldView& view, const SvgTransform& transform,
                SvgLayerCounts& counts, bool noNpcs, bool showLabels, LabelPlacer& labels) {
    out << "<g id=\"npcs\">\n";
    if (!noNpcs) {
        // Semantic major NPCs (hollow markers)
        for (const auto& npc : view.semanticMajorNpcs) {
            const auto [sx, sy] = transform.mapPosition(npc.position, view.cellSize);
            const bool dead=npc.lifecycle==world::NpcLifecycle::Dead;
            writeCircle(out, sx, sy, 4.0, dead?"#aaaaaa":"none", dead?"#555555":"#ff6600", 1.0,
                "semantic-npc-"+fmtU(npc.id.value()),dead?"semantic-npc dead":"semantic-npc",
                "NpcId "+fmtU(npc.id.value())+" faction "+fmtU(npc.faction.value())+(dead?" DEAD":" abstract/semantic"));
            if (showLabels) placedText(out,labels,sx,sy,"NPC#"+fmtU(npc.id.value())+(dead?" DEAD":""),dead?"#555555":"#663300",7,
                "semantic-npc-label-"+fmtU(npc.id.value()));
            ++counts.semanticNpcs;
        }
        // Active physical NPCs (solid markers)
        for (const auto& npc : view.activeNpcs) {
            const auto [sx, sy] = transform.mapPosition(npc.position, view.cellSize);
            writeCircle(out, sx, sy, 3.0, "#ff0000", "#880000", 0.5,
                "active-npc-"+fmtU(npc.id.value()),"active-npc","NpcId "+fmtU(npc.id.value())+" EntityId "+fmtU(npc.entity.value()));
            ++counts.activeNpcs;
        }
    }
    out << "</g>\n";
}

void renderPlayer(std::ostream& out, const WorldView& view, const SvgTransform& transform, bool showLabels, LabelPlacer& labels) {
    if (!view.playerExists) return;
    out << "<g id=\"player\">\n";
    const auto [sx, sy] = transform.mapPosition(view.playerPos, view.cellSize);
    const double size = 8.0;
    const double cx = sx;
    const double cy = sy;
    const auto yaw = view.playerYaw;
    const double dx = std::sin(yaw) * size;
    const double dy = std::cos(yaw) * size;
    const double px = cx + dx;
    const double py = cy + dy;
    const double leftX = cx + std::sin(yaw - 2.5) * size;
    const double leftY = cy + std::cos(yaw - 2.5) * size;
    const double rightX = cx + std::sin(yaw + 2.5) * size;
    const double rightY = cy + std::cos(yaw + 2.5) * size;
    const auto global=world::toGlobal(view.playerPos,view.cellSize);
    out << "<polygon id=\"player-marker\" data-entity=\""<<view.gameState->player.controlledEntity.value()<<"\" data-world-x=\""<<fmtNum(global.x)<<"\" data-world-z=\""<<fmtNum(global.z)<<"\" data-yaw=\""<<fmtNum(yaw,6)<<"\" data-cx=\""<<fmtNum(cx)<<"\" data-cy=\""<<fmtNum(cy)<<"\" points=\"" << fmtNum(px) << "," << fmtNum(py) << " "
        << fmtNum(leftX) << "," << fmtNum(leftY) << " " << fmtNum(rightX) << "," << fmtNum(rightY)
        << "\" fill=\"#0000ff\" stroke=\"#000088\" stroke-width=\"0.5\"/>\n";
    writeLine(out, cx, cy, px, py, "#0000ff", 1.5, "player-dir");
    if(showLabels) placedText(out,labels,cx,cy,"PLAYER","#0000ff",10,"player-label");
    out << "</g>\n";
}

void renderMetadata(std::ostream& out, const WorldView& view, const SvgOptions& opts,
                    double svgH, double margin) {
    (void)opts;
    out << "<g id=\"metadata\">\n";
    double boxX = margin;
    double boxY = svgH - 180;
    writeRect(out, boxX, boxY, 280, 170, "#ffffff", "#999999", 0.5, "metadata-box");
    writeText(out, boxX + 6, boxY + 14, "FAULTLINE 2D Debug Map (non-authoritative)", "#333333", 10);
    writeText(out, boxX + 6, boxY + 28, "Mode: " + view.mode, "#333333", 9);
    writeText(out, boxX + 6, boxY + 42, "Seed: " + fmtU(view.base->metadata.seed.value), "#333333", 9);
    if (view.hasGameState) {
        if(view.saveFormatVersion) writeText(out, boxX + 6, boxY + 56, "SaveFormatVersion: " + fmtU(*view.saveFormatVersion), "#333333", 9);
        writeText(out, boxX + 6, boxY + 70, "StateHash: " + fmtHex(stateHash(*view.gameState)), "#333333", 9);
        writeText(out, boxX + 6, boxY + 84, "Tick: " + fmtU(view.gameState->clock.tick()), "#333333", 9);
    } else if (view.base) {
        writeText(out, boxX + 6, boxY + 56, "GeneratorVersion: " + fmtU(view.base->metadata.generatorVersion.value), "#333333", 9);
    }
    if (view.base) {
        writeText(out, boxX + 6, boxY + 98, "BaseWorldHash: " + fmtHex(baseWorldHash(*view.base)), "#333333", 9);
        writeText(out, boxX + 6, boxY + 112, "Player: " + std::string(view.playerExists?"present":"absent"), "#333333", 9);
        writeText(out, boxX + 6, boxY + 126, "Major/active NPCs: "+fmtU(view.semanticMajorNpcs.size())+"/"+fmtU(view.activeNpcs.size()),"#333333",9);
    }
    out << "</g>\n";
}

void renderLegend(std::ostream& out, const WorldView& view, double legendY, double margin) {
    out << "<g id=\"legend\">\n";
    double lx = margin;
    double ly = legendY;
    writeRect(out, lx - 4, ly - 4, 380, 180, "#ffffff", "#999999", 0.5, "legend-box");
    writeText(out, lx, ly + 12, "Legend", "#333333", 11);
    double ly2 = ly + 28;
    writeRect(out, lx, ly2, 14, 14, "#a0c8f0", "#6090c0", 0.5);
    writeText(out, lx + 20, ly2 + 11, "Water", "#333333", 9);
    ly2 += 18;
    writeRect(out, lx, ly2, 14, 14, "", "#888888", 0.8);
    writeText(out, lx + 20, ly2 + 11, "District", "#333333", 9);
    ly2 += 18;
    writeLine(out, lx, ly2 + 7, lx + 14, ly2 + 7, "#444444", 1.2);
    writeText(out, lx + 20, ly2 + 11, "Roads", "#333333", 9);
    ly2 += 18;
    writeRectRounded(out, lx, ly2, 14, 10, 2, "#333333", "#222222", 0.2);
    writeText(out, lx + 20, ly2 + 8, "Buildings", "#333333", 9);
    ly2 += 18;
    writeRect(out, lx, ly2, 14, 10, withAlpha(std::string("#e41a1c"), 0.2), "#444444", 0.3);
    writeText(out, lx + 20, ly2 + 8, "Territory (gray=contested/unowned)", "#333333", 9);
    ly2 += 18;
    writeCircle(out, lx + 7, ly2 + 7, 4, "#0000ff", "#000088", 0.5);
    writeText(out, lx + 20, ly2 + 11, "Player (runtime/save only)", "#333333", 9);
    ly2 += 18;
    writeCircle(out, lx + 7, ly2 + 7, 3, "#ff0000", "#880000", 0.5);
    writeText(out, lx + 20, ly2 + 11, "Active NPC", "#333333", 9);
    ly2 += 18;
    writeCircle(out, lx + 7, ly2 + 7, 4, "none", "#ff6600", 1.0);
    writeText(out, lx + 20, ly2 + 11, "Major NPC (semantic)", "#333333", 9);
    out << "</g>\n";

    // Faction color legend
    out << "<g id=\"faction-legend\">\n";
    double fx = lx + 210;
    double fy = ly;
    writeText(out, fx, fy + 12, "Faction colors (no HQ data)", "#333333", 11);
    fy += 24;
    if (view.base) {
        for (const auto& f : view.base->factions) {
            const std::string color = factionFillColor(f.id);
            writeCircle(out, fx + 7, fy + 7, 5, color, "#000000", 0.5);
            writeText(out, fx + 18, fy + 11, "F" + fmtU(f.id.value()) + " " + f.state.name,
                color, 9, "faction-" + fmtU(f.id.value()));
            fy += 16;
        }
    }
    out << "</g>\n";
}

void collectWorldView(WorldView& view, bool fromSeed, SvgOptions& opts);

void renderSvg(std::ostream& out, const WorldView& view, const SvgOptions& opts,
               SvgLayerCounts counts, const SvgTransform& transform,
               double worldW, double worldD,
               double margin, double mapAreaH) {
    const double mapW = worldW * transform.scale;
    const double mapH = worldD * transform.scale;
    const double legendHeight = 190.0;
    const double svgW = static_cast<double>(opts.width);
    const double svgH = mapAreaH + 2 * margin + legendHeight + 10;
    const double legendY = mapAreaH + margin + 10;
    LabelPlacer labels{transform.mapLeft,transform.mapTop,transform.mapLeft+mapW,transform.mapTop+mapH,{}};

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
        << "width=\"" << fmtNum(svgW, 0) << "\" height=\"" << fmtNum(svgH, 0) << "\" "
        << "viewBox=\"0 0 " << fmtNum(svgW, 0) << " " << fmtNum(svgH, 0) << "\">\n";
    out << "<desc>FAULTLINE 2D Debug Map Viewer (non-authoritative). Coordinate mapping: world X->SVG x, world Z->SVG y (north-up).</desc>\n";
    out << "<style>\n";
    out << "  .water { fill:#a0c8f0; stroke:#6090c0; stroke-width:0.5; }\n";
    out << "  .district { fill:none; stroke:#888888; stroke-width:0.8; }\n";
    out << "  .road-arterial { stroke:#444444; stroke-width:1.2;}\n";
    out << "  .road-major { stroke:#555555; stroke-width:0.8; }\n";
    out << "  .road-local { stroke:#666666; stroke-width:0.4; }\n";
    out << "  .road-service { stroke:#777777; stroke-width:0.2; }\n";
    out << "  .parcel { fill:none; stroke:#cccccc; stroke-width:0.2; }\n";
    out << "  .building { fill:#333333; stroke:#222222; stroke-width:0.2; }\n";
    out << "  .boundary { fill:none; stroke:#000000; stroke-width:2; stroke-dasharray:4,2; }\n";
    out << "  .territory { stroke-width:0.5; }\n";
    out << "  text { font-family:monospace; font-size:10px; }\n";
    out << "</style>\n";

    // Layer 1: Background
    renderBackground(out, transform, mapW, mapH);

    // Layer 2: World bounds
    renderBounds(out, transform, static_cast<double>(view.base->bounds.x),
                 static_cast<double>(view.base->bounds.z),
                 static_cast<double>(right(view.base->bounds)),
                 static_cast<double>(bottom(view.base->bounds)));

    // Layer 3: Water
    renderWater(out, view, transform, counts);

    // Layer 4: Territories
    if (!opts.noTerritories)
        renderTerritories(out, view, transform);

    // Layer 5: Districts
    renderDistricts(out, view, transform, opts.labels, labels);

    // Layer 6: Roads
    renderRoads(out, view, transform, counts);

    // Layer 7: Parcels
    renderParcels(out, view, transform, counts, opts.noParcels);

    // Layer 8: Buildings
    renderBuildings(out, view, transform, counts, opts.noBuildings);

    // Layer 9: Factions (HQ markers)
    renderFactions(out, view, transform, opts.labels, labels);

    // Layer 10: NPCs (semantic major + active physical)
    renderNpcs(out, view, transform, counts, opts.noNpcs, opts.labels, labels);

    // Layer 11: Player
    renderPlayer(out, view, transform, opts.labels, labels);

    // Metadata box
    renderMetadata(out, view, opts, svgH, margin);

    // Legend
    renderLegend(out, view, legendY, margin);

    out << "</svg>\n";
}

void collectStateMarkers(WorldView& view) {
    for (const auto& [id,npc] : view.gameState->world.npcs.entries()) {
        if (npc.major) view.semanticMajorNpcs.push_back({id,npc.position,npc.lifecycle,npc.faction,npc.activeEntity});
        if (npc.activeEntity && view.gameState->world.entities.exists(npc.activeEntity) &&
            view.gameState->world.transforms.contains(npc.activeEntity))
            view.activeNpcs.push_back({id,view.gameState->world.transforms.get(npc.activeEntity).position,npc.lifecycle,npc.faction,npc.activeEntity});
    }
    if (view.gameState->player.controlledEntity &&
        view.gameState->world.entities.exists(view.gameState->player.controlledEntity) &&
        view.gameState->world.transforms.contains(view.gameState->player.controlledEntity)) {
        view.playerPos=view.gameState->world.transforms.get(view.gameState->player.controlledEntity).position;
        view.playerExists=true;
        view.playerYaw=view.gameState->world.characterBodies.get(view.gameState->player.controlledEntity).yaw;
    }
}

void collectWorldView(WorldView& view, bool fromSeed, SvgOptions& opts) {
    if (fromSeed && !opts.runtime) {
        const GenerationContext context{WorldSeed{opts.seed}, currentGeneratorVersion, {}};
        view.ownedWorld = generateWorld(context);
        view.base = &view.ownedWorld;
        view.cellSize = 128.0F;
        // Collect semantic NPC positions from generated world
        for (const auto& npc : view.base->majorNpcs)
            view.semanticMajorNpcs.push_back({npc.id,npc.state.position,npc.state.lifecycle,npc.state.faction,{}});
    } else if (fromSeed) {
        view.loadedState.emplace(WorldSeed{opts.seed});
        view.gameState=&*view.loadedState; view.hasGameState=true; view.mode="Runtime initial state";
        view.base=&*view.gameState->world.base; view.cellSize=view.gameState->world.spatial.cellSize();
        core::require(!view.gameState->world.npcs.entries().empty(),"Runtime world has no player spawn candidate");
        createPlayerEntity(*view.loadedState,view.gameState->world.npcs.entries().begin()->second.position);
        world::WorldStreamingSystem streaming;
        updatePlayerMovement(*view.loadedState,{},streaming); // canonical zero-input residency commit; no tick advances
        collectStateMarkers(view);
    } else {
        std::ifstream input(opts.load,std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot open input save: "+opts.load.string());
        const auto length=input.tellg(); if(length<0) throw std::runtime_error("Cannot size input save");
        persistence::SaveBytes bytes(static_cast<std::size_t>(length)); input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!input) throw std::runtime_error("Cannot read input save");
        view.saveFormatVersion=persistence::readSaveHeader(bytes).saveFormatVersion.value;
        view.loadedState.emplace(persistence::loadGameStateFile(opts.load));
        view.gameState = &*view.loadedState;
        view.hasGameState = true;
        view.mode="Loaded save";
        view.base = view.gameState->world.base.has_value() ? &*view.gameState->world.base : nullptr;
        view.cellSize = view.gameState->world.spatial.cellSize();
        const auto before=stateHash(*view.loadedState);
        restorePlayerResidency(*view.loadedState);
        core::require(stateHash(*view.loadedState)==before,"Residency reconstruction changed semantic StateHash");
        collectStateMarkers(view);
    }
}

void collectCounts(const WorldView& view, SvgLayerCounts& counts) {
    if (view.base) {
        counts.roadNodes = view.base->roads.nodes.size();
        counts.roadEdges = view.base->roads.edges.size();
        counts.districts = view.base->districts.size();
        counts.parcels = view.base->parcels.size();
        counts.buildings = view.base->buildings.size();
        counts.territories = view.base->territories.size();
        counts.factions = view.base->factions.size();
        counts.water = view.base->water.size();
    }
    counts.activeNpcs = view.activeNpcs.size();
    counts.semanticNpcs = view.semanticMajorNpcs.size();
    counts.player = view.playerExists ? 1 : 0;
}

// Diagnostics: track spatial bounds for worldgen data analysis.
struct SpatialBounds {
    std::int64_t minX{0}, maxX{0}, minZ{0}, maxZ{0};
    std::size_t count{0};
    bool empty() const { return count == 0; }
    std::int64_t width() const { return maxX - minX; }
    std::int64_t depth() const { return maxZ - minZ; }
};

struct SpatialCoverage {
    std::size_t count{0};
    SpatialBounds bounds;
    double coverageXPercent{0.0};
    double coverageZPercent{0.0};
};

void extendBounds(SpatialBounds& b, std::int64_t x, std::int64_t z) {
    if (b.count == 0) {
        b.minX = x; b.maxX = x; b.minZ = z; b.maxZ = z;
    } else {
        if (x < b.minX) b.minX = x;
        if (x > b.maxX) b.maxX = x;
        if (z < b.minZ) b.minZ = z;
        if (z > b.maxZ) b.maxZ = z;
    }
    ++b.count;
}
void extendBounds(SpatialBounds& b, const Rect& r) {
    if (!valid(r)) return;
    extendBounds(b, static_cast<std::int64_t>(r.x), static_cast<std::int64_t>(r.z));
    extendBounds(b, right(r), bottom(r));
}

void printBounds(std::ostream& out, const std::string& label, const SpatialCoverage& sc) {
    out << label << "\n";
    out << "count: " << sc.count << "\n";
    if (sc.bounds.empty()) {
        out << "bounds: NONE\n";
    } else {
        out << "minX: " << sc.bounds.minX << "\n";
        out << "maxX: " << sc.bounds.maxX << "\n";
        out << "minZ: " << sc.bounds.minZ << "\n";
        out << "maxZ: " << sc.bounds.maxZ << "\n";
        out << "width: " << sc.bounds.width() << "\n";
        out << "depth: " << sc.bounds.depth() << "\n";
        out << "coverageXPercent: " << fmtNum(sc.coverageXPercent, 2) << "\n";
        out << "coverageZPercent: " << fmtNum(sc.coverageZPercent, 2) << "\n";
    }
}

void printLayerBounds(std::ostream& out, const std::string& label,
                      const std::vector<Rect>& items,
                      std::int64_t worldMinX, std::int64_t worldMaxX,
                      std::int64_t worldMinZ, std::int64_t worldMaxZ) {
    SpatialBounds b;
    for (const auto& r : items) {
        extendBounds(b, r);
    }
    const std::int64_t worldW = worldMaxX - worldMinX;
    const std::int64_t worldD = worldMaxZ - worldMinZ;
    SpatialCoverage sc{items.size(), b};  // Use actual item count; bounds count is per-corner
    sc.coverageXPercent = (worldW > 0) ? (static_cast<double>(b.width()) * 100.0 /
                                           static_cast<double>(worldW)) : 0.0;
    sc.coverageZPercent = (worldD > 0) ? (static_cast<double>(b.depth()) * 100.0 /
                                           static_cast<double>(worldD)) : 0.0;
    printBounds(out, label, sc);
}

void printRoadNodeBounds(std::ostream& out,
                         const std::vector<RoadNode>& nodes,
                         const std::vector<RoadEdge>& edges,
                         std::int64_t worldMinX, std::int64_t worldMaxX,
                         std::int64_t worldMinZ, std::int64_t worldMaxZ) {
    SpatialBounds b;
    for (const auto& node : nodes) {
        extendBounds(b, static_cast<std::int64_t>(node.x), static_cast<std::int64_t>(node.z));
    }
    // Extend bounds with edge endpoints (already counted via nodes, but ensures
    // bounds include all edge endpoints even if some nodes aren't in the node list).
    for (const auto& edge : edges) {
        for (const auto& node : nodes) {
            if (node.id == edge.a || node.id == edge.b) {
                extendBounds(b, static_cast<std::int64_t>(node.x),
                             static_cast<std::int64_t>(node.z));
            }
        }
    }
    const std::int64_t worldW = worldMaxX - worldMinX;
    const std::int64_t worldD = worldMaxZ - worldMinZ;
    SpatialCoverage sc{nodes.size(), b};  // Use actual node count, not extend calls
    sc.coverageXPercent = (worldW > 0) ? (static_cast<double>(b.width()) * 100.0 /
                                           static_cast<double>(worldW)) : 0.0;
    sc.coverageZPercent = (worldD > 0) ? (static_cast<double>(b.depth()) * 100.0 /
                                           static_cast<double>(worldD)) : 0.0;
    printBounds(out, "ROAD NODES", sc);
}

void computeRoadBounds(const std::vector<RoadNode>& nodes,
                       const std::vector<RoadEdge>& edges,
                       SpatialBounds& out) {
    for (const auto& node : nodes) {
        extendBounds(out, static_cast<std::int64_t>(node.x), static_cast<std::int64_t>(node.z));
    }
    for (const auto& edge : edges) {
        for (const auto& node : nodes) {
            if (node.id == edge.a || node.id == edge.b) {
                extendBounds(out, static_cast<std::int64_t>(node.x),
                             static_cast<std::int64_t>(node.z));
            }
        }
    }
}

void printDiagnostics(const WorldView& view, const SvgOptions& opts,
                      double worldWMeters, double worldDMeters,
                      const SvgTransform& transform) {
    std::ostream& out = std::cout;

    const auto& base = *view.base;

    const std::int64_t worldMinX = static_cast<std::int64_t>(base.bounds.x);
    const std::int64_t worldMaxX = right(base.bounds);
    const std::int64_t worldMinZ = static_cast<std::int64_t>(base.bounds.z);
    const std::int64_t worldMaxZ = bottom(base.bounds);
    const std::int64_t worldW = worldMaxX - worldMinX;
    const std::int64_t worldD = worldMaxZ - worldMinZ;

    (void)worldWMeters;
    (void)worldDMeters;
    (void)transform;

    out << "=== MAP2D SPATIAL DIAGNOSTICS ===\n\n";

    // WORLD BOUNDS
    out << "WORLD BOUNDS\n";
    out << "minX: " << worldMinX << "\n";
    out << "maxX: " << worldMaxX << "\n";
    out << "minZ: " << worldMinZ << "\n";
    out << "maxZ: " << worldMaxZ << "\n";
    out << "width: " << worldW << "\n";
    out << "depth: " << worldD << "\n\n";

    // ROAD NODES
    printRoadNodeBounds(out, base.roads.nodes, base.roads.edges,
                        worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    out << "\n";

    // DISTRICTS
    {
        std::vector<Rect> rects;
        rects.reserve(base.districts.size());
        for (const auto& d : base.districts) rects.push_back(d.boundary);
        printLayerBounds(out, "DISTRICTS", rects, worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    }
    out << "\n";

    // PARCELS
    {
        std::vector<Rect> rects;
        rects.reserve(base.parcels.size());
        for (const auto& p : base.parcels) rects.push_back(p.boundary);
        printLayerBounds(out, "PARCELS", rects, worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    }
    out << "\n";

    // BUILDINGS
    {
        std::vector<Rect> rects;
        rects.reserve(base.buildings.size());
        for (const auto& b : base.buildings) rects.push_back(b.footprint);
        printLayerBounds(out, "BUILDINGS", rects, worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    }
    out << "\n";

    // TERRITORIES
    {
        std::vector<Rect> rects;
        rects.reserve(base.territories.size());
        for (const auto& t : base.territories) rects.push_back(t.boundary);
        printLayerBounds(out, "TERRITORIES", rects, worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    }
    out << "\n";

    // WATER
    {
        std::vector<Rect> rects;
        rects.reserve(base.water.size());
        for (const auto& w : base.water) rects.push_back(w.boundary);
        printLayerBounds(out, "WATER", rects, worldMinX, worldMaxX, worldMinZ, worldMaxZ);
    }
    out << "\n";

    // CITY / WORLD COVERAGE SUMMARY
    out << "CITY / WORLD COVERAGE SUMMARY\n";

    SpatialBounds roadBounds;
    computeRoadBounds(base.roads.nodes, base.roads.edges, roadBounds);
    double roadCovX = (worldW > 0) ? (static_cast<double>(roadBounds.width()) * 100.0 /
                                        static_cast<double>(worldW)) : 0.0;
    double roadCovZ = (worldD > 0) ? (static_cast<double>(roadBounds.depth()) * 100.0 /
                                        static_cast<double>(worldD)) : 0.0;
    out << "roadCoverageX: " << fmtNum(roadCovX, 2) << "\n";
    out << "roadCoverageZ: " << fmtNum(roadCovZ, 2) << "\n";

    SpatialBounds buildingBounds;
    for (const auto& b : base.buildings) {
        extendBounds(buildingBounds, b.footprint);
    }
    double buildingCovX = (worldW > 0) ? (static_cast<double>(buildingBounds.width()) *
                                           100.0 / static_cast<double>(worldW)) : 0.0;
    double buildingCovZ = (worldD > 0) ? (static_cast<double>(buildingBounds.depth()) *
                                           100.0 / static_cast<double>(worldD)) : 0.0;
    out << "buildingCoverageX: " << fmtNum(buildingCovX, 2) << "\n";
    out << "buildingCoverageZ: " << fmtNum(buildingCovZ, 2) << "\n";

    SpatialBounds districtBounds;
    for (const auto& d : base.districts) {
        extendBounds(districtBounds, d.boundary);
    }
    double districtCovX = (worldW > 0) ? (static_cast<double>(districtBounds.width()) *
                                            100.0 / static_cast<double>(worldW)) : 0.0;
    double districtCovZ = (worldD > 0) ? (static_cast<double>(districtBounds.depth()) *
                                            100.0 / static_cast<double>(worldD)) : 0.0;
    out << "districtCoverageX: " << fmtNum(districtCovX, 2) << "\n";
    out << "districtCoverageZ: " << fmtNum(districtCovZ, 2) << "\n\n";

    // VIEW TRANSFORM
    out << "VIEW TRANSFORM\n";
    out << "worldMinX: " << fmtNum(static_cast<double>(worldMinX), 2) << "\n";
    out << "worldMaxX: " << fmtNum(static_cast<double>(worldMaxX), 2) << "\n";
    out << "worldMinZ: " << fmtNum(static_cast<double>(worldMinZ), 2) << "\n";
    out << "worldMaxZ: " << fmtNum(static_cast<double>(worldMaxZ), 2) << "\n";
    out << "svgWidth: " << fmtNum(static_cast<double>(opts.width), 0) << "\n";

    const double margin = 8.0;
    const double mapAreaW = static_cast<double>(opts.width) - 2 * margin;
    const double mapAreaH = mapAreaW * 0.75;
    const double svgH = transform.scale * static_cast<double>(worldD) + 2 * margin + 190.0 + 10.0;

    out << "svgHeight: " << fmtNum(svgH, 0) << "\n";
    out << "scale: " << fmtNum(transform.scale, 6) << "\n";
    out << "mapLeft: " << fmtNum(transform.mapLeft, 2) << "\n";
    out << "mapTop: " << fmtNum(transform.mapTop, 2) << "\n";

    // === ACTUAL SVG GEOMETRY BOUNDS ===
    out << "\n=== ACTUAL SVG GEOMETRY BOUNDS ===\n\n";

    // Use the actual transform (with centering + world-min subtraction)
    auto svgX = [&transform](double worldX) {
        return transform.mapPoint(worldX, transform.worldMinZ).first;
    };
    auto svgY = [&transform](double worldZ) {
        return transform.mapPoint(transform.worldMinX, worldZ).second;
    };
    auto svgRect = [&transform](const Rect& r) -> std::tuple<double,double,double,double> {
        const auto tl = transform.mapPoint(static_cast<double>(r.x),
                                           static_cast<double>(r.z));
        const auto br = transform.mapPoint(static_cast<double>(right(r)),
                                           static_cast<double>(bottom(r)));
        return {tl.first, tl.second, br.first - tl.first, br.second - tl.second};
    };

    // Expected map area: world bounds centered in map area
    out << "EXPECTED MAP AREA (world bounds centered)\n";
    out << "mapLeft: " << fmtNum(margin) << "\n";
    out << "mapTop: " << fmtNum(margin) << "\n";
    out << "mapRight: " << fmtNum(mapAreaW + margin) << "\n";
    out << "mapBottom: " << fmtNum(mapAreaH + margin) << "\n";
    out << "mapWidth: " << fmtNum(mapAreaW) << "\n";
    out << "mapHeight: " << fmtNum(mapAreaH) << "\n\n";

    out << "WORLD TRANSFORMED CORNERS\n";
    out << "world(" << worldMinX << "," << worldMinZ << ") -> svg("
        << fmtNum(svgX(static_cast<double>(worldMinX))) << ","
        << fmtNum(svgY(static_cast<double>(worldMinZ))) << ")\n";
    out << "world(" << worldMaxX << "," << worldMinZ << ") -> svg("
        << fmtNum(svgX(static_cast<double>(worldMaxX))) << ","
        << fmtNum(svgY(static_cast<double>(worldMinZ))) << ")\n";
    out << "world(" << worldMinX << "," << worldMaxZ << ") -> svg("
        << fmtNum(svgX(static_cast<double>(worldMinX))) << ","
        << fmtNum(svgY(static_cast<double>(worldMaxZ))) << ")\n";
    out << "world(" << worldMaxX << "," << worldMaxZ << ") -> svg("
        << fmtNum(svgX(static_cast<double>(worldMaxX))) << ","
        << fmtNum(svgY(static_cast<double>(worldMaxZ))) << ")\n\n";

    // WATER SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        bool any = false;
        for (const auto& w : base.water) {
            auto [x, y, w2, h2] = svgRect(w.boundary);
            minX = std::min(minX, x); maxX = std::max(maxX, x + w2);
            minY = std::min(minY, y); maxY = std::max(maxY, y + h2);
            any = true;
        }
        out << "WATER\n";
        if (!any) { out << "bounds: NONE\n"; }
        else {
            out << "minSvgX: " << fmtNum(minX) << "\n";
            out << "maxSvgX: " << fmtNum(maxX) << "\n";
            out << "minSvgY: " << fmtNum(minY) << "\n";
            out << "maxSvgY: " << fmtNum(maxY) << "\n";
            out << "width: " << fmtNum(maxX - minX) << "\n";
            out << "height: " << fmtNum(maxY - minY) << "\n";
        }
    }
    out << "\n";

    // DISTRICTS SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        bool any = false;
        for (const auto& d : base.districts) {
            auto [x, y, w2, h2] = svgRect(d.boundary);
            minX = std::min(minX, x); maxX = std::max(maxX, x + w2);
            minY = std::min(minY, y); maxY = std::max(maxY, y + h2);
            any = true;
        }
        out << "DISTRICTS\n";
        if (!any) { out << "bounds: NONE\n"; }
        else {
            out << "minSvgX: " << fmtNum(minX) << "\n";
            out << "maxSvgX: " << fmtNum(maxX) << "\n";
            out << "minSvgY: " << fmtNum(minY) << "\n";
            out << "maxSvgY: " << fmtNum(maxY) << "\n";
            out << "width: " << fmtNum(maxX - minX) << "\n";
            out << "height: " << fmtNum(maxY - minY) << "\n";
        }
    }
    out << "\n";

    // PARCELS SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        bool any = false;
        for (const auto& p : base.parcels) {
            auto [x, y, w2, h2] = svgRect(p.boundary);
            minX = std::min(minX, x); maxX = std::max(maxX, x + w2);
            minY = std::min(minY, y); maxY = std::max(maxY, y + h2);
            any = true;
        }
        out << "PARCELS\n";
        if (!any) { out << "bounds: NONE\n"; }
        else {
            out << "minSvgX: " << fmtNum(minX) << "\n";
            out << "maxSvgX: " << fmtNum(maxX) << "\n";
            out << "minSvgY: " << fmtNum(minY) << "\n";
            out << "maxSvgY: " << fmtNum(maxY) << "\n";
            out << "width: " << fmtNum(maxX - minX) << "\n";
            out << "height: " << fmtNum(maxY - minY) << "\n";
        }
    }
    out << "\n";

    // BUILDINGS SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        bool any = false;
        for (const auto& b : base.buildings) {
            auto [x, y, w2, h2] = svgRect(b.footprint);
            minX = std::min(minX, x); maxX = std::max(maxX, x + w2);
            minY = std::min(minY, y); maxY = std::max(maxY, y + h2);
            any = true;
        }
        out << "BUILDINGS\n";
        if (!any) { out << "bounds: NONE\n"; }
        else {
            out << "minSvgX: " << fmtNum(minX) << "\n";
            out << "maxSvgX: " << fmtNum(maxX) << "\n";
            out << "minSvgY: " << fmtNum(minY) << "\n";
            out << "maxSvgY: " << fmtNum(maxY) << "\n";
            out << "width: " << fmtNum(maxX - minX) << "\n";
            out << "height: " << fmtNum(maxY - minY) << "\n";
            out << "visibleInX: " << fmtNum((maxX > 0 ? maxX : 0) - (minX > 0 ? minX : 0)) << "\n";
        }
    }
    out << "\n";

    // TERRITORIES SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        bool any = false;
        for (const auto& t : base.territories) {
            auto [x, y, w2, h2] = svgRect(t.boundary);
            minX = std::min(minX, x); maxX = std::max(maxX, x + w2);
            minY = std::min(minY, y); maxY = std::max(maxY, y + h2);
            any = true;
        }
        out << "TERRITORIES\n";
        if (!any) { out << "bounds: NONE\n"; }
        else {
            out << "minSvgX: " << fmtNum(minX) << "\n";
            out << "maxSvgX: " << fmtNum(maxX) << "\n";
            out << "minSvgY: " << fmtNum(minY) << "\n";
            out << "maxSvgY: " << fmtNum(maxY) << "\n";
            out << "width: " << fmtNum(maxX - minX) << "\n";
            out << "height: " << fmtNum(maxY - minY) << "\n";
        }
    }
    out << "\n";

    // ROADS SVG bounds
    {
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
        for (const auto& node : base.roads.nodes) {
            double sx = svgX(static_cast<double>(node.x));
            double sy = svgY(static_cast<double>(node.z));
            minX = std::min(minX, sx); maxX = std::max(maxX, sx);
            minY = std::min(minY, sy); maxY = std::max(maxY, sy);
        }
        out << "ROADS\n";
        out << "minSvgX: " << fmtNum(minX) << "\n";
        out << "maxSvgX: " << fmtNum(maxX) << "\n";
        out << "minSvgY: " << fmtNum(minY) << "\n";
        out << "maxSvgY: " << fmtNum(maxY) << "\n";
        out << "width: " << fmtNum(maxX - minX) << "\n";
        out << "height: " << fmtNum(maxY - minY) << "\n\n";
    }

    // EXTREME OBJECT TRACE
    out << "=== EXTREME OBJECT TRACE ===\n\n";

    // Find extreme road node (max X)
    const RoadNode* maxXNode = nullptr;
    for (const auto& node : base.roads.nodes) {
        if (!maxXNode || node.x > maxXNode->x) maxXNode = &node;
    }
    if (maxXNode) {
        const auto [sx, sy] = transform.mapPoint(static_cast<double>(maxXNode->x),
                                                  static_cast<double>(maxXNode->z));
        out << "ROAD NODE id=" << maxXNode->id.value() << "\n";
        out << "world=(" << maxXNode->x << "," << maxXNode->z << ")\n";
        out << "expectedSvgX=" << fmtNum(sx, 2) << "\n";
        out << "emittedSvgX=" << fmtNum(sx, 2) << "\n";
        out << "emittedSvgY=" << fmtNum(sy, 2) << "\n";
        out << "Note: using transform with mapLeft=" << fmtNum(transform.mapLeft, 2)
            << ", mapTop=" << fmtNum(transform.mapTop, 2) << "\n";
    }
    out << "\n";

    // Find extreme building (max X)
    const BuildingState* maxXBuilding = nullptr;
    for (const auto& b : base.buildings) {
        if (!maxXBuilding || b.footprint.x > maxXBuilding->footprint.x) maxXBuilding = &b;
    }
    if (maxXBuilding) {
        const auto [x, y, w2, h2] = transform.mapRect(maxXBuilding->footprint);
        out << "BUILDING id=" << maxXBuilding->id.value() << " (max X)\n";
        out << "sourceRect=(" << maxXBuilding->footprint.x << ","
            << maxXBuilding->footprint.z << " w=" << maxXBuilding->footprint.width
            << " d=" << maxXBuilding->footprint.depth << ")\n";
        out << "emittedSvgRect=(" << fmtNum(x, 2) << "," << fmtNum(y, 2)
            << " w=" << fmtNum(w2, 2) << " h=" << fmtNum(h2, 2) << ")\n";
    }
    out << "\n";

    // Find extreme building (min X)
    const BuildingState* minXBuilding = nullptr;
    for (const auto& b : base.buildings) {
        if (!minXBuilding || b.footprint.x < minXBuilding->footprint.x) minXBuilding = &b;
    }
    if (minXBuilding) {
        const auto [x, y, w2, h2] = transform.mapRect(minXBuilding->footprint);
        out << "BUILDING id=" << minXBuilding->id.value() << " (min X)\n";
        out << "sourceRect=(" << minXBuilding->footprint.x << ","
            << minXBuilding->footprint.z << " w=" << minXBuilding->footprint.width << ")\n";
        out << "emittedSvgRect=(" << fmtNum(x, 2) << "," << fmtNum(y, 2)
            << " w=" << fmtNum(w2, 2) << " h=" << fmtNum(h2, 2) << ")\n";
    }
    out << "\n";

    // SCALE RATIO ANALYSIS
    out << "=== SCALE RATIO ANALYSIS ===\n\n";
    out << "worldWidth=" << worldW << " (meters)\n";
    out << "svgCanvasWidth=" << fmtNum(static_cast<double>(opts.width), 0) << "\n";
    out << "mapAreaWidth=" << fmtNum(mapAreaW, 2) << "\n";
    out << "mapAreaHeight=" << fmtNum(mapAreaH, 2) << "\n";
    out << "effectiveScale=" << fmtNum(transform.scale, 6) << "\n";
    out << "worldInSvgWidth=" << fmtNum(static_cast<double>(worldW) * transform.scale, 2) << "\n";
    out << "worldInSvgHeight=" << fmtNum(static_cast<double>(worldD) * transform.scale, 2) << "\n";
    out << "mapAreaStartX=" << fmtNum(margin, 2) << " to " << fmtNum(margin + mapAreaW, 2) << "\n";
    out << "mapAreaStartY=" << fmtNum(margin, 2) << " to " << fmtNum(margin + mapAreaH, 2) << "\n";
    out << "actualWorldStartX=" << fmtNum(svgX(static_cast<double>(worldMinX)), 2) << "\n";
    out << "actualWorldEndX=" << fmtNum(svgX(static_cast<double>(worldMaxX)), 2) << "\n";
    out << "actualWorldStartY=" << fmtNum(svgY(static_cast<double>(worldMinZ)), 2) << "\n";
    out << "actualWorldEndY=" << fmtNum(svgY(static_cast<double>(worldMaxZ)), 2) << "\n";
    out << "\n";
    out << "Transform uses mapLeft=" << fmtNum(transform.mapLeft, 2)
        << " to center world within map viewport.\n";
    out << "World content now on-canvas: " << (svgX(static_cast<double>(worldMinX)) >= 0.0 ? "YES" : "NO") << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        SvgOptions opts;
        bool seedSet = false;
        bool loadSet = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            const auto value = [&]() -> std::string_view {
                if (++i >= argc) throw std::invalid_argument("Missing option value");
                return argv[i];
            };
            if (arg == "--seed") { opts.seed = toNumber(value()); seedSet = true; }
            else if (arg == "--output") opts.output = value();
            else if (arg == "--load") { opts.load = value(); loadSet = true; }
            else if (arg == "--no-buildings") opts.noBuildings = true;
            else if (arg == "--no-parcels") opts.noParcels = true;
            else if (arg == "--no-territories") opts.noTerritories = true;
            else if (arg == "--no-npcs") opts.noNpcs = true;
             else if (arg == "--labels") opts.labels = true;
             else if (arg == "--width") opts.width = static_cast<std::int32_t>(toNumber(value()));
             else if (arg == "--diagnostics") opts.diagnostics = true;
             else if (arg == "--runtime") opts.runtime = true;
             else if (arg == "--help") {
                std::cout << "faultline_map2d --seed N --output PATH [--no-buildings] [--no-parcels]\n"
                    "                [--no-territories] [--no-npcs] [--labels] [--width PX]\n"
                    "                [--runtime] [--diagnostics]\n"
                    "faultline_map2d --load PATH --output PATH [--no-buildings] [--no-parcels]\n"
                    "                [--no-territories] [--no-npcs] [--labels] [--width PX]\n"
                    "                [--diagnostics]\n";
                return 0;
            }
            else throw std::invalid_argument("Unknown option: " + std::string{arg});
        }
        if (seedSet && loadSet)
            throw std::invalid_argument("Cannot use both --seed and --load; choose one mode");
        if (opts.runtime && !seedSet) throw std::invalid_argument("--runtime requires --seed");
        if (opts.output.empty())
            throw std::invalid_argument("Missing --output PATH");
        if (!seedSet && !loadSet) throw std::invalid_argument("Specify exactly one of --seed N or --load PATH");
        if (opts.width < 320 || opts.width > 10000) throw std::invalid_argument("--width must be in 320..10000");

        WorldView view;
        SvgLayerCounts counts;

        collectWorldView(view, seedSet, opts);
        collectCounts(view, counts);

        if (!view.base)
            throw std::runtime_error("No base world available for rendering");

        // Derive bounds from actual generated world.
        double worldW = static_cast<double>(view.base->bounds.width);
        double worldD = static_cast<double>(view.base->bounds.depth);
        double worldMinX = static_cast<double>(view.base->bounds.x);
        double worldMinZ = static_cast<double>(view.base->bounds.z);

        // Compute deterministic scale to fit within width, preserving aspect ratio.
        // The world is centered within the map viewport, accounting for negative coords.
        const double margin = 8.0;
        const std::int32_t svgWidth = opts.width;
        const double mapAreaW = static_cast<double>(svgWidth) - 2 * margin;
        const double mapAreaH = mapAreaW * 0.75;
        const SvgTransform transform = computeTransform(worldW, worldD,
                                                         worldMinX, worldMinZ,
                                                         margin, mapAreaW, mapAreaH);

        // Open output file
        std::ofstream out(opts.output, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output file: " + opts.output.string());
        out.imbue(std::locale::classic());

        renderSvg(out, view, opts, counts, transform, worldW, worldD,
                  margin, mapAreaH);

         out.close();

         // Diagnostics: print spatial bounds report (console only, does not affect SVG)
         if (opts.diagnostics) {
             printDiagnostics(view, opts, worldW, worldD, transform);
         }

         // Report to stdout
        std::cout << "FAULTLINE 2D DEBUG MAP VIEWER\n";
        std::cout << "Mode: " << view.mode << "\n";
        std::cout << "Output: " << opts.output << "\n";
        std::cout << "Road nodes: " << counts.roadNodes << "\n";
        std::cout << "Road edges: " << counts.roadEdges << "\n";
        std::cout << "Districts: " << counts.districts << "\n";
        std::cout << "Parcels: " << counts.parcels << "\n";
        std::cout << "Buildings: " << counts.buildings << "\n";
        std::cout << "Territories: " << counts.territories << "\n";
        std::cout << "Factions: " << counts.factions << "\n";
        std::cout << "Major NPCs (semantic): " << counts.semanticNpcs << "\n";
        std::cout << "Active NPCs: " << counts.activeNpcs << "\n";
        std::cout << "Player: " << counts.player;
        if(!counts.player && view.mode=="Generated world") std::cout << " (absent: raw generated-world mode)";
        std::cout << "\n";
        if(view.playerExists){const auto g=world::toGlobal(view.playerPos,view.cellSize);const auto p=transform.mapPosition(view.playerPos,view.cellSize);
            std::cout<<"Player world: "<<fmtNum(g.x)<<","<<fmtNum(g.z)<<" svg: "<<fmtNum(p.first)<<","<<fmtNum(p.second)<<" yaw: "<<fmtNum(view.playerYaw,6)<<"\n";}
        if(!counts.activeNpcs && view.mode=="Generated world") std::cout << "Active physical NPCs are absent in raw generated-world mode\n";
        if (view.hasGameState) {
            std::cout << "StateHash: " << std::hex << stateHash(*view.gameState) << std::dec << "\n";
        }
        std::cout << "BaseWorldHash: " << std::hex << baseWorldHash(*view.base) << std::dec << "\n";
        std::cout << "Validation: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "faultline_map2d: " << error.what() << '\n';
        return 1;
    }
}
