#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "data/UnitMods.h"
#include "ui/App.h"

namespace gc {
namespace ui {

// ---------------------------------------------------------------------------
// The unit designer. Everything about a unit lives here: what it is, what it
// costs to buy and to keep, what it can do in a fight, what it looks like on
// the star map and on the battlefield, and every weapon mount bolted to its
// hull. Edits apply to the running game immediately and are written to
// unitmods.txt, which is folded back over the built-in roster at start-up.
// ---------------------------------------------------------------------------
namespace {

const Color kPanelBg{14, 19, 28, 246};
const Color kPanelEdge{58, 88, 122};
const Color kSectionBg{20, 28, 40, 235};

const char* kFactionNames[] = {"Neutral", "Republic", "CIS", "Hutts"};
const char* kClassNames[] = {"Corvette", "Frigate",  "Cruiser",   "Capital", "Fighter",
                             "Bomber",   "Infantry", "Vehicle",   "Artillery", "Air Support"};
const char* kShapeNames[] = {"Wedge", "Dagger", "Hammerhead", "Sphere", "Ring",
                             "Block", "Arrow",  "Walker",     "Tank",   "Trooper"};
const char* kHardpointNames[] = {"Turbolaser", "Ion Cannon",       "Missile Launcher", "Laser Cannon",
                                 "Point Def.", "Shield Generator", "Engine",           "Hangar Bay"};
const char* kYesNo[] = {"No", "Yes"};

std::string credits(int value) {
    std::string s = std::to_string(std::abs(value));
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ".");
    return (value < 0 ? "-" : "") + s;
}

}  // namespace

// Shared with the rest of the UI.
void drawUnitGlyphShared(Gfx& g, const Rect& r, UnitClass c, Color col);

/// Half-length and half-beam a hull is drawn at inside `area`. Long hulls are
/// sized by the width, fat ones by the height, so nothing ever spills out of
/// its frame. The designer's drag handling uses the same numbers.
void hullMetrics(const Rect& area, const UnitAppearance& a, float& len, float& beam) {
    len = area.w * 0.46f * std::max(0.15f, a.length);
    beam = len * std::max(0.12f, a.beam);
    const float maxBeam = area.h * 0.40f;
    if (beam > maxBeam) {
        len *= maxBeam / beam;
        beam = maxBeam;
    }
}

// ---------------------------------------------------------------------------
// Hull painting. Everything is derived from the unit's appearance block, so
// changing it in the designer changes the ship everywhere in the game.
// ---------------------------------------------------------------------------
void drawHull(Gfx& g, const Rect& area, const UnitDef& u, Color factionColour, bool showHardpoints,
              int selectedHardpoint, float uiScale) {
    const UnitAppearance& a = u.look;
    Color primary = a.useFactionColour ? factionColour
                                       : Color(a.primary[0], a.primary[1], a.primary[2]);
    Color secondary = a.useFactionColour
                          ? factionColour.scaled(0.6f)
                          : Color(a.secondary[0], a.secondary[1], a.secondary[2]);
    Color accent = Color(a.accent[0], a.accent[1], a.accent[2]);

    float cx = area.x + area.w * 0.5f;
    float cy = area.y + area.h * 0.5f;
    float len = 0.0f;
    float beam = 0.0f;
    hullMetrics(area, a, len, beam);

    switch (a.shape) {
        case HullShape::Wedge:
            g.triangle(Vec2(cx + len, cy), Vec2(cx - len, cy - beam), Vec2(cx - len, cy + beam), primary);
            g.triangle(Vec2(cx + len * 0.55f, cy), Vec2(cx - len * 0.7f, cy - beam * 0.45f),
                       Vec2(cx - len * 0.7f, cy + beam * 0.45f), secondary);
            // Command tower aft, the way a Star Destroyer wears it.
            g.rect(Rect{cx - len * 0.86f, cy - beam * 0.30f, len * 0.34f, beam * 0.60f}, secondary);
            g.rect(Rect{cx - len * 0.78f, cy - beam * 0.15f, len * 0.16f, beam * 0.30f}, accent);
            break;
        case HullShape::Dagger:
            g.triangle(Vec2(cx + len, cy), Vec2(cx - len * 0.2f, cy - beam * 0.75f),
                       Vec2(cx - len * 0.2f, cy + beam * 0.75f), primary);
            g.rect(Rect{cx - len, cy - beam * 0.55f, len * 0.85f, beam * 1.1f}, secondary);
            break;
        case HullShape::Hammerhead:
            g.rect(Rect{cx + len * 0.55f, cy - beam, len * 0.45f, beam * 2.0f}, primary);
            g.rect(Rect{cx - len, cy - beam * 0.45f, len * 1.55f, beam * 0.9f}, secondary);
            break;
        case HullShape::Sphere:
            g.circle(cx, cy, beam * 1.4f, primary);
            g.circle(cx - beam * 0.4f, cy - beam * 0.4f, beam * 0.7f, secondary);
            break;
        case HullShape::Ring:
            g.circleOutline(cx, cy, len * 0.85f, primary);
            for (int i = 0; i < 5; ++i) {
                g.circleOutline(cx, cy, len * 0.85f - static_cast<float>(i) * uiScale, primary);
            }
            g.circle(cx, cy, beam * 0.5f, secondary);
            break;
        case HullShape::Block:
            g.rect(Rect{cx - len, cy - beam, len * 2.0f, beam * 2.0f}, primary);
            g.rect(Rect{cx - len * 0.6f, cy - beam * 0.5f, len * 1.2f, beam}, secondary);
            break;
        case HullShape::Arrow:
            for (int i = 0; i < 3; ++i) {
                float ox = static_cast<float>(i - 1) * len * 0.55f;
                float oy = (i == 1 ? -beam * 0.5f : beam * 0.3f);
                g.triangle(Vec2(cx + ox + len * 0.34f, cy + oy), Vec2(cx + ox - len * 0.2f, cy + oy - beam * 0.4f),
                           Vec2(cx + ox - len * 0.2f, cy + oy + beam * 0.4f), i == 1 ? primary : secondary);
            }
            break;
        case HullShape::Walker: {
            g.rect(Rect{cx - len * 0.5f, cy - beam * 0.55f, len, beam * 0.8f}, primary);
            g.rect(Rect{cx + len * 0.25f, cy - beam * 0.95f, len * 0.45f, beam * 0.6f}, secondary);
            for (int i = 0; i < 3; ++i) {
                float lx = cx - len * 0.4f + static_cast<float>(i) * len * 0.4f;
                g.thickLine(lx, cy + beam * 0.2f, lx - len * 0.12f, cy + beam * 1.1f,
                            std::max(1.0f, 2.0f * uiScale), secondary);
            }
            break;
        }
        case HullShape::Tank:
            g.rect(Rect{cx - len * 0.75f, cy - beam * 0.35f, len * 1.5f, beam * 0.8f}, primary);
            g.rect(Rect{cx - len * 0.3f, cy - beam * 0.8f, len * 0.7f, beam * 0.5f}, secondary);
            g.rect(Rect{cx + len * 0.1f, cy - beam * 0.68f, len * 0.85f, beam * 0.18f}, accent);
            for (int i = 0; i < 4; ++i) {
                g.circle(cx - len * 0.55f + static_cast<float>(i) * len * 0.36f, cy + beam * 0.42f,
                         beam * 0.22f, secondary);
            }
            break;
        case HullShape::Trooper:
            for (int i = 0; i < 3; ++i) {
                float ox = static_cast<float>(i - 1) * len * 0.5f;
                g.circle(cx + ox, cy - beam * 0.5f, beam * 0.22f, primary);
                g.rect(Rect{cx + ox - beam * 0.18f, cy - beam * 0.22f, beam * 0.36f, beam * 0.9f},
                       i == 1 ? primary : secondary);
            }
            break;
        default:
            g.circle(cx, cy, beam, primary);
            break;
    }

    // Engines glow at the stern.
    for (int i = 0; i < a.engines; ++i) {
        float spread = a.engines > 1 ? (static_cast<float>(i) / static_cast<float>(a.engines - 1) - 0.5f)
                                     : 0.0f;
        float ey = cy + spread * beam * 1.1f;
        g.circle(cx - len * 0.98f, ey, std::max(1.0f, beam * 0.11f), Color(120, 200, 255, 220));
    }

    if (!showHardpoints) return;
    for (size_t i = 0; i < u.hardpoints.size(); ++i) {
        const Hardpoint& h = u.hardpoints[i];
        float hx = cx + h.offsetX * len;
        float hy = cy + h.offsetY * beam;
        bool sel = static_cast<int>(i) == selectedHardpoint;
        Color c = accent;
        switch (h.type) {
            case HardpointType::Turbolaser: c = Color(255, 120, 90); break;
            case HardpointType::IonCannon: c = Color(120, 190, 255); break;
            case HardpointType::Missile: c = Color(255, 190, 90); break;
            case HardpointType::LaserCannon: c = Color(255, 230, 120); break;
            case HardpointType::PointDefence: c = Color(160, 255, 160); break;
            case HardpointType::ShieldGenerator: c = Color(140, 160, 255); break;
            case HardpointType::Engine: c = Color(120, 220, 255); break;
            case HardpointType::Hangar: c = Color(200, 200, 210); break;
            default: break;
        }
        float r = std::max(2.0f, 4.0f * uiScale);
        g.circle(hx, hy, r, c);
        if (sel) g.circleOutline(hx, hy, r + 3.0f * uiScale, Color(255, 255, 255));
    }
}

void App::drawUnitIcon(const Rect& r, Id unitDefId, Faction owner, bool showHardpoints) {
    if (unitDefId == kInvalid) return;
    drawHull(gfx_, r, db().unit(unitDefId), pal::faction(owner), showHardpoints, -1, gfx_.uiScale());
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------
void App::openDesigner() {
    screen_ = Screen::Designer;
    SDL_StartTextInput();
    if (designUnit_ == kInvalid && !db().units().empty()) designUnit_ = 0;
    designFocus_ = -1;
}

void App::closeDesigner() {
    SDL_StopTextInput();
    designFocus_ = -1;
    screen_ = designReturn_;
}

void App::drawDesigner() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    gfx_.rect(Rect{0, 0, w, h}, Color(8, 11, 17));

    Database& edit = editableDb();
    if (designUnit_ < 0 || designUnit_ >= static_cast<Id>(edit.units().size())) designUnit_ = 0;
    UnitDef& u = edit.unitMutable(designUnit_);
    Color fc = pal::faction(u.faction);

    // Focus id counter: every field asks for the next one.
    int fieldId = 0;
    auto focused = [&](int id) { return designFocus_ == id; };
    auto claim = [&](const Rect& r, int id) {
        if (r.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
            input_.mouseClicked) {
            designFocus_ = id;
            designEdit_.clear();
        }
    };

    // ------------------------------------------------------------------
    // Header
    // ------------------------------------------------------------------
    Rect header{0, 0, w, S(52)};
    gfx_.panel(header, kPanelBg, kPanelEdge);
    gfx_.text(S(16), header.y + S(6), "UNIT DESIGNER", pal::kAccent, F(3));
    gfx_.text(S(16), header.y + S(30), "Edits apply to the running game and are saved to unitmods.txt",
              pal::kTextDim, F(1));

    ButtonStyle hs;
    hs.textScale = F(2);
    Rect saveBtn{w - S(430), S(10), S(130), S(32)};
    Rect reloadBtn{w - S(292), S(10), S(130), S(32)};
    Rect closeBtn{w - S(154), S(10), S(140), S(32)};
    if (button(gfx_, input_, saveBtn, "SAVE ALL", true, hs)) {
        int n = unitmods::save(edit);
        designMessage_ = n >= 0 ? std::to_string(n) + " unit(s) written to unitmods.txt"
                                : "Could not write unitmods.txt";
        setStatus(designMessage_);
    }
    if (button(gfx_, input_, reloadBtn, "RELOAD", true, hs)) {
        int n = unitmods::load(edit, unitmods::kDefaultPath);
        designMessage_ = n >= 0 ? std::to_string(n) + " unit(s) reloaded" : "No unitmods.txt to load";
    }
    if (button(gfx_, input_, closeBtn, "CLOSE", true, hs) || input_.keyPressed(SDLK_ESCAPE)) {
        closeDesigner();
        return;
    }
    if (!designMessage_.empty()) {
        gfx_.textRight(w - S(460), header.y + S(16), designMessage_, pal::kGood, F(1));
    }

    const float top = header.bottom() + S(8);
    const float bottom = h - S(8);

    // ------------------------------------------------------------------
    // Left: the roster
    // ------------------------------------------------------------------
    Rect list{S(8), top, S(300), bottom - top};
    gfx_.panel(list, kPanelBg, kPanelEdge);
    gfx_.text(list.x + S(10), list.y + S(8), "ROSTER", pal::kAccent, F(2));

    const char* filterNames[] = {"ALL", "REP", "CIS", "HUTT"};
    float fw = (list.w - S(20)) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        Rect r{list.x + S(10) + static_cast<float>(i) * fw, list.y + S(30), fw - S(2), S(24)};
        if (toggleButton(gfx_, input_, r, filterNames[i], designFilter_ == i)) designFilter_ = i;
    }

    std::vector<Id> shown;
    for (const UnitDef& d : edit.units()) {
        if (designFilter_ == 1 && d.faction != Faction::Republic) continue;
        if (designFilter_ == 2 && d.faction != Faction::CIS) continue;
        if (designFilter_ == 3 && d.faction != Faction::Hutts) continue;
        shown.push_back(d.id);
    }

    Rect rows{list.x + S(6), list.y + S(60), list.w - S(12), list.h - S(112)};
    gfx_.pushClip(rows);
    if (rows.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
        input_.wheel != 0) {
        designScroll_ = std::max(0.0f, designScroll_ - static_cast<float>(input_.wheel) * S(40));
    }
    float rowH = S(22);
    float y = rows.y - designScroll_;
    for (Id id : shown) {
        const UnitDef& d = edit.unit(id);
        Rect row{rows.x, y, rows.w, rowH - S(2)};
        y += rowH;
        if (row.bottom() < rows.y || row.y > rows.bottom()) continue;
        bool sel = id == designUnit_;
        bool hover = row.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
        gfx_.rect(row, sel ? Color(40, 62, 88, 240) : (hover ? Color(28, 40, 54, 230) : Color(18, 24, 34, 200)));
        gfx_.rect(Rect{row.x, row.y, S(4), row.h}, pal::faction(d.faction));
        gfx_.text(row.x + S(10), row.y + S(4), d.name.substr(0, 30), sel ? pal::kText : pal::kTextDim, F(1));
        if (d.custom) gfx_.textRight(row.right() - S(6), row.y + S(4), "*", pal::kWarning, F(1));
        if (hover && input_.mouseClicked) {
            designUnit_ = id;
            designHardpoint_ = -1;
            designFocus_ = -1;
        }
    }
    gfx_.popClip();

    ButtonStyle ls;
    ls.textScale = F(1);
    float bw = (list.w - S(24)) / 3.0f;
    Rect newBtn{list.x + S(10), list.bottom() - S(44), bw, S(28)};
    Rect cloneBtn{newBtn.right() + S(4), newBtn.y, bw, S(28)};
    Rect delBtn{cloneBtn.right() + S(4), newBtn.y, bw, S(28)};
    if (button(gfx_, input_, newBtn, "NEW", true, ls)) {
        UnitDef fresh;
        fresh.name = "New Unit";
        fresh.faction = u.faction;
        fresh.unitClass = u.unitClass;
        fresh.custom = true;
        designUnit_ = edit.createUnit("custom_" + std::to_string(edit.units().size()), fresh);
        designMessage_ = "Created a new unit";
    }
    if (button(gfx_, input_, cloneBtn, "CLONE", true, ls)) {
        UnitDef copy = u;
        copy.name = u.name + " II";
        copy.custom = true;
        designUnit_ = edit.createUnit(u.key + "_copy" + std::to_string(edit.units().size()), copy);
        designMessage_ = "Cloned " + u.name;
    }
    if (button(gfx_, input_, delBtn, "RESET", true, ls)) {
        // Custom units cannot be removed from a running campaign safely, so
        // "reset" clears the custom flag instead: the unit stops being saved.
        u.custom = false;
        designMessage_ = u.name + " will no longer be saved";
    }

    // ------------------------------------------------------------------
    // Centre: preview and hardpoints
    // ------------------------------------------------------------------
    // The property sheet is a fixed width; the preview takes whatever is left,
    // so on a wide monitor the hull you are editing gets the extra room.
    const float propsW = S(430);
    Rect centre{list.right() + S(8), top, w - S(16) - propsW - S(8) - (list.right() + S(8)),
                bottom - top};
    gfx_.panel(centre, kPanelBg, kPanelEdge);
    gfx_.text(centre.x + S(10), centre.y + S(8), "PREVIEW", pal::kAccent, F(2));
    gfx_.textRight(centre.right() - S(10), centre.y + S(10), u.key, pal::kTextDim, F(1));

    Rect hull{centre.x + S(10), centre.y + S(32), centre.w - S(20),
              std::max(S(200), (bottom - top) * 0.42f)};
    gfx_.rect(hull, Color(6, 9, 14, 240));
    gfx_.rectOutline(hull, kPanelEdge);
    drawHull(gfx_, hull, u, fc, true, designHardpoint_, gfx_.uiScale());
    gfx_.text(hull.x + S(6), hull.y + S(4), "BATTLE VIEW - drag a mount to move it", pal::kTextDim, F(1));

    // Dragging hardpoints around the hull.
    if (designHardpoint_ >= 0 && designHardpoint_ < static_cast<int>(u.hardpoints.size()) &&
        input_.mouseDown && hull.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY))) {
        float cx = hull.x + hull.w * 0.5f;
        float cy = hull.y + hull.h * 0.5f;
        float len = 0.0f;
        float beam = 0.0f;
        hullMetrics(hull, u.look, len, beam);
        Hardpoint& h = u.hardpoints[static_cast<size_t>(designHardpoint_)];
        h.offsetX = std::max(-1.4f, std::min(1.4f, (static_cast<float>(input_.mouseX) - cx) / len));
        h.offsetY = std::max(-1.6f, std::min(1.6f, (static_cast<float>(input_.mouseY) - cy) / beam));
        u.custom = true;
    }

    // Map icons at the sizes the star map uses.
    Rect icons{centre.x + S(10), hull.bottom() + S(8), centre.w - S(20), S(92)};
    gfx_.rect(icons, Color(6, 9, 14, 240));
    gfx_.rectOutline(icons, kPanelEdge);
    gfx_.text(icons.x + S(6), icons.y + S(4), "MAP ICON - ZOOMED OUT, NORMAL, ZOOMED IN", pal::kTextDim,
              F(1));
    // The three sizes the star map and the planet view actually draw at, each
    // in its own frame so the silhouette can be judged at a glance.
    float ix = icons.x + S(24);
    const float iconRow = icons.y + S(20);
    const float iconMax = icons.h - S(28);
    for (float scale : {0.55f, 1.0f, 1.7f}) {
        float side = iconMax * scale;
        Rect cell{ix, iconRow + (iconMax - side) * 0.5f, side, side};
        gfx_.rectOutline(cell, Color(30, 44, 64));
        drawHull(gfx_, cell, u, fc, false, -1, gfx_.uiScale());
        ix += side + S(28);
    }

    // Hardpoint list.
    Rect hpArea{centre.x + S(10), icons.bottom() + S(8), centre.w - S(20), centre.bottom() - icons.bottom() - S(18)};
    gfx_.rect(hpArea, kSectionBg);
    gfx_.rectOutline(hpArea, kPanelEdge);
    gfx_.text(hpArea.x + S(8), hpArea.y + S(6),
              "HARDPOINTS  (" + std::to_string(u.hardpoints.size()) + ")", pal::kAccent, F(2));
    Rect addBtn{hpArea.right() - S(150), hpArea.y + S(4), S(70), S(22)};
    Rect remBtn{hpArea.right() - S(76), hpArea.y + S(4), S(70), S(22)};
    if (button(gfx_, input_, addBtn, "ADD", true, ls)) {
        Hardpoint h;
        h.offsetX = 0.2f;
        h.offsetY = -0.4f;
        u.hardpoints.push_back(h);
        designHardpoint_ = static_cast<int>(u.hardpoints.size()) - 1;
        u.custom = true;
    }
    if (button(gfx_, input_, remBtn, "REMOVE",
               designHardpoint_ >= 0 && designHardpoint_ < static_cast<int>(u.hardpoints.size()), ls)) {
        u.hardpoints.erase(u.hardpoints.begin() + designHardpoint_);
        designHardpoint_ = -1;
        u.custom = true;
    }

    float hy = hpArea.y + S(32);
    for (size_t i = 0; i < u.hardpoints.size(); ++i) {
        Hardpoint& h = u.hardpoints[i];
        Rect row{hpArea.x + S(6), hy, hpArea.w - S(12), S(22)};
        hy += S(24);
        if (row.bottom() > hpArea.bottom() - S(4)) {
            gfx_.text(row.x, row.y, "...", pal::kTextDim, F(1));
            break;
        }
        bool sel = static_cast<int>(i) == designHardpoint_;
        bool hover = row.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
        gfx_.rect(row, sel ? Color(40, 62, 88, 240) : (hover ? Color(28, 40, 54, 220) : Color(16, 22, 32, 200)));
        gfx_.text(row.x + S(6), row.y + S(4), hardpointTypeName(h.type), pal::kText, F(1));
        gfx_.text(row.x + S(140), row.y + S(4),
                  "DMG " + std::to_string(static_cast<int>(h.damage)) + "   RNG " +
                      std::to_string(static_cast<int>(h.range)),
                  pal::kTextDim, F(1));
        gfx_.textRight(row.right() - S(6), row.y + S(4),
                       "HP " + std::to_string(static_cast<int>(h.health)), pal::kTextDim, F(1));
        if (hover && input_.mouseClicked) designHardpoint_ = static_cast<int>(i);
    }

    // Editing the selected hardpoint.
    if (designHardpoint_ >= 0 && designHardpoint_ < static_cast<int>(u.hardpoints.size())) {
        Hardpoint& h = u.hardpoints[static_cast<size_t>(designHardpoint_)];
        float ey = hpArea.bottom() - S(96);
        auto hpRow = [&](const char* label, float& value, float step, float lo, float hi) {
            gfx_.text(hpArea.x + S(8), ey + S(5), label, pal::kTextDim, F(1));
            Rect field{hpArea.x + S(120), ey, S(180), S(22)};
            int id = ++fieldId;
            claim(field, id);
            if (numberField(gfx_, input_, field, value, step, lo, hi, focused(id), designEdit_, F(1))) {
                u.custom = true;
            }
            ey += S(24);
        };
        int type = static_cast<int>(h.type);
        gfx_.text(hpArea.x + S(8), ey + S(5), "TYPE", pal::kTextDim, F(1));
        Rect typeField{hpArea.x + S(120), ey, S(180), S(22)};
        if (enumField(gfx_, input_, typeField, type, kHardpointNames,
                      static_cast<int>(HardpointType::Count), F(1))) {
            h.type = static_cast<HardpointType>(type);
            h.name = hardpointTypeName(h.type);
            u.custom = true;
        }
        ey += S(24);
        hpRow("DAMAGE", h.damage, 5.0f, 0.0f, 500.0f);
        hpRow("RANGE", h.range, 10.0f, 0.0f, 1000.0f);
        hpRow("MOUNT HP", h.health, 25.0f, 0.0f, 5000.0f);
    }

    // ------------------------------------------------------------------
    // Right: the property sheet
    // ------------------------------------------------------------------
    Rect props{centre.right() + S(8), top, w - centre.right() - S(16), bottom - top};
    gfx_.panel(props, kPanelBg, kPanelEdge);
    gfx_.pushClip(props.inset(2));
    if (props.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
        input_.wheel != 0 && designFocus_ < 0) {
        designPropScroll_ = std::max(0.0f, designPropScroll_ - static_cast<float>(input_.wheel) * S(40));
    }

    float py = props.y + S(8) - designPropScroll_;
    const float labelW = S(150);
    const float fieldW = std::min(S(260.0f), props.w - labelW - S(30));

    auto section = [&](const char* title) {
        Rect bar{props.x + S(6), py, props.w - S(12), S(24)};
        gfx_.rect(bar, kSectionBg);
        gfx_.text(bar.x + S(8), bar.y + S(6), title, pal::kAccent, F(2));
        py += S(30);
    };
    auto textRow = [&](const char* label, std::string& value) {
        gfx_.text(props.x + S(12), py + S(5), label, pal::kTextDim, F(1));
        Rect field{props.x + labelW, py, fieldW, S(22)};
        int id = ++fieldId;
        claim(field, id);
        if (textField(gfx_, input_, field, value, focused(id), F(1))) u.custom = true;
        py += S(26);
    };
    auto numRow = [&](const char* label, float& value, float step, float lo, float hi) {
        gfx_.text(props.x + S(12), py + S(5), label, pal::kTextDim, F(1));
        Rect field{props.x + labelW, py, fieldW, S(22)};
        int id = ++fieldId;
        claim(field, id);
        if (numberField(gfx_, input_, field, value, step, lo, hi, focused(id), designEdit_, F(1))) {
            u.custom = true;
        }
        py += S(26);
    };
    auto intRow = [&](const char* label, int& value, int step, int lo, int hi) {
        float v = static_cast<float>(value);
        numRow(label, v, static_cast<float>(step), static_cast<float>(lo), static_cast<float>(hi));
        value = static_cast<int>(std::lround(v));
    };
    auto enumRow = [&](const char* label, int& value, const char* const* names, int count) {
        gfx_.text(props.x + S(12), py + S(5), label, pal::kTextDim, F(1));
        Rect field{props.x + labelW, py, fieldW, S(22)};
        if (enumField(gfx_, input_, field, value, names, count, F(1))) u.custom = true;
        py += S(26);
    };

    section("IDENTITY");
    textRow("NAME", u.name);
    int factionIdx = static_cast<int>(u.faction);
    enumRow("FACTION", factionIdx, kFactionNames, kFactionCount);
    u.faction = factionFromIndex(factionIdx);
    int classIdx = static_cast<int>(u.unitClass);
    enumRow("CLASS", classIdx, kClassNames, static_cast<int>(UnitClass::Count));
    u.unitClass = static_cast<UnitClass>(classIdx);
    textRow("ROLE", u.role);
    textRow("MANUFACTURER", u.manufacturer);
    textRow("DESCRIPTION", u.description);

    section("PRODUCTION");
    intRow("COST (CR)", u.cost, 50, 0, 100000);
    intRow("UPKEEP / WEEK", u.upkeep, 5, 0, 5000);
    intRow("BUILD DAYS", u.buildDays, 1, 1, 60);
    intRow("UNIT SLOTS", u.popCost, 1, 1, 10);
    intRow("FACILITY TIER", u.requiredTier, 1, 1, 3);
    {
        // Technology requirement, cycled over this faction's tree.
        std::vector<Id> techs = db().factionTechs(u.faction);
        std::vector<std::string> names;
        names.push_back("None");
        for (Id t : techs) names.push_back(db().tech(t).name);
        std::vector<const char*> ptrs;
        for (const std::string& n : names) ptrs.push_back(n.c_str());
        int idx = 0;
        for (size_t i = 0; i < techs.size(); ++i) {
            if (techs[i] == u.requiredTech) idx = static_cast<int>(i) + 1;
        }
        gfx_.text(props.x + S(12), py + S(5), "REQUIRES TECH", pal::kTextDim, F(1));
        Rect field{props.x + labelW, py, fieldW, S(22)};
        if (enumField(gfx_, input_, field, idx, ptrs.data(), static_cast<int>(ptrs.size()), F(1))) {
            u.requiredTech = idx == 0 ? kInvalid : techs[static_cast<size_t>(idx - 1)];
            u.custom = true;
        }
        py += S(26);
    }

    section("COMBAT");
    numRow("HULL", u.hull, 50.0f, 1.0f, 20000.0f);
    numRow("SHIELD", u.shield, 25.0f, 0.0f, 20000.0f);
    numRow("SHIELD REGEN", u.shieldRegen, 0.5f, 0.0f, 200.0f);
    numRow("ANTI-CAPITAL", u.damageAntiCapital, 5.0f, 0.0f, 1000.0f);
    numRow("ANTI-SQUADRON", u.damageAntiFighter, 5.0f, 0.0f, 1000.0f);
    numRow("WEAPON RANGE", u.range, 10.0f, 10.0f, 2000.0f);
    numRow("SPEED", u.speed, 2.0f, 1.0f, 400.0f);
    numRow("ACCURACY", u.accuracy, 0.05f, 0.1f, 3.0f);
    {
        int heroIdx = u.isHero ? 1 : 0;
        enumRow("HERO UNIT", heroIdx, kYesNo, 2);
        u.isHero = heroIdx != 0;
        if (u.isHero) {
            numRow("HERO COMBAT BONUS", u.heroCombatBonus, 0.01f, 0.0f, 1.0f);
            intRow("HERO INCOME", u.heroIncomeBonus, 10, 0, 2000);
            intRow("RESPAWN DAYS", u.respawnDays, 5, 0, 200);
        }
    }

    section("APPEARANCE");
    int shapeIdx = static_cast<int>(u.look.shape);
    enumRow("HULL SHAPE", shapeIdx, kShapeNames, static_cast<int>(HullShape::Count));
    u.look.shape = static_cast<HullShape>(shapeIdx);
    numRow("LENGTH", u.look.length, 0.05f, 0.15f, 2.0f);
    numRow("BEAM", u.look.beam, 0.05f, 0.12f, 1.5f);
    intRow("ENGINES", u.look.engines, 1, 0, 8);
    {
        int useFaction = u.look.useFactionColour ? 1 : 0;
        enumRow("FACTION COLOUR", useFaction, kYesNo, 2);
        u.look.useFactionColour = useFaction != 0;
    }
    auto colourRow = [&](const char* label, int* rgb) {
        gfx_.text(props.x + S(12), py + S(5), label, pal::kTextDim, F(1));
        float cw = (fieldW - S(30)) / 3.0f;
        for (int i = 0; i < 3; ++i) {
            Rect field{props.x + labelW + static_cast<float>(i) * (cw + S(4)), py, cw, S(22)};
            float v = static_cast<float>(rgb[i]);
            int id = ++fieldId;
            claim(field, id);
            if (numberField(gfx_, input_, field, v, 10.0f, 0.0f, 255.0f, focused(id), designEdit_, F(1))) {
                u.custom = true;
            }
            rgb[i] = static_cast<int>(std::lround(v));
        }
        Rect swatch{props.x + labelW + fieldW - S(22), py, S(22), S(22)};
        gfx_.rect(swatch, Color(rgb[0], rgb[1], rgb[2]));
        gfx_.rectOutline(swatch, pal::kBorder);
        py += S(26);
    };
    colourRow("PRIMARY", u.look.primary);
    colourRow("SECONDARY", u.look.secondary);
    colourRow("ACCENT", u.look.accent);

    section("CARRIED SQUADRONS");
    for (size_t i = 0; i < u.wings.size(); ++i) {
        CarriedWing& wg = u.wings[i];
        gfx_.text(props.x + S(12), py + S(5),
                  (wg.unitId != kInvalid ? db().unit(wg.unitId).name : wg.unitKey).substr(0, 22),
                  pal::kText, F(1));
        Rect field{props.x + labelW, py, fieldW * 0.5f, S(22)};
        float count = static_cast<float>(wg.count);
        int id = ++fieldId;
        claim(field, id);
        if (numberField(gfx_, input_, field, count, 1.0f, 0.0f, 20.0f, focused(id), designEdit_, F(1))) {
            wg.count = static_cast<int>(std::lround(count));
            u.custom = true;
        }
        Rect del{field.right() + S(6), py, S(60), S(22)};
        if (button(gfx_, input_, del, "DROP", true, ls)) {
            u.wings.erase(u.wings.begin() + static_cast<long>(i));
            u.custom = true;
            break;
        }
        py += S(26);
    }
    {
        // Add a squadron: cycle through this faction's fighters and bombers.
        std::vector<Id> squadrons;
        for (const UnitDef& d : db().units()) {
            if (d.faction == u.faction && d.isSquadron()) squadrons.push_back(d.id);
        }
        if (!squadrons.empty()) {
            designWingPick_ = std::max(0, std::min(designWingPick_, static_cast<int>(squadrons.size()) - 1));
            std::vector<const char*> names;
            for (Id id : squadrons) names.push_back(db().unit(id).name.c_str());
            Rect field{props.x + labelW, py, fieldW, S(22)};
            gfx_.text(props.x + S(12), py + S(5), "ADD SQUADRON", pal::kTextDim, F(1));
            enumField(gfx_, input_, field, designWingPick_, names.data(),
                      static_cast<int>(names.size()), F(1));
            py += S(26);
            Rect addWing{props.x + labelW, py, S(120), S(22)};
            if (button(gfx_, input_, addWing, "ADD", true, ls)) {
                CarriedWing wg;
                wg.unitId = squadrons[static_cast<size_t>(designWingPick_)];
                wg.unitKey = db().unit(wg.unitId).key;
                wg.count = 1;
                u.wings.push_back(wg);
                u.custom = true;
            }
            py += S(30);
        }
    }

    section("DERIVED");
    gfx_.text(props.x + S(12), py, "TOTAL ANTI-CAPITAL", pal::kTextDim, F(1));
    gfx_.textRight(props.right() - S(12), py, std::to_string(static_cast<int>(u.antiCapital())),
                   pal::kText, F(1));
    py += S(18);
    gfx_.text(props.x + S(12), py, "TOTAL ANTI-SQUADRON", pal::kTextDim, F(1));
    gfx_.textRight(props.right() - S(12), py, std::to_string(static_cast<int>(u.antiFighter())),
                   pal::kText, F(1));
    py += S(18);
    gfx_.text(props.x + S(12), py, "TOTAL SHIELD", pal::kTextDim, F(1));
    gfx_.textRight(props.right() - S(12), py, std::to_string(static_cast<int>(u.totalShield())),
                   pal::kText, F(1));
    py += S(18);
    gfx_.text(props.x + S(12), py, "TOTAL SPEED", pal::kTextDim, F(1));
    gfx_.textRight(props.right() - S(12), py, std::to_string(static_cast<int>(u.totalSpeed())),
                   pal::kText, F(1));
    py += S(18);
    gfx_.text(props.x + S(12), py, "PRICE / UPKEEP", pal::kTextDim, F(1));
    gfx_.textRight(props.right() - S(12), py, credits(u.cost) + " / " + credits(u.upkeep),
                   pal::kWarning, F(1));
    py += S(26);

    gfx_.popClip();

    // Clicking outside every field drops focus.
    if (input_.mouseClicked && designFocus_ > fieldId) designFocus_ = -1;
}

}  // namespace ui
}  // namespace gc
