#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "ui/App.h"

namespace gc {
namespace ui {

// ---------------------------------------------------------------------------
// The galactic conquest screen is laid out like Empire at War's: the star map
// fills the whole window, the HUD is a command console welded across the
// bottom, the hero roster sits in the top right corner and a paused banner
// drops down from the top of the screen.
//
// Every length below is written against a 1600x900 reference window and passed
// through S() (and F() for font sizes), so the whole HUD keeps its proportions
// on any monitor, in a window or full screen.
// ---------------------------------------------------------------------------
namespace {

constexpr float kBarH = 208.0f;  ///< Reference height of the command console.
constexpr float kIconStripW = 40.0f;
constexpr float kMinimapW = 214.0f;

// Console styling: EaW's HUD is a dark metal frame lit by green readouts.
const Color kConsoleFill{14, 20, 20, 246};
const Color kConsoleEdge{58, 104, 74};
const Color kConsoleInner{10, 16, 16, 240};
const Color kReadout{126, 226, 132};
const Color kReadoutDim{72, 132, 84};

const char* kCategoryNames[] = {"FLEET", "ARMY",   "ORBIT", "BASE",
                                "TECH",  "HEROES", "WORLD", "NEWS"};
constexpr int kCategoryCount = 8;

enum Category {
    CatFleet = 0,  ///< Buildable space units.
    CatArmy,       ///< Buildable ground units.
    CatOrbit,      ///< Orbital structures.
    CatSurface,    ///< Surface structures.
    CatResearch,
    CatHeroes,
    CatWorld,   ///< Planet dossier and garrison.
    CatHolonet  ///< Event log.
};

std::string credits(int value) {
    std::string s = std::to_string(std::abs(value));
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ".");
    return (value < 0 ? "-" : "") + s;
}

std::string oneDecimal(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    return buf;
}

std::string classTag(UnitClass c) {
    switch (c) {
        case UnitClass::Capital: return "CAP";
        case UnitClass::Cruiser: return "CRU";
        case UnitClass::Frigate: return "FRG";
        case UnitClass::Corvette: return "COR";
        case UnitClass::Fighter: return "FTR";
        case UnitClass::Bomber: return "BMB";
        case UnitClass::Infantry: return "INF";
        case UnitClass::Vehicle: return "VEH";
        case UnitClass::Artillery: return "ART";
        case UnitClass::AirSupport: return "AIR";
        default: return "???";
    }
}

/// Two-letter badge for hero portraits.
std::string initials(const std::string& name) {
    std::string out;
    bool boundary = true;
    for (char c : name) {
        if (c == ' ' || c == '-' || c == '(') {
            boundary = true;
            continue;
        }
        if (boundary && out.size() < 2) out += static_cast<char>(std::toupper(c));
        boundary = false;
    }
    if (out.empty() && !name.empty()) out += static_cast<char>(std::toupper(name[0]));
    return out;
}

/// A little silhouette per unit class, drawn into the tray cards.
void drawUnitGlyph(Gfx& g, const Rect& r, UnitClass c, Color col) {
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float s = std::min(r.w, r.h) * 0.5f;
    switch (c) {
        case UnitClass::Capital:
        case UnitClass::Cruiser:
        case UnitClass::Frigate:
        case UnitClass::Corvette: {
            float len = s * (c == UnitClass::Capital   ? 1.0f
                             : c == UnitClass::Cruiser ? 0.85f
                             : c == UnitClass::Frigate ? 0.7f
                                                       : 0.55f);
            g.triangle(Vec2(cx + len, cy), Vec2(cx - len, cy - len * 0.55f),
                       Vec2(cx - len, cy + len * 0.55f), col);
            g.rect(Rect{cx - len, cy - len * 0.18f, len * 1.2f, len * 0.36f}, col.scaled(0.7f));
            break;
        }
        case UnitClass::Fighter:
        case UnitClass::Bomber: {
            for (int i = 0; i < 3; ++i) {
                float ox = static_cast<float>(i - 1) * s * 0.5f;
                float oy = (i == 1 ? -s * 0.25f : s * 0.15f);
                g.triangle(Vec2(cx + ox + s * 0.28f, cy + oy), Vec2(cx + ox - s * 0.18f, cy + oy - s * 0.2f),
                           Vec2(cx + ox - s * 0.18f, cy + oy + s * 0.2f),
                           c == UnitClass::Bomber ? col.scaled(0.8f) : col);
            }
            break;
        }
        case UnitClass::Infantry: {
            for (int i = 0; i < 3; ++i) {
                float ox = static_cast<float>(i - 1) * s * 0.45f;
                g.circle(cx + ox, cy - s * 0.35f, s * 0.16f, col);
                g.rect(Rect{cx + ox - s * 0.13f, cy - s * 0.15f, s * 0.26f, s * 0.6f}, col);
            }
            break;
        }
        case UnitClass::Vehicle:
        case UnitClass::Artillery: {
            g.rect(Rect{cx - s * 0.7f, cy - s * 0.25f, s * 1.4f, s * 0.5f}, col);
            g.rect(Rect{cx - s * 0.35f, cy - s * 0.6f, s * 0.7f, s * 0.4f}, col.scaled(0.75f));
            if (c == UnitClass::Artillery) {
                g.rect(Rect{cx - s * 0.1f, cy - s * 0.95f, s * 0.9f, s * 0.14f}, col);
            }
            for (int i = 0; i < 3; ++i) {
                g.circle(cx - s * 0.5f + static_cast<float>(i) * s * 0.5f, cy + s * 0.35f, s * 0.18f,
                         col.scaled(0.6f));
            }
            break;
        }
        case UnitClass::AirSupport: {
            g.triangle(Vec2(cx + s * 0.8f, cy), Vec2(cx - s * 0.4f, cy - s * 0.35f),
                       Vec2(cx - s * 0.4f, cy + s * 0.35f), col);
            g.rect(Rect{cx - s * 0.6f, cy - s * 0.08f, s * 1.1f, s * 0.16f}, col.scaled(0.7f));
            break;
        }
        default:
            g.circle(cx, cy, s * 0.5f, col);
            break;
    }
}

void drawStructureGlyph(Gfx& g, const Rect& r, const BuildingDef& b, Color col) {
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float s = std::min(r.w, r.h) * 0.5f;
    if (b.domain == Domain::Space) {
        g.circleOutline(cx, cy, s * 0.85f, col);
        g.circle(cx, cy, s * 0.35f, col);
        g.rect(Rect{cx - s * 0.9f, cy - s * 0.06f, s * 1.8f, s * 0.12f}, col.scaled(0.8f));
    } else {
        g.rect(Rect{cx - s * 0.75f, cy - s * 0.1f, s * 1.5f, s * 0.75f}, col.scaled(0.8f));
        g.triangle(Vec2(cx, cy - s * 0.85f), Vec2(cx + s * 0.8f, cy - s * 0.1f),
                   Vec2(cx - s * 0.8f, cy - s * 0.1f), col);
    }
    if (b.productionTier > 0) {
        for (int i = 0; i < b.productionTier; ++i) {
            g.rect(Rect{cx - s * 0.7f + static_cast<float>(i) * s * 0.5f, cy + s * 0.72f, s * 0.34f,
                        std::max(2.0f, s * 0.1f)},
                   col);
        }
    }
}

}  // namespace

// Shared with PlanetView.cpp so unit tiles look the same everywhere.
void drawUnitGlyphShared(Gfx& g, const Rect& r, UnitClass c, Color col) {
    drawUnitGlyph(g, r, c, col);
}
void drawStructureGlyphShared(Gfx& g, const Rect& r, const BuildingDef& b, Color col) {
    drawStructureGlyph(g, r, b, col);
}

// ---------------------------------------------------------------------------
// Camera. The star map is a plane seen at an angle: points further "north"
// sit further away, so they are foreshortened and shrink with distance.
// ---------------------------------------------------------------------------
Rect App::mapViewport() const {
    return Rect{0, 0, static_cast<float>(gfx_.width()), static_cast<float>(gfx_.height()) - S(kBarH)};
}

namespace {
// Tilt of the galactic plane. cos is the vertical squash, sin how quickly
// things recede; the focal length controls how strong the perspective is.
constexpr float kTiltCos = 0.60f;
constexpr float kTiltSin = 0.80f;
constexpr float kFocal = 1500.0f;
}  // namespace

/// Scrolling in past this zoom dives into the world view.
static constexpr float kZoomDiveThreshold = 3.2f;

float App::perspectiveAt(float worldY) const {
    float depth = -(worldY - viewCamera_.y) * kTiltSin;
    float p = kFocal / std::max(200.0f, kFocal + depth);
    return std::max(0.35f, std::min(2.4f, p));
}

Vec2 App::worldToScreen(Vec2 world) const {
    Rect vp = mapViewport();
    float z = viewZoom_ * gfx_.uiScale();
    float dx = world.x - viewCamera_.x;
    float dy = world.y - viewCamera_.y;
    float p = perspectiveAt(world.y);
    return Vec2(vp.x + vp.w * 0.5f + dx * z * p, vp.y + vp.h * 0.5f + dy * kTiltCos * z * p);
}

Vec2 App::screenToWorld(Vec2 screen) const {
    Rect vp = mapViewport();
    float z = viewZoom_ * gfx_.uiScale();
    float X = (screen.x - (vp.x + vp.w * 0.5f)) / z;
    float Y = (screen.y - (vp.y + vp.h * 0.5f)) / z;
    float denom = kTiltCos * kFocal + Y * kTiltSin;
    if (std::fabs(denom) < 1e-3f) denom = denom < 0.0f ? -1e-3f : 1e-3f;
    float dy = Y * kFocal / denom;
    float p = kFocal / std::max(200.0f, kFocal - dy * kTiltSin);
    return Vec2(viewCamera_.x + X / p, viewCamera_.y + dy);
}

void App::focusOn(Id planet) {
    if (planet == kInvalid) return;
    camera_ = game_.planet(planet).def().pos;
}

void App::centreCameraOnHomeworld() {
    Faction me = game_.playerFaction();
    Vec2 sum;
    int n = 0;
    for (int i = 0; i < game_.planetCount(); ++i) {
        if (game_.planet(i).owner != me) continue;
        sum += game_.planet(i).def().pos;
        ++n;
        if (selectedPlanet_ == kInvalid) selectedPlanet_ = i;
    }
    if (n > 0) camera_ = sum / static_cast<float>(n);
}

Id App::planetAtScreen(float x, float y) const {
    Rect vp = mapViewport();
    if (!vp.contains(x, y)) return kInvalid;
    Id best = kInvalid;
    float bestDist = 1e9f;
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = worldToScreen(game_.planet(i).def().pos);
        float d = distance(p, Vec2(x, y));
        float radius = std::max(S(14.0f), S(10.0f) * zoom_);
        if (d < radius && d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void App::selectPlanet(Id planet) {
    if (planet == selectedPlanet_) return;
    selectedPlanet_ = planet;
    selectedUnits_.clear();
    trayScroll_ = 0.0f;
}

void App::toggleUnitSelection(Id unitId) {
    auto it = std::find(selectedUnits_.begin(), selectedUnits_.end(), unitId);
    if (it == selectedUnits_.end()) {
        selectedUnits_.push_back(unitId);
    } else {
        selectedUnits_.erase(it);
    }
}

void App::selectAllAt(Id planet, Domain domain) {
    Faction me = game_.playerFaction();
    std::vector<Id> ids = game_.unitsAt(planet, me, domain);
    bool allSelected = !ids.empty();
    for (Id id : ids) {
        if (std::find(selectedUnits_.begin(), selectedUnits_.end(), id) == selectedUnits_.end()) {
            allSelected = false;
            break;
        }
    }
    for (Id id : ids) {
        auto it = std::find(selectedUnits_.begin(), selectedUnits_.end(), id);
        if (allSelected) {
            if (it != selectedUnits_.end()) selectedUnits_.erase(it);
        } else if (it == selectedUnits_.end()) {
            selectedUnits_.push_back(id);
        }
    }
}

void App::issueMoveOrder(Id destination) {
    if (selectedUnits_.empty() || destination == kInvalid) return;
    std::vector<Id> valid;
    for (Id id : selectedUnits_) {
        const UnitInstance& u = game_.unit(id);
        if (u.alive && u.owner == game_.playerFaction() && u.planet != kInvalid) valid.push_back(id);
    }
    if (valid.empty()) {
        setStatus("Those units are already in transit");
        return;
    }
    OrderResult r = game_.moveUnits(valid, destination);
    setStatus(r.message);
    if (r.ok) selectedUnits_.clear();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void App::updateGalaxy(float dt) {
    updatePlanetTransition(dt);

    if (input_.keyPressed(SDLK_SPACE)) game_.togglePause();
    if (input_.keyPressed(SDLK_1)) game_.setSpeed(GameSpeed::Normal);
    if (input_.keyPressed(SDLK_2)) game_.setSpeed(GameSpeed::Fast);
    if (input_.keyPressed(SDLK_3)) game_.setSpeed(GameSpeed::Fastest);
    if (input_.keyPressed(SDLK_F1)) showHelp_ = !showHelp_;
    if (input_.keyPressed(SDLK_F2)) {
        designReturn_ = Screen::Galaxy;
        openDesigner();
        return;
    }
    if (input_.keyPressed(SDLK_TAB)) {
        if (planetViewT_ > 0.0f) {
            leavePlanetView();
        } else if (selectedPlanet_ != kInvalid) {
            enterPlanetView(selectedPlanet_);
        }
    }
    if (input_.keyPressed(SDLK_ESCAPE)) {
        if (planetViewT_ > 0.0f) {
            leavePlanetView();
        } else if (showHelp_) {
            showHelp_ = false;
        } else if (!selectedUnits_.empty()) {
            selectedUnits_.clear();
        }
    }
    if (input_.keyPressed(SDLK_q)) category_ = CatFleet;
    if (input_.keyPressed(SDLK_e)) category_ = CatArmy;
    if (input_.keyPressed(SDLK_r)) category_ = CatResearch;
    if (input_.keyPressed(SDLK_f)) category_ = CatWorld;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float z = viewZoom_ * gfx_.uiScale();
    float panSpeed = 460.0f * dt * gfx_.uiScale() / std::max(0.2f, z);
    if (planetViewT_ <= 0.0f) {
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) camera_.x -= panSpeed;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) camera_.x += panSpeed;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) camera_.y -= panSpeed;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) camera_.y += panSpeed;

        // Hold the middle mouse button and drag to pull the galaxy around.
        if (input_.middleDown && (input_.dragDeltaX != 0.0f || input_.dragDeltaY != 0.0f)) {
            camera_.x -= input_.dragDeltaX / z;
            camera_.y -= input_.dragDeltaY / (z * kTiltCos);
        }
    }

    Rect vp = mapViewport();
    float mx = static_cast<float>(input_.mouseX);
    float my = static_cast<float>(input_.mouseY);
    bool overMap = vp.contains(mx, my) && planetViewT_ <= 0.0f;
    if (my < S(120.0f) && mx > static_cast<float>(gfx_.width()) - S(360.0f)) overMap = false;

    hoverPlanet_ = overMap ? planetAtScreen(mx, my) : kInvalid;

    // The wheel zooms; keep scrolling in over a world and the camera dives
    // down to it and opens the world view. Scrolling out backs away again.
    if (input_.wheel != 0) {
        if (planetViewT_ > 0.0f) {
            if (input_.wheel < 0) leavePlanetView();
        } else if (overMap) {
            if (input_.wheel > 0 && zoom_ >= kZoomDiveThreshold) {
                Id target = hoverPlanet_ != kInvalid ? hoverPlanet_ : selectedPlanet_;
                if (target != kInvalid) enterPlanetView(target);
            } else {
                Vec2 before = screenToWorld(Vec2(mx, my));
                zoom_ *= (input_.wheel > 0) ? 1.15f : 1.0f / 1.15f;
                zoom_ = std::max(0.55f, std::min(kZoomDiveThreshold, zoom_));
                viewZoom_ = zoom_;
                Vec2 after = screenToWorld(Vec2(mx, my));
                camera_ += before - after;
            }
        }
    }

    if (!game_.hasPendingPlayerBattle() && !showHelp_ && planetViewT_ <= 0.0f) {
        if (overMap && input_.mouseClicked && hoverPlanet_ != kInvalid && !drag_.active) {
            if (hoverPlanet_ == selectedPlanet_) {
                enterPlanetView(hoverPlanet_);  // click the selected world again to open it
            } else {
                selectPlanet(hoverPlanet_);
            }
        }
        if (overMap && input_.rightClicked && hoverPlanet_ != kInvalid) issueMoveOrder(hoverPlanet_);
    }

    // A press on a unit tile arms a drag; a few pixels of movement starts it.
    if (drag_.armed && input_.mouseDown && !drag_.active) {
        if (distance(Vec2(mx, my), drag_.startPos) > S(6.0f)) drag_.active = true;
    }

    game_.update(dt);
}

// ---------------------------------------------------------------------------
// Star map
// ---------------------------------------------------------------------------
void App::drawRegionLabels() {
    if (viewZoom_ > 2.2f || planetViewT_ > 0.0f) return;

    std::map<std::string, std::pair<Vec2, int>> centres;
    Vec2 galaxyCentre;
    for (int i = 0; i < game_.planetCount(); ++i) {
        const PlanetDef& pd = game_.planet(i).def();
        auto& e = centres[pd.region];
        e.first += pd.pos;
        e.second += 1;
        galaxyCentre += pd.pos;
    }
    if (centres.empty()) return;
    galaxyCentre = galaxyCentre / static_cast<float>(game_.planetCount());

    std::vector<Rect> placed;
    for (const auto& kv : centres) {
        Vec2 c = kv.second.first / static_cast<float>(kv.second.second);
        Vec2 away = (c - galaxyCentre).normalized();
        if (away.lengthSq() < 0.01f) away = Vec2(0.0f, 1.0f);
        Vec2 s = worldToScreen(c + away * 130.0f);

        std::string spaced;
        for (char ch : kv.first) {
            spaced += static_cast<char>(std::toupper(ch));
            spaced += ' ';
        }
        int scale = F(viewZoom_ > 1.2f ? 5 : 4);
        float w = static_cast<float>(Gfx::textWidth(spaced, scale));
        float h = static_cast<float>(Gfx::textHeight(scale));
        Rect box{s.x - w * 0.5f, s.y - h * 0.5f, w, h};

        bool clash = false;
        for (const Rect& r : placed) {
            if (box.x < r.right() + S(30) && box.right() + S(30) > r.x && box.y < r.bottom() + S(24) &&
                box.bottom() + S(24) > r.y) {
                clash = true;
                break;
            }
        }
        if (clash) continue;
        placed.push_back(box);
        gfx_.text(box.x, box.y, spaced, Color(150, 132, 74, 40), scale);
    }
}

void App::drawGalaxy() {
    dropTargets_.clear();
    Rect vp = mapViewport();
    gfx_.pushClip(vp);

    uint32_t seed = 987654321u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };

    // Deep space behind the plane.
    for (int i = 0; i < 500; ++i) {
        Vec2 s(rnd() * vp.w, rnd() * vp.h);
        int v = 40 + static_cast<int>(rnd() * 90.0f);
        gfx_.rect(Rect{s.x, s.y, 1.0f, 1.0f}, Color(v, v, v + 20 > 255 ? 255 : v + 20));
    }

    // The galactic disc: soft clouds laid out along a pair of spiral arms and
    // projected onto the tilted plane, so the map reads as a disc seen from
    // above and to one side.
    const Vec2 hub(520.0f, 540.0f);
    for (int arm = 0; arm < 2; ++arm) {
        for (int i = 0; i < 130; ++i) {
            float t = static_cast<float>(i) / 130.0f;
            float angle = t * 5.4f + static_cast<float>(arm) * 3.14159f + rnd() * 0.28f;
            float radius = 60.0f + t * 480.0f + rnd() * 60.0f;
            Vec2 world(hub.x + std::cos(angle) * radius, hub.y + std::sin(angle) * radius * 0.92f);
            Vec2 s = worldToScreen(world);
            float p = perspectiveAt(world.y);
            float r = (26.0f + rnd() * 54.0f) * viewZoom_ * gfx_.uiScale() * p;
            if (s.x + r < vp.x || s.x - r > vp.right() || s.y + r < vp.y || s.y - r > vp.bottom()) continue;
            int blue = 34 + static_cast<int>(rnd() * 34.0f);
            gfx_.circle(s.x, s.y, r, Color(14 + blue / 3, 16 + blue / 2, blue, 16));
        }
    }
    // A brighter core.
    {
        Vec2 s = worldToScreen(hub);
        float p = perspectiveAt(hub.y);
        float r = 150.0f * viewZoom_ * gfx_.uiScale() * p;
        for (int i = 4; i >= 1; --i) {
            gfx_.circle(s.x, s.y, r * static_cast<float>(i) * 0.28f, Color(40, 42, 66, 14));
        }
    }

    // Stars sitting in the plane, so they slide correctly as the map moves.
    for (int i = 0; i < 700; ++i) {
        Vec2 world(rnd() * 1300.0f - 140.0f, rnd() * 1100.0f - 140.0f);
        Vec2 s = worldToScreen(world);
        if (!vp.contains(s.x, s.y)) continue;
        int v = 60 + static_cast<int>(rnd() * 160.0f);
        float p = perspectiveAt(world.y);
        float size = (rnd() > 0.93f ? S(2.0f) : S(1.0f)) * p;
        gfx_.rect(Rect{s.x, s.y, size, size}, Color(v, v, v + 25 > 255 ? 255 : v + 25));
    }

    drawRegionLabels();

    // Trade routes. Hyperlanes are thick and lit; ordinary lanes are thin.
    for (const LaneDef& l : game_.lanes()) {
        Vec2 a = worldToScreen(game_.planet(l.a).def().pos);
        Vec2 b = worldToScreen(game_.planet(l.b).def().pos);
        Faction oa = game_.planet(l.a).owner;
        Faction ob = game_.planet(l.b).owner;
        if (l.hyperlane) {
            Color c = (oa == ob && oa != Faction::Neutral) ? pal::faction(oa).scaled(0.9f)
                                                           : pal::kHyperlane;
            gfx_.thickLine(a.x, a.y, b.x, b.y, std::max(S(3.0f), S(4.0f) * viewZoom_ * 0.55f), c.withAlpha(90));
            gfx_.thickLine(a.x, a.y, b.x, b.y, std::max(S(1.5f), S(2.0f) * viewZoom_ * 0.55f), c);
        } else {
            gfx_.line(a.x, a.y, b.x, b.y, pal::kLane);
        }
    }

    // Projected route for the units under orders.
    if (selectedPlanet_ != kInvalid && hoverPlanet_ != kInvalid && !selectedUnits_.empty() &&
        hoverPlanet_ != selectedPlanet_) {
        std::vector<Id> path = game_.findPath(selectedPlanet_, hoverPlanet_);
        Id prev = selectedPlanet_;
        float totalDays = 0.0f;
        for (Id step : path) {
            Vec2 a = worldToScreen(game_.planet(prev).def().pos);
            Vec2 b = worldToScreen(game_.planet(step).def().pos);
            gfx_.thickLine(a.x, a.y, b.x, b.y, S(2.5f), pal::kAccent.withAlpha(190));
            totalDays += game_.laneTravelDays(prev, step);
            prev = step;
        }
        if (!path.empty()) {
            Vec2 t = worldToScreen(game_.planet(hoverPlanet_).def().pos);
            gfx_.text(t.x + S(16), t.y - S(34), oneDecimal(totalDays) + " DAYS", pal::kAccent, F(1));
        }
    }

    // Fleets under way.
    for (const Fleet& f : game_.fleets()) {
        if (!f.alive || f.units.empty()) continue;
        Vec2 a = game_.planet(f.from).def().pos;
        Vec2 b = game_.planet(f.to).def().pos;
        Vec2 world = lerp(a, b, std::max(0.0f, std::min(1.0f, f.fraction())));
        Vec2 s = worldToScreen(world);
        Vec2 dir = (b - a).normalized();
        Vec2 perp(-dir.y, dir.x);
        float size = S(8.0f);
        Color c = pal::faction(f.owner);
        gfx_.triangle(s + dir * size, s - dir * size * 0.6f + perp * size * 0.6f,
                      s - dir * size * 0.6f - perp * size * 0.6f, c);
        gfx_.text(s.x + S(10), s.y - S(6), std::to_string(f.units.size()), c, F(1));
    }

    // Worlds, then nameplates in priority order with collision avoidance.
    std::vector<std::pair<float, int>> byPriority;
    for (int i = 0; i < game_.planetCount(); ++i) {
        float priority = static_cast<float>(game_.planet(i).def().baseIncome) * 0.01f;
        if (game_.planet(i).owner == game_.playerFaction()) priority += 5.0f;
        if (game_.isContested(i)) priority += 20.0f;
        if (i == hoverPlanet_) priority += 500.0f;
        if (i == selectedPlanet_) priority += 1000.0f;
        byPriority.push_back({priority, i});
    }
    std::sort(byPriority.begin(), byPriority.end(),
              [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                  return a.first > b.first;
              });
    std::vector<Rect> nameplates;
    badgeBoxes_.clear();

    for (int i = 0; i < game_.planetCount(); ++i) {
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 s = worldToScreen(pd.pos);
        float r = std::max(S(5.0f), S(pd.spaceOnly ? 6.0f : 9.0f) * viewZoom_ * 0.8f) *
                  perspectiveAt(pd.pos.y);
        if (pd.baseIncome > 250) r *= 1.3f;
        Color c = pal::faction(p.owner);

        if (i == selectedPlanet_) {
            gfx_.circleOutline(s.x, s.y, r + S(9.0f), pal::kAccent);
            gfx_.circleOutline(s.x, s.y, r + S(10.0f), pal::kAccent.withAlpha(110));
        }
        if (i == hoverPlanet_) gfx_.circleOutline(s.x, s.y, r + S(5.0f), Color(255, 255, 255, 170));

        if (pd.spaceOnly) {
            gfx_.triangle(Vec2(s.x, s.y - r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c);
            gfx_.triangle(Vec2(s.x, s.y + r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c.scaled(0.7f));
        } else {
            gfx_.circle(s.x, s.y, r, c.scaled(0.42f));
            gfx_.circle(s.x - r * 0.28f, s.y - r * 0.28f, r * 0.55f, c.scaled(0.78f));
            gfx_.circleOutline(s.x, s.y, r, c);
        }

        // Who is present is told by the fleet and army badges, so the world
        // itself stays clean.
        if (game_.isContested(i)) {
            gfx_.circleOutline(s.x, s.y, r + S(13.0f), pal::kDanger);
            gfx_.text(s.x + r + S(8.0f), s.y - r - S(14.0f), "!", pal::kDanger, F(2));
        }
        if (!p.queue.empty() && p.owner == game_.playerFaction()) {
            gfx_.rect(Rect{s.x + r + S(4.0f), s.y - r - S(2.0f), S(4.0f), S(4.0f)}, pal::kWarning);
        }
    }

    // Fleet and army badges: on the star map the three orbital slots read as
    // one stack, and the ten surface cells as one. They go down before the
    // nameplates so the names can dodge them.
    if (planetViewT_ <= 0.0f) drawForceBadges();
    nameplates = badgeBoxes_;

    for (const auto& entry : byPriority) {
        int i = entry.second;
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 s = worldToScreen(pd.pos);
        if (!vp.contains(s.x, s.y)) continue;
        float r = std::max(S(5.0f), S(pd.spaceOnly ? 6.0f : 9.0f) * viewZoom_ * 0.8f) *
                  perspectiveAt(pd.pos.y);
        if (pd.baseIncome > 250) r *= 1.3f;

        bool showIncome = p.owner != Faction::Neutral;
        float w = static_cast<float>(Gfx::textWidth(pd.name, F(2)));
        // The selected world wears its slots above it, so its plate drops below.
        bool below = (i == selectedPlanet_);
        float plateY = below ? s.y + r + S(22.0f) : s.y - r - S(22.0f);
        Rect plate{s.x - w * 0.5f - S(4), plateY, w + S(8), showIncome ? S(26.0f) : S(14.0f)};

        bool clash = false;
        for (const Rect& other : nameplates) {
            if (plate.x < other.right() + S(2) && plate.right() + S(2) > other.x &&
                plate.y < other.bottom() + S(1) && plate.bottom() + S(1) > other.y) {
                clash = true;
                break;
            }
        }
        if (clash && i != selectedPlanet_ && i != hoverPlanet_) continue;
        nameplates.push_back(plate);

        Color c = pal::faction(p.owner);
        gfx_.textCentred(s.x, plate.y, pd.name, c, F(2));
        if (showIncome) {
            gfx_.textCentred(s.x, plate.y + lineH(2) + S(1), "+" + std::to_string(game_.planetIncome(i)),
                             c.scaled(0.85f), F(1));
        }
    }

    gfx_.popClip();

    // The world view takes over the map area, so the overlays that live there
    // step aside while it is open.
    if (planetViewT_ <= 0.0f) {
        drawHeroRoster();
        drawPlanetTooltip();
        drawPausedBanner();
    }
    drawCommandBar();

    if (statusTimer_ > 0.0f && !status_.empty()) {
        Rect r{S(16), vp.bottom() - S(40),
               static_cast<float>(Gfx::textWidth(status_, F(2))) + S(24), S(28)};
        gfx_.panel(r, kConsoleFill, kConsoleEdge);
        gfx_.text(r.x + S(12), r.y + (r.h - lineH(2)) * 0.5f, status_, kReadout, F(2));
    }

    if (showHelp_) {
        Rect r{vp.x + vp.w * 0.5f - S(320), vp.y + S(90), S(640), S(360)};
        gfx_.panel(r, kConsoleFill, pal::kBorderBright);
        gfx_.textCentred(r.x + r.w * 0.5f, r.y + S(14), "CONTROLS", pal::kAccent, F(3));
        const char* lines[] = {
            "LEFT CLICK PLANET      select; click again to dive into the world",
            "RIGHT CLICK PLANET     send the selected units there",
            "MIDDLE MOUSE DRAG      pull the galaxy around",
            "MOUSE WHEEL            zoom; keep scrolling in to enter a world",
            "SPACE                  pause / resume     1 2 3  speed",
            "Q E R F                fleet / army / research / world panels",
            "TAB                    world view       F11  full screen",
            "F1                     this help       F2  unit designer",
            "ESC                    back / clear selection",
            "",
            "RULES OF CONQUEST",
            "- Destroy every enemy ship and orbital gun before landing.",
            "- A world only changes hands when your ground troops hold it.",
            "- Space-only systems are taken by holding orbit alone.",
            "- Income arrives once a week; besieged worlds pay nothing.",
        };
        float y = r.y + S(50);
        for (const char* line : lines) {
            gfx_.text(r.x + S(20), y, line, pal::kText, F(1));
            y += lineH(1) + S(6);
        }
        Rect close{r.x + r.w * 0.5f - S(70), r.bottom() - S(40), S(140), S(28)};
        if (button(gfx_, input_, close, "CLOSE")) showHelp_ = false;
    }

    drawPlanetView();
    if (game_.hasPendingPlayerBattle()) drawBattlePrompt();

    // Finish any drag the player just let go of, then paint what is in hand.
    drawDraggedUnits();
    if (!input_.mouseDown) {
        if (drag_.active) {
            resolveUnitDrop();
        } else {
            drag_ = UnitDrag{};
        }
    }
    drawQueuedTooltip();
}

// ---------------------------------------------------------------------------
// Paused banner
// ---------------------------------------------------------------------------
void App::drawPausedBanner() {
    if (!game_.paused() || game_.hasPendingPlayerBattle()) return;
    const float w = static_cast<float>(gfx_.width());
    Rect banner{w * 0.5f - S(280), 0, S(560), S(46)};
    gfx_.rect(banner, Color(78, 16, 20, 230));
    gfx_.rectOutline(banner, pal::kDanger, 2);
    gfx_.textCentred(banner.x + banner.w * 0.5f, banner.y + (banner.h - lineH(3)) * 0.5f, "GAME PAUSED",
                     Color(255, 190, 190), F(3));

    Rect resume{w * 0.5f - S(150), banner.bottom() + S(6), S(300), S(36)};
    ButtonStyle s;
    s.fill = Color(96, 22, 26);
    s.fillHover = Color(132, 34, 38);
    s.border = pal::kDanger;
    s.text = Color(255, 214, 214);
    s.textScale = F(3);
    if (button(gfx_, input_, resume, "RESUME GAME", true, s)) game_.setSpeed(GameSpeed::Normal);
}

// ---------------------------------------------------------------------------
// Hero roster
// ---------------------------------------------------------------------------
void App::drawHeroRoster() {
    Faction me = game_.playerFaction();
    struct Portrait {
        Id defId;
        Id unitId;
        Id planet;
        int respawn;
    };
    std::vector<Portrait> portraits;
    for (const UnitInstance& u : game_.units()) {
        if (!u.alive || u.owner != me || !u.def().isHero) continue;
        portraits.push_back({u.defId, u.id, u.planet, 0});
    }
    for (const auto& kv : game_.faction(me).heroRespawnTimer) {
        if (kv.second <= 0) continue;
        portraits.push_back({kv.first, kInvalid, kInvalid, kv.second});
    }
    if (portraits.empty()) return;

    const float r = S(24.0f);
    const float pitch = S(52.0f);
    const int perRow = 7;
    float right = static_cast<float>(gfx_.width()) - S(16.0f);

    for (size_t i = 0; i < portraits.size() && i < 14; ++i) {
        int row = static_cast<int>(i) / perRow;
        int col = static_cast<int>(i) % perRow;
        int rowCount = std::min(perRow, static_cast<int>(portraits.size()) - row * perRow);
        float x = right - static_cast<float>(rowCount - col) * pitch + pitch * 0.5f;
        float y = S(32.0f) + static_cast<float>(row) * pitch;
        const Portrait& p = portraits[i];
        const UnitDef& d = db().unit(p.defId);
        bool available = p.unitId != kInvalid;
        Color ring = available ? pal::faction(me) : Color(90, 90, 96);

        Rect hit{x - r, y - r, r * 2, r * 2};
        bool hover = hit.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));

        gfx_.circle(x, y, r, Color(18, 24, 34, 235));
        gfx_.circle(x, y, r - S(3.0f), available ? Color(34, 46, 64) : Color(26, 28, 32));
        gfx_.circleOutline(x, y, r, hover ? pal::kAccent : ring);
        gfx_.circleOutline(x, y, r - 1.0f, (hover ? pal::kAccent : ring).withAlpha(120));
        gfx_.textCentred(x, y - lineH(2) * 0.5f, initials(d.name),
                         available ? pal::kText : Color(120, 120, 128), F(2));

        if (!available) {
            gfx_.textCentred(x, y + S(10.0f), std::to_string(p.respawn) + "D", pal::kWarning, F(1));
        } else if (p.planet != kInvalid && game_.isContested(p.planet)) {
            gfx_.circleOutline(x, y, r + S(3.0f), pal::kDanger);
        }

        if (hover) {
            tipUnit_ = p.defId;
            std::vector<std::string> lines;
            if (available) {
                lines.push_back(p.planet != kInvalid ? "AT " + game_.planet(p.planet).def().name
                                                     : "IN TRANSIT");
            } else {
                lines.push_back("RETURNS IN " + std::to_string(p.respawn) + " DAYS");
            }
            queueTooltip(d.name, lines);
            if (input_.mouseClicked && available && p.planet != kInvalid) {
                selectPlanet(p.planet);
                focusOn(p.planet);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Command console
// ---------------------------------------------------------------------------
void App::drawCommandBar() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    const float barH = S(kBarH);
    Rect bar{0, h - barH, w, barH};

    gfx_.rect(bar, kConsoleFill);
    gfx_.rect(Rect{0, bar.y, w, S(3)}, kConsoleEdge);
    gfx_.rect(Rect{0, bar.y + S(3), w, 1}, Color(96, 168, 118, 120));

    // --- far left: system buttons ---
    float iy = bar.y + S(10);
    struct StripButton {
        const char* label;
        const char* tip;
    };
    const StripButton strip[] = {{"?", "Controls and rules (F1)"},
                                 {"H", "Holonet reports"},
                                 {"W", "World view (TAB, or scroll in)"},
                                 {"X", "Main menu"}};
    for (int i = 0; i < 4; ++i) {
        Rect r{S(6), iy, S(32), S(32)};
        if (button(gfx_, input_, r, strip[i].label)) {
            switch (i) {
                case 0: showHelp_ = !showHelp_; break;
                case 1: category_ = CatHolonet; break;
                case 2: enterPlanetView(selectedPlanet_); break;
                case 3: screen_ = Screen::Menu; break;
            }
        }
        if (r.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY))) {
            queueTooltip(strip[i].tip, {});
        }
        iy += S(38);
    }

    // --- minimap and time controls ---
    Rect mini{S(kIconStripW + 6), bar.y + S(10), S(kMinimapW), S(148)};
    drawMinimap(mini);

    Rect speeds{mini.x, mini.bottom() + S(6), mini.w, S(32)};
    struct SpeedButton {
        const char* label;
        GameSpeed speed;
    };
    const SpeedButton sb[] = {{"||", GameSpeed::Paused},
                              {"1X", GameSpeed::Normal},
                              {"2X", GameSpeed::Fast},
                              {"4X", GameSpeed::Fastest}};
    float bw = speeds.w / 4.0f - S(3);
    for (int i = 0; i < 4; ++i) {
        Rect r{speeds.x + static_cast<float>(i) * (bw + S(4)), speeds.y, bw, speeds.h};
        if (toggleButton(gfx_, input_, r, sb[i].label, game_.speed() == sb[i].speed)) {
            game_.setSpeed(sb[i].speed);
        }
    }

    // --- centre column ---
    const float centreX = mini.right() + S(10);
    const float rightBlockW = S(250);
    const float centreW = w - centreX - rightBlockW - S(10);

    Rect prod{centreX, bar.y + S(10), centreW * 0.34f, S(58)};
    Rect grid{prod.right() + S(8), bar.y + S(8), S(268), S(62)};
    Rect research{grid.right() + S(8), bar.y + S(10), centreX + centreW - (grid.right() + S(8)), S(58)};

    gfx_.panel(prod, kConsoleInner, kConsoleEdge);
    gfx_.text(prod.x + S(8), prod.y + S(6), "PRODUCTION", kReadoutDim, F(1));
    if (selectedPlanet_ != kInvalid && !game_.planet(selectedPlanet_).queue.empty()) {
        const PlanetState& p = game_.planet(selectedPlanet_);
        const BuildOrder& o = p.queue.front();
        std::string name =
            o.kind == BuildKind::Unit ? db().unit(o.defId).name : db().building(o.defId).name;
        gfx_.text(prod.x + S(8), prod.y + S(20), name.substr(0, 30), kReadout, F(1));
        float frac = o.totalDays > 0 ? 1.0f - o.daysRemaining / o.totalDays : 0.0f;
        progressBar(gfx_, Rect{prod.x + S(8), prod.y + S(36), prod.w - S(60), S(12)}, frac, kReadout,
                    Color(10, 20, 16));
        gfx_.textRight(prod.right() - S(8), prod.y + S(37), std::to_string(p.queue.size()) + " QUEUED",
                       kReadoutDim, F(1));
        if (p.owner == game_.playerFaction()) {
            Rect cancel{prod.right() - S(30), prod.y + S(18), S(24), S(14)};
            if (button(gfx_, input_, cancel, "X")) {
                setStatus(game_.cancelBuildOrder(selectedPlanet_, 0, game_.playerFaction()).message);
            }
        }
    } else {
        gfx_.text(prod.x + S(8), prod.y + S(26), "NO ORDERS", kReadoutDim, F(2));
    }

    drawCategoryGrid(grid);

    gfx_.panel(research, kConsoleInner, kConsoleEdge);
    gfx_.text(research.x + S(8), research.y + S(6), "RESEARCH", kReadoutDim, F(1));
    const FactionState& fs = game_.faction(game_.playerFaction());
    if (!fs.research.empty()) {
        const ResearchOrder& ro = fs.research.front();
        gfx_.text(research.x + S(8), research.y + S(20), db().tech(ro.techId).name.substr(0, 30), kReadout,
                  F(1));
        progressBar(gfx_, Rect{research.x + S(8), research.y + S(36), research.w - S(16), S(12)},
                    ro.totalDays > 0 ? 1.0f - ro.daysRemaining / ro.totalDays : 0.0f, kReadout,
                    Color(10, 20, 16));
    } else {
        gfx_.text(research.x + S(8), research.y + S(26), "IDLE", kReadoutDim, F(2));
    }

    Rect status{centreX, bar.y + S(74), centreW, S(22)};
    drawStatusLine(status);

    Rect tray{centreX, bar.y + S(100), centreW, barH - S(108)};
    drawTray(tray);

    Rect actions{w - rightBlockW, bar.y + S(8), rightBlockW - S(8), barH - S(16)};
    drawActionCluster(actions);
}

void App::drawCategoryGrid(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    float bw = (area.w - S(10)) / 4.0f;
    float bh = (area.h - S(8)) / 2.0f;
    for (int i = 0; i < kCategoryCount; ++i) {
        int row = i / 4;
        int col = i % 4;
        Rect r{area.x + S(4) + static_cast<float>(col) * bw, area.y + S(4) + static_cast<float>(row) * bh,
               bw - S(2), bh - S(2)};
        bool enabled = true;
        if (selectedPlanet_ != kInvalid && game_.planet(selectedPlanet_).def().spaceOnly &&
            (i == CatArmy || i == CatSurface)) {
            enabled = false;
        }
        ButtonStyle st;
        st.textScale = F(2);
        if (category_ == i) {
            if (toggleButton(gfx_, input_, r, kCategoryNames[i], true, enabled)) category_ = i;
        } else if (button(gfx_, input_, r, kCategoryNames[i], enabled, st)) {
            category_ = i;
            trayScroll_ = 0.0f;
        }
    }
}

void App::drawStatusLine(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    Faction me = game_.playerFaction();
    float x = area.x + S(8);
    float textY = area.y + (area.h - lineH(2)) * 0.5f;
    float smallY = area.y + (area.h - lineH(1)) * 0.5f;

    if (selectedPlanet_ != kInvalid) {
        const PlanetState& p = game_.planet(selectedPlanet_);
        gfx_.text(x, textY, p.def().name, pal::faction(p.owner), F(2));
        x += static_cast<float>(Gfx::textWidth(p.def().name, F(2))) + S(16);
        gfx_.text(x, smallY, "+" + std::to_string(game_.planetIncome(selectedPlanet_)), kReadout, F(1));
        x += S(60);
        int su = game_.usedUnitSlots(selectedPlanet_, p.owner, Domain::Space);
        int sc = game_.unitSlotCapacity(selectedPlanet_, Domain::Space);
        int gu = game_.usedUnitSlots(selectedPlanet_, p.owner, Domain::Ground);
        int gc = game_.unitSlotCapacity(selectedPlanet_, Domain::Ground);
        gfx_.text(x, smallY,
                  "ORBIT " + std::to_string(su) + "/" + std::to_string(sc) + "   SURFACE " +
                      std::to_string(gu) + "/" + std::to_string(gc),
                  kReadoutDim, F(1));
        x += S(200);
        if (game_.isContested(selectedPlanet_)) {
            gfx_.text(x, smallY, "UNDER SIEGE", pal::kDanger, F(1));
        }
    }

    float rx = area.right() - S(8);
    std::string treasury = credits(game_.faction(me).credits);
    gfx_.textRight(rx, textY, treasury, pal::kWarning, F(2));
    rx -= static_cast<float>(Gfx::textWidth(treasury, F(2))) + S(18);
    gfx_.textRight(rx, smallY, "+" + credits(game_.factionIncome(me)) + "/WK", kReadout, F(1));
    rx -= S(120);
    Rect weekBar{rx - S(90), area.y + (area.h - S(11)) * 0.5f, S(84), S(11)};
    float weekFrac = (static_cast<float>(game_.date().dayOfWeek()) + game_.dayFraction()) /
                     static_cast<float>(kDaysPerWeek);
    progressBar(gfx_, weekBar, weekFrac, kReadout, Color(10, 20, 16));
    rx -= S(100);
    std::string week = "WEEK " + std::to_string(game_.date().week() + 1);
    gfx_.textRight(rx, textY, week, pal::kText, F(2));
    rx -= static_cast<float>(Gfx::textWidth(week, F(2))) + S(18);
    std::string standings;
    for (Faction f : playableFactions()) {
        if (game_.faction(f).defeated) continue;
        standings += std::string(factionShortName(f)) + " " + std::to_string(game_.planetsOwned(f)) + "  ";
    }
    gfx_.textRight(rx, smallY, standings, kReadoutDim, F(1));
}

// ---------------------------------------------------------------------------
// Tray
// ---------------------------------------------------------------------------
void App::drawTray(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    Faction me = game_.playerFaction();
    gfx_.pushClip(area.inset(2));

    float mx = static_cast<float>(input_.mouseX);
    float my = static_cast<float>(input_.mouseY);
    bool overTray = area.contains(mx, my);

    auto scrollAxis = [&](bool horizontal, float contentSize) {
        if (!overTray || input_.wheel == 0) return;
        trayScroll_ -= static_cast<float>(input_.wheel) * S(horizontal ? 64.0f : 34.0f);
        float visible = horizontal ? area.w : area.h;
        trayScroll_ =
            std::max(0.0f, std::min(std::max(0.0f, contentSize - visible + S(12)), trayScroll_));
    };

    if (selectedPlanet_ == kInvalid) {
        gfx_.text(area.x + S(10), area.y + S(10), "SELECT A WORLD ON THE STAR MAP", kReadoutDim, F(2));
        gfx_.popClip();
        return;
    }

    const PlanetState& p = game_.planet(selectedPlanet_);
    const PlanetDef& pd = p.def();
    bool mine = p.owner == me;

    // ---- card strips ----
    if (category_ == CatFleet || category_ == CatArmy || category_ == CatOrbit ||
        category_ == CatSurface) {
        if (!mine) {
            gfx_.text(area.x + S(10), area.y + S(10), "YOU DO NOT CONTROL THIS WORLD", kReadoutDim, F(2));
            gfx_.popClip();
            return;
        }
        bool units = (category_ == CatFleet || category_ == CatArmy);
        Domain domain = (category_ == CatFleet || category_ == CatOrbit) ? Domain::Space : Domain::Ground;

        std::vector<Id> ids;
        if (units) {
            for (Id uid : game_.buildableUnits(selectedPlanet_, me)) {
                if (db().unit(uid).domain() == domain) ids.push_back(uid);
            }
        } else {
            for (Id bid : game_.buildableBuildings(selectedPlanet_, me)) {
                if (db().building(bid).domain == domain) ids.push_back(bid);
            }
        }

        const float cardW = S(104.0f);
        const float cardH = area.h - S(8);
        scrollAxis(true, static_cast<float>(ids.size()) * (cardW + S(6)));

        if (ids.empty() || (units && game_.bestProductionTier(selectedPlanet_, me, domain) == 0)) {
            std::string msg = units ? (domain == Domain::Space
                                           ? "BUILD AN ORBITAL STATION BEFORE LAYING DOWN SHIPS"
                                           : "BUILD A BARRACKS OR FACTORY BEFORE TRAINING TROOPS")
                                    : "NOTHING AVAILABLE HERE";
            gfx_.text(area.x + S(10), area.y + S(10), msg, kReadoutDim, F(1));
            gfx_.popClip();
            return;
        }

        float x = area.x + S(4) - trayScroll_;
        for (Id id : ids) {
            Rect card{x, area.y + S(4), cardW, cardH};
            x += cardW + S(6);
            if (card.right() < area.x || card.x > area.right()) continue;

            OrderResult can = units ? game_.canQueueUnit(selectedPlanet_, id, me)
                                    : game_.canQueueBuilding(selectedPlanet_, id, me);
            bool hover = card.contains(mx, my) && overTray;
            gfx_.rect(card, hover ? Color(26, 44, 34, 235) : Color(16, 26, 24, 220));
            gfx_.rectOutline(card, can.ok ? (hover ? pal::kAccent : kConsoleEdge) : Color(78, 44, 44));

            Rect icon{card.x + S(4), card.y + S(4), card.w - S(8), cardH * 0.42f};
            gfx_.rect(icon, Color(8, 14, 14, 200));
            Color glyphColour = can.ok ? pal::faction(me) : Color(96, 96, 100);
            std::string name;
            int cost;
            float days;
            if (units) {
                const UnitDef& u = db().unit(id);
                drawUnitIcon(icon, id, can.ok ? me : Faction::Neutral, false);
                (void)glyphColour;
                gfx_.text(icon.x + S(3), icon.y + S(3), classTag(u.unitClass), kReadoutDim, F(1));
                if (u.isHero) gfx_.textRight(icon.right() - S(3), icon.y + S(3), "*", pal::kWarning, F(1));
                name = u.name;
                cost = game_.unitCost(selectedPlanet_, id);
                days = game_.unitBuildDays(selectedPlanet_, id);
            } else {
                const BuildingDef& b = db().building(id);
                drawStructureGlyph(gfx_, icon, b, glyphColour);
                name = b.name;
                cost = game_.buildingCost(selectedPlanet_, id);
                days = game_.buildingBuildDays(selectedPlanet_, id);
            }

            std::string line1 = name;
            std::string line2;
            if (name.size() > 17) {
                size_t cut = name.rfind(' ', 17);
                if (cut == std::string::npos || cut < 6) cut = 17;
                line1 = name.substr(0, cut);
                size_t rest = (cut < name.size() && name[cut] == ' ') ? cut + 1 : cut;
                line2 = name.substr(rest, 17);
            }
            Color nameColour = can.ok ? pal::kText : Color(120, 110, 110);
            gfx_.text(card.x + S(5), icon.bottom() + S(4), line1, nameColour, F(1));
            gfx_.text(card.x + S(5), icon.bottom() + S(4) + lineH(1) + S(3), line2, nameColour, F(1));
            gfx_.text(card.x + S(5), card.bottom() - lineH(2) - S(4), credits(cost), pal::kWarning, F(2));
            gfx_.textRight(card.right() - S(5), card.bottom() - lineH(1) - S(4), oneDecimal(days) + "D",
                           kReadoutDim, F(1));

            if (hover) {
                if (units) {
                    tipUnit_ = id;
                } else {
                    tipBuilding_ = id;
                }
                if (!can.ok) queueTooltip(std::string(), {can.message});
                if (input_.mouseClicked) {
                    OrderResult r = units ? game_.queueUnit(selectedPlanet_, id, me)
                                          : game_.queueBuilding(selectedPlanet_, id, me);
                    setStatus(r.message);
                }
            }
        }
        gfx_.popClip();
        return;
    }

    // ---- research ----
    if (category_ == CatResearch) {
        std::vector<Id> options = game_.researchableTechs(me);
        const float rowH = S(30);
        scrollAxis(false, static_cast<float>(options.size()) * rowH);
        float y = area.y + S(6) - trayScroll_;
        if (options.empty()) {
            gfx_.text(area.x + S(10), area.y + S(10), "ALL TECHNOLOGY RESEARCHED", kReadout, F(2));
        }
        for (Id tid : options) {
            const TechDef& t = db().tech(tid);
            Rect row{area.x + S(4), y, area.w - S(8), rowH - S(2)};
            y += rowH;
            if (row.bottom() < area.y || row.y > area.bottom()) continue;
            bool hover = row.contains(mx, my) && overTray;
            int cost = game_.techCost(me, tid);
            bool afford = game_.faction(me).credits >= cost && game_.faction(me).research.empty();
            gfx_.rect(row, hover ? Color(26, 44, 34, 230) : Color(16, 26, 24, 200));
            gfx_.text(row.x + S(8), row.y + S(3), "T" + std::to_string(t.tier) + "  " + t.name, pal::kText,
                      F(1));
            gfx_.text(row.x + S(8), row.y + S(3) + lineH(1) + S(2), t.unlocksText.substr(0, 74),
                      kReadoutDim, F(1));
            gfx_.textRight(row.right() - S(120), row.y + row.h * 0.5f - lineH(1) * 0.5f,
                           credits(cost) + " CR", pal::kWarning, F(1));
            gfx_.textRight(row.right() - S(70), row.y + row.h * 0.5f - lineH(1) * 0.5f,
                           oneDecimal(game_.techDays(me, tid)) + "D", kReadoutDim, F(1));
            Rect go{row.right() - S(62), row.y + S(4), S(56), row.h - S(8)};
            ButtonStyle st;
            st.textScale = F(1);
            if (button(gfx_, input_, go, "RESEARCH", afford, st)) {
                setStatus(game_.startResearch(me, tid).message);
            }
            if (hover) queueTooltip(t.name, {t.description, "UNLOCKS: " + t.unlocksText});
        }
        gfx_.popClip();
        return;
    }

    // ---- heroes ----
    if (category_ == CatHeroes) {
        std::vector<Id> recruitable;
        if (mine) {
            for (Id uid : game_.buildableUnits(selectedPlanet_, me)) {
                if (db().unit(uid).isHero) recruitable.push_back(uid);
            }
        }
        gfx_.text(area.x + S(6), area.y + S(6), "RECRUIT AT " + pd.name, kReadoutDim, F(1));
        float y = area.y + S(22);
        const float rowH = S(26);
        for (Id uid : recruitable) {
            const UnitDef& u = db().unit(uid);
            OrderResult can = game_.canQueueUnit(selectedPlanet_, uid, me);
            Rect row{area.x + S(4), y, area.w * 0.5f - S(8), rowH - S(2)};
            y += rowH;
            if (row.bottom() > area.bottom()) break;
            bool hover = row.contains(mx, my) && overTray;
            gfx_.rect(row, hover ? Color(26, 44, 34, 230) : Color(16, 26, 24, 200));
            gfx_.circle(row.x + S(14), row.y + row.h * 0.5f, S(9), Color(34, 46, 64));
            gfx_.circleOutline(row.x + S(14), row.y + row.h * 0.5f, S(9), pal::faction(me));
            gfx_.textCentred(row.x + S(14), row.y + row.h * 0.5f - lineH(1) * 0.5f, initials(u.name),
                             pal::kText, F(1));
            gfx_.text(row.x + S(30), row.y + row.h * 0.5f - lineH(1) * 0.5f, u.name.substr(0, 26),
                      can.ok ? pal::kText : kReadoutDim, F(1));
            gfx_.textRight(row.right() - S(56), row.y + row.h * 0.5f - lineH(1) * 0.5f,
                           credits(game_.unitCost(selectedPlanet_, uid)), pal::kWarning, F(1));
            Rect go{row.right() - S(50), row.y + S(3), S(46), row.h - S(6)};
            ButtonStyle st;
            st.textScale = F(1);
            if (button(gfx_, input_, go, "HIRE", can.ok, st)) {
                setStatus(game_.queueUnit(selectedPlanet_, uid, me).message);
            }
            if (hover) {
                tipUnit_ = uid;
                if (!can.ok) queueTooltip(std::string(), {can.message});
            }
        }
        if (recruitable.empty()) {
            gfx_.text(area.x + S(10), area.y + S(26), mine ? "NO COMMANDERS AVAILABLE HERE" : "-",
                      kReadoutDim, F(1));
        }

        float rx = area.x + area.w * 0.5f + S(8);
        gfx_.text(rx, area.y + S(6), "IN THE FIELD", kReadoutDim, F(1));
        float ry = area.y + S(22);
        for (const UnitInstance& u : game_.units()) {
            if (!u.alive || u.owner != me || !u.def().isHero) continue;
            if (ry + lineH(1) > area.bottom()) break;
            gfx_.text(rx, ry, u.def().name.substr(0, 26), pal::kText, F(1));
            gfx_.textRight(area.right() - S(8), ry,
                           u.planet != kInvalid ? game_.planet(u.planet).def().name : "IN TRANSIT",
                           kReadoutDim, F(1));
            ry += lineH(1) + S(4);
        }
        gfx_.popClip();
        return;
    }

    // ---- world dossier and garrison ----
    if (category_ == CatWorld) {
        float y = area.y + S(6);
        gfx_.text(area.x + S(8), y, pd.name + "  -  " + pd.region, pal::faction(p.owner), F(2));
        gfx_.textRight(area.right() - S(8), y + S(2),
                       pd.spaceOnly ? "SPACE-ONLY SYSTEM"
                                    : "OWNER: " + std::string(factionShortName(p.owner)),
                       pd.spaceOnly ? pal::kWarning : kReadoutDim, F(1));
        y += lineH(2) + S(6);
        for (Trait t : pd.traits) {
            gfx_.text(area.x + S(8), y, std::string("+ ") + traitName(t), pal::kAccent, F(1));
            gfx_.text(area.x + S(160), y, traitDescription(t), kReadoutDim, F(1));
            y += lineH(1) + S(3);
        }
        if (!p.buildings.empty()) {
            std::string list = "BUILT: ";
            for (Id bid : p.buildings) {
                const BuildingInstance& b = game_.buildingInst(bid);
                if (b.alive) list += b.def().name + ", ";
            }
            gfx_.text(area.x + S(8), y, list.substr(0, 96), kReadoutDim, F(1));
            y += lineH(1) + S(4);
        }

        float listY = y + S(2);
        float colW = area.w * 0.5f - S(12);
        const float rowH = lineH(1) + S(4);
        int column = 0;
        for (Id id : p.units) {
            const UnitInstance& u = game_.unit(id);
            if (!u.alive) continue;
            if (listY + rowH > area.bottom()) {
                ++column;
                listY = y + S(2);
                if (column > 1) break;
            }
            Rect row{area.x + S(8) + static_cast<float>(column) * (colW + S(8)), listY, colW, rowH - S(1)};
            listY += rowH;
            bool isMine = u.owner == me;
            bool selected =
                std::find(selectedUnits_.begin(), selectedUnits_.end(), id) != selectedUnits_.end();
            bool hover = isMine && row.contains(mx, my) && overTray;
            if (selected) gfx_.rect(row, Color(34, 74, 52, 220));
            else if (hover) gfx_.rect(row, Color(24, 40, 32, 200));
            gfx_.text(row.x + S(2), row.y + S(1), classTag(u.def().unitClass), pal::faction(u.owner), F(1));
            gfx_.text(row.x + S(28), row.y + S(1), u.def().name.substr(0, 24),
                      isMine ? pal::kText : kReadoutDim, F(1));
            if (u.def().domain() == Domain::Ground) {
                gfx_.text(row.right() - S(96), row.y + S(1), u.landed ? "SURFACE" : "ORBIT",
                          u.landed ? kReadout : pal::kWarning, F(1));
            }
            progressBar(gfx_, Rect{row.right() - S(44), row.y + S(2), S(40), S(7)}, u.health,
                        u.health > 0.6f ? kReadout : (u.health > 0.3f ? pal::kWarning : pal::kDanger),
                        Color(10, 18, 16));
            if (hover) {
                tipUnit_ = u.defId;
                if (input_.mouseClicked) toggleUnitSelection(id);
            }
        }
        gfx_.popClip();
        return;
    }

    // ---- holonet ----
    if (category_ == CatHolonet) {
        const std::deque<GameEvent>& events = game_.events();
        float y = area.y + S(6);
        int shown = 0;
        for (auto it = events.rbegin(); it != events.rend() && y < area.bottom() - lineH(1); ++it) {
            Color c = it->faction == Faction::Neutral ? kReadoutDim : pal::faction(it->faction);
            gfx_.text(area.x + S(8), y, "D" + std::to_string(it->day) + "  " + it->text.substr(0, 110), c,
                      F(1));
            y += lineH(1) + S(3);
            ++shown;
        }
        if (shown == 0) gfx_.text(area.x + S(8), y, "NO REPORTS YET", kReadoutDim, F(1));
        gfx_.popClip();
        return;
    }

    gfx_.popClip();
}

// ---------------------------------------------------------------------------
// Right hand action cluster
// ---------------------------------------------------------------------------
void App::drawActionCluster(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    Faction me = game_.playerFaction();

    gfx_.text(area.x + S(8), area.y + S(6), "SELECTED " + std::to_string(selectedUnits_.size()),
              kReadoutDim, F(1));

    ButtonStyle small;
    small.textScale = F(2);
    Rect allShips{area.x + S(8), area.y + S(22), area.w * 0.5f - S(12), S(26)};
    Rect allTroops{area.x + area.w * 0.5f, area.y + S(22), area.w * 0.5f - S(12), S(26)};
    if (button(gfx_, input_, allShips, "ALL SHIPS", selectedPlanet_ != kInvalid, small)) {
        selectAllAt(selectedPlanet_, Domain::Space);
        category_ = CatWorld;
    }
    if (button(gfx_, input_, allTroops, "ALL TROOPS", selectedPlanet_ != kInvalid, small)) {
        selectAllAt(selectedPlanet_, Domain::Ground);
        category_ = CatWorld;
    }

    bool canWithdraw = selectedPlanet_ != kInvalid && game_.isContested(selectedPlanet_) &&
                       !game_.allUnitsAt(selectedPlanet_, me).empty();
    Rect withdraw{area.x + S(8), area.y + S(52), area.w - S(16), S(24)};
    ButtonStyle danger;
    danger.fill = Color(62, 30, 30);
    danger.fillHover = Color(96, 42, 40);
    danger.border = pal::kDanger;
    danger.textScale = F(2);
    if (button(gfx_, input_, withdraw, "WITHDRAW", canWithdraw, danger)) {
        setStatus(game_.withdraw(selectedPlanet_, me).message);
        selectedUnits_.clear();
    }

    std::vector<Id> landing;
    if (selectedPlanet_ != kInvalid) {
        for (Id id : selectedUnits_) {
            const UnitInstance& u = game_.unit(id);
            if (u.alive && u.owner == me && u.planet == selectedPlanet_ &&
                u.def().domain() == Domain::Ground && !u.landed) {
                landing.push_back(id);
            }
        }
        if (landing.empty()) {
            landing = game_.unitsAt(selectedPlanet_, me, Domain::Ground, false, true);
        }
    }
    bool canLand = selectedPlanet_ != kInvalid && !landing.empty() &&
                   !game_.planet(selectedPlanet_).def().spaceOnly &&
                   game_.orbitClearFor(selectedPlanet_, me);
    bool hostile = selectedPlanet_ != kInvalid && game_.planet(selectedPlanet_).owner != me;

    float cx = area.x + area.w * 0.5f;
    float cy = area.bottom() - S(54);
    float r = S(44.0f);
    bool hover = distance(Vec2(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)),
                          Vec2(cx, cy)) < r;
    Color ring = canLand ? (hostile ? pal::kDanger : pal::kAccent) : Color(70, 74, 80);
    gfx_.circle(cx, cy, r, canLand ? (hover ? Color(52, 30, 30) : Color(30, 40, 40)) : Color(20, 24, 28));
    gfx_.circleOutline(cx, cy, r, ring);
    gfx_.circleOutline(cx, cy, r - S(3), ring.withAlpha(120));
    gfx_.textCentred(cx, cy - lineH(2), hostile ? "INVADE" : "DEPLOY",
                     canLand ? pal::kText : Color(110, 114, 120), F(2));
    gfx_.textCentred(cx, cy + S(2), std::to_string(landing.size()) + " UNITS",
                     canLand ? kReadoutDim : Color(90, 94, 100), F(1));
    if (canLand && hover) {
        queueTooltip(hostile ? "INVADE" : "DEPLOY",
                     {"Land your ground forces from orbit.",
                      hostile ? "Defended worlds trigger a ground battle; you must hold the surface "
                                "to take the planet."
                              : "Troops on the surface defend the world against invasion."});
        if (input_.mouseClicked) {
            OrderResult res = game_.invade(selectedPlanet_, landing, me);
            setStatus(res.message);
            selectedUnits_.clear();
        }
    } else if (!canLand && hover && selectedPlanet_ != kInvalid) {
        queueTooltip("LANDING UNAVAILABLE",
                     {game_.planet(selectedPlanet_).def().spaceOnly
                          ? "This system has no surface: hold orbit to take it."
                          : (!game_.orbitClearFor(selectedPlanet_, me)
                                 ? "Enemy ships or orbital guns still hold the orbit."
                                 : "No ground forces waiting in orbit here.")});
    }
}

// ---------------------------------------------------------------------------
// Force badges on the star map
//
// Empire at War shows a world's forces as a couple of small icons in orbit,
// not as a spreadsheet: the fleet is one stack and the army is another. The
// slots themselves are only unfolded in the world view. Both badges are drag
// handles - pick one up and drop it on another world to send it there, or on
// the army badge to land the troops.
// ---------------------------------------------------------------------------
void App::drawForceBadges() {
    Faction me = game_.playerFaction();
    float mx = static_cast<float>(input_.mouseX);
    float my = static_cast<float>(input_.mouseY);

    for (int i = 0; i < game_.planetCount(); ++i) {
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 centre = worldToScreen(pd.pos);
        Rect vp = mapViewport();
        if (!vp.contains(centre.x, centre.y)) continue;
        float pr = std::max(S(5.0f), S(pd.spaceOnly ? 6.0f : 9.0f) * viewZoom_ * 0.8f) *
                   perspectiveAt(pd.pos.y);
        if (pd.baseIncome > 250) pr *= 1.3f;

        // Every world is somewhere a dragged stack can be sent.
        float hit = std::max(S(18.0f), pr + S(10.0f));
        if (drag_.active) {
            addMoveTarget(Rect{centre.x - hit, centre.y - hit, hit * 2.0f, hit * 2.0f}, i);
            bool over = std::fabs(mx - centre.x) < hit && std::fabs(my - centre.y) < hit;
            if (over) gfx_.circleOutline(centre.x, centre.y, hit, pal::kAccent);
        }

        const float badgeW = S(46.0f);
        const float badgeH = S(26.0f);
        float stackX = centre.x + pr + S(5.0f);
        float stackY = centre.y - pr - S(4.0f);

        for (int fi = 0; fi < kFactionCount; ++fi) {
            Faction f = factionFromIndex(fi);
            std::vector<Id> ships = game_.unitsAt(i, f, Domain::Space);
            std::vector<Id> orbitingTroops = game_.unitsAt(i, f, Domain::Ground, false, true);
            ships.insert(ships.end(), orbitingTroops.begin(), orbitingTroops.end());
            if (ships.empty()) continue;

            Rect badge{stackX, stackY, badgeW, badgeH};
            stackY -= badgeH + S(3.0f);
            badgeBoxes_.push_back(badge);
            Color c = pal::faction(f);
            bool hot = drag_.active && badge.contains(mx, my);
            bool hover = badge.contains(mx, my);
            gfx_.rect(badge, hot ? Color(28, 50, 62, 240) : Color(10, 16, 24, 226));
            gfx_.rectOutline(badge, hot || hover ? pal::kAccent : c);

            // The heaviest ship in the stack stands for the whole fleet.
            Id flagship = ships.front();
            for (Id id : ships) {
                if (static_cast<int>(game_.unit(id).def().unitClass) <
                    static_cast<int>(game_.unit(flagship).def().unitClass)) {
                    flagship = id;
                }
            }
            drawUnitIcon(Rect{badge.x + S(2), badge.y + S(2), badgeH - S(4), badgeH - S(4)},
                         game_.unit(flagship).defId, f, false);
            gfx_.textRight(badge.right() - S(4), badge.y + (badge.h - lineH(2)) * 0.5f,
                           "x" + std::to_string(ships.size()), c, F(2));

            if (f == me) {
                addDropTarget(badge, 0, false);
                if (hover && input_.mouseDown && !drag_.armed && !drag_.active) {
                    beginStackDrag(ships, false);
                }
                if (hover && input_.mouseClicked && !drag_.active) {
                    selectPlanet(i);
                    selectedUnits_ = ships;
                }
            }
            if (hover && !drag_.active) {
                std::vector<std::string> lines;
                lines.push_back(std::to_string(ships.size()) + " units in orbit");
                if (f == me) lines.push_back("Drag to another world to send them there");
                queueTooltip(std::string(factionShortName(f)) + " FLEET", lines);
            }
        }

        // The army badge sits on the world itself.
        if (!pd.spaceOnly) {
            float armyY = centre.y + pr + S(4.0f);
            for (int fi = 0; fi < kFactionCount; ++fi) {
                Faction f = factionFromIndex(fi);
                std::vector<Id> troops = game_.unitsAt(i, f, Domain::Ground, true);
                if (troops.empty()) continue;
                Rect badge{centre.x - badgeW * 0.5f, armyY, badgeW, badgeH};
                armyY += badgeH + S(3.0f);
                badgeBoxes_.push_back(badge);
                Color c = pal::faction(f);
                bool hot = drag_.active && badge.contains(mx, my);
                bool hover = badge.contains(mx, my);
                gfx_.rect(badge, hot ? Color(30, 58, 40, 240) : Color(10, 20, 16, 226));
                gfx_.rectOutline(badge, hot || hover ? pal::kAccent : c);
                drawUnitIcon(Rect{badge.x + S(2), badge.y + S(2), badgeH - S(4), badgeH - S(4)},
                             game_.unit(troops.front()).defId, f, false);
                gfx_.textRight(badge.right() - S(4), badge.y + (badge.h - lineH(2)) * 0.5f,
                               "x" + std::to_string(troops.size()), c, F(2));

                // Anyone can drop troops here: that is an invasion.
                addDropTarget(badge, -1, true);
                if (f == me) {
                    if (hover && input_.mouseDown && !drag_.armed && !drag_.active) {
                        beginStackDrag(troops, true);
                    }
                    if (hover && input_.mouseClicked && !drag_.active) {
                        selectPlanet(i);
                        selectedUnits_ = troops;
                    }
                }
                if (hover && !drag_.active) {
                    queueTooltip(std::string(factionShortName(f)) + " ARMY",
                                 {std::to_string(troops.size()) + " / " +
                                      std::to_string(kGroundSlotCapacity) + " divisions on the surface",
                                  f == me ? "Drag into orbit to load the transports" : ""});
                }
            }
            // Even an empty surface accepts a landing.
            if (drag_.active && drag_.fromSurface == false) {
                Rect landing{centre.x - badgeW * 0.5f, centre.y + pr + S(4.0f), badgeW, badgeH};
                addDropTarget(landing, -1, true);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Minimap
// ---------------------------------------------------------------------------
void App::drawMinimap(const Rect& area) {
    gfx_.panel(area, Color(8, 14, 14, 240), kConsoleEdge);
    gfx_.pushClip(area.inset(2));

    Vec2 lo(1e9f, 1e9f), hi(-1e9f, -1e9f);
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = game_.planet(i).def().pos;
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
    }
    Rect inner = area.inset(S(6));
    float s = std::min(inner.w / std::max(1.0f, hi.x - lo.x), inner.h / std::max(1.0f, hi.y - lo.y));
    auto toMini = [&](Vec2 p) {
        return Vec2(inner.x + (p.x - lo.x) * s + (inner.w - (hi.x - lo.x) * s) * 0.5f,
                    inner.y + (p.y - lo.y) * s + (inner.h - (hi.y - lo.y) * s) * 0.5f);
    };

    for (const LaneDef& l : game_.lanes()) {
        Vec2 a = toMini(game_.planet(l.a).def().pos);
        Vec2 b = toMini(game_.planet(l.b).def().pos);
        gfx_.line(a.x, a.y, b.x, b.y, l.hyperlane ? pal::kHyperlane.scaled(0.55f) : Color(30, 46, 40));
    }
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = toMini(game_.planet(i).def().pos);
        Color c = pal::faction(game_.planet(i).owner);
        float r = game_.isContested(i) ? S(3.0f) : S(2.0f);
        gfx_.rect(Rect{p.x - r, p.y - r, r * 2, r * 2}, c);
        if (i == selectedPlanet_) gfx_.rectOutline(Rect{p.x - S(4), p.y - S(4), S(8), S(8)}, pal::kAccent);
    }

    // What the main view is looking at. The tilted projection can throw the
    // corners a long way off, so the marker is clamped to the minimap.
    Vec2 topLeft = screenToWorld(Vec2(0, 0));
    Vec2 bottomRight = screenToWorld(Vec2(mapViewport().right(), mapViewport().bottom()));
    Vec2 a = toMini(topLeft);
    Vec2 b = toMini(bottomRight);
    float x0 = std::max(inner.x, std::min(a.x, b.x));
    float y0 = std::max(inner.y, std::min(a.y, b.y));
    float x1 = std::min(inner.right(), std::max(a.x, b.x));
    float y1 = std::min(inner.bottom(), std::max(a.y, b.y));
    if (x1 > x0 && y1 > y0) {
        gfx_.rectOutline(Rect{x0, y0, x1 - x0, y1 - y0}, Color(255, 255, 255, 120));
    }
    gfx_.popClip();

    if (area.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
        (input_.mouseClicked || (input_.mouseDown && input_.dragStartX > area.x))) {
        float wx = lo.x + (static_cast<float>(input_.mouseX) - inner.x -
                           (inner.w - (hi.x - lo.x) * s) * 0.5f) / s;
        float wy = lo.y + (static_cast<float>(input_.mouseY) - inner.y -
                           (inner.h - (hi.y - lo.y) * s) * 0.5f) / s;
        camera_ = Vec2(wx, wy);
    }
}

// ---------------------------------------------------------------------------
// Tooltips and info cards
// ---------------------------------------------------------------------------
void App::queueTooltip(const std::string& title, const std::vector<std::string>& lines) {
    tipTitle_ = title;
    tipLines_ = lines;
}

void App::drawInfoCard(const Rect& anchor, Id defId, bool isUnit) {
    // The EaW unit card: portrait box, name and price, then the dossier.
    std::vector<std::pair<std::string, std::string>> rows;
    std::vector<std::string> body;
    std::string name;
    int cost = 0;
    UnitClass glyphClass = UnitClass::Corvette;
    const BuildingDef* bdef = nullptr;

    if (isUnit) {
        const UnitDef& u = db().unit(defId);
        name = u.name;
        cost = selectedPlanet_ != kInvalid ? game_.unitCost(selectedPlanet_, defId) : u.cost;
        glyphClass = u.unitClass;
        rows.push_back({"CLASS", unitClassName(u.unitClass)});
        if (!u.role.empty()) rows.push_back({"ROLE", u.role});
        if (!u.manufacturer.empty()) rows.push_back({"MANUFACTURER", u.manufacturer});
        rows.push_back({"SLOTS", std::to_string(u.popCost) + "   BUILD TIME " +
                                    oneDecimal(selectedPlanet_ != kInvalid
                                                   ? game_.unitBuildDays(selectedPlanet_, defId)
                                                   : static_cast<float>(u.buildDays)) +
                                    " DAYS"});
        body.push_back(u.description);
        rows.push_back({"HULL", std::to_string(static_cast<int>(u.hull))});
        if (u.shield > 0.0f) {
            rows.push_back({"SHIELD", std::to_string(static_cast<int>(u.shield)) + "  (regen " +
                                          oneDecimal(u.shieldRegen) + "/s)"});
        } else {
            rows.push_back({"SHIELD", "none"});
        }
        rows.push_back({"SPEED", std::to_string(static_cast<int>(u.speed)) + "   RANGE " +
                                     std::to_string(static_cast<int>(u.range))});
        rows.push_back({"WEAPONS", std::to_string(static_cast<int>(u.damageAntiCapital)) +
                                       " anti-capital, " +
                                       std::to_string(static_cast<int>(u.damageAntiFighter)) +
                                       " anti-squadron"});
        if (!u.wings.empty()) {
            std::string wings;
            for (const CarriedWing& w : u.wings) {
                if (w.unitId == kInvalid) continue;
                if (!wings.empty()) wings += ", ";
                wings += std::to_string(w.count) + "x " + db().unit(w.unitId).name;
            }
            rows.push_back({"COMPLEMENT", wings});
        }
        if (u.requiredTech != kInvalid) {
            rows.push_back({"REQUIRES", db().tech(u.requiredTech).name});
        }
        rows.push_back({"PRODUCED AT", std::string(u.domain() == Domain::Space ? "orbital station tier "
                                                                              : "ground facility tier ") +
                                           std::to_string(u.requiredTier)});
        if (u.isHero) {
            std::string bonus;
            if (u.heroCombatBonus > 0.0f) {
                bonus += "+" + std::to_string(static_cast<int>(u.heroCombatBonus * 100.0f)) + "% combat";
            }
            if (u.heroIncomeBonus > 0) {
                if (!bonus.empty()) bonus += ", ";
                bonus += "+" + std::to_string(u.heroIncomeBonus) + " credits/week";
            }
            rows.push_back({"COMMAND", bonus});
        }
    } else {
        const BuildingDef& b = db().building(defId);
        bdef = &b;
        name = b.name;
        cost = selectedPlanet_ != kInvalid ? game_.buildingCost(selectedPlanet_, defId) : b.cost;
        rows.push_back({"CLASS", b.domain == Domain::Space ? "Orbital structure" : "Surface structure"});
        rows.push_back({"BUILD TIME", std::to_string(b.buildDays) + " DAYS"});
        body.push_back(b.description);
        if (b.productionTier > 0) {
            rows.push_back({"PRODUCTION", "unlocks tier " + std::to_string(b.productionTier) + " units"});
        }
        if (b.incomeFlat > 0 || b.incomeMult > 0.0f) {
            rows.push_back({"INCOME", "+" + std::to_string(b.incomeFlat) + " and +" +
                                          std::to_string(static_cast<int>(b.incomeMult * 100.0f)) + "%"});
        }
        if (b.defenceHp > 0.0f) {
            rows.push_back({"DEFENCE", std::to_string(static_cast<int>(b.defenceHp)) + " HP, " +
                                           std::to_string(static_cast<int>(b.defenceDamage)) + " damage"});
        }
        if (b.shieldStrength > 0.0f) {
            rows.push_back({"SHIELD", std::to_string(static_cast<int>(b.shieldStrength))});
        }
        if (b.unitSlotBonus > 0) rows.push_back({"CAPACITY", "+" + std::to_string(b.unitSlotBonus) + " slots"});
        if (b.researchSpeed > 0.0f) {
            rows.push_back({"RESEARCH", "-" + std::to_string(static_cast<int>(b.researchSpeed * 100.0f)) +
                                            "% research time"});
        }
        if (b.requiredTech != kInvalid) rows.push_back({"REQUIRES", db().tech(b.requiredTech).name});
        if (b.requiredTrait != Trait::Count) {
            rows.push_back({"ONLY ON", std::string(traitName(b.requiredTrait)) + "S"});
        }
    }

    // Measure.
    const float labelW = S(112);
    float width = S(400);
    const float pad = S(10);
    float height = S(52) + pad;
    for (const std::string& line : body) {
        int chars = std::max(10, static_cast<int>((width - pad * 2) / (6.0f * static_cast<float>(F(1)))));
        int rowsNeeded = static_cast<int>(line.size()) / chars + 1;
        height += static_cast<float>(rowsNeeded) * (lineH(1) + S(3)) + S(4);
    }
    height += static_cast<float>(rows.size()) * (lineH(1) + S(4)) + pad;

    Rect box{anchor.x, anchor.y - height - S(8), width, height};
    if (box.right() > static_cast<float>(gfx_.width())) {
        box.x = static_cast<float>(gfx_.width()) - box.w - S(6);
    }
    if (box.x < S(4)) box.x = S(4);
    if (box.y < S(4)) box.y = std::min(anchor.bottom() + S(8), static_cast<float>(gfx_.height()) - height - S(4));

    gfx_.panel(box, Color(10, 14, 18, 248), pal::kBorderBright);
    // Header: icon, name, price.
    Rect icon{box.x + pad, box.y + S(8), S(52), S(36)};
    gfx_.rect(icon, Color(6, 10, 12, 220));
    gfx_.rectOutline(icon, kConsoleEdge);
    if (isUnit) {
        drawUnitIcon(icon, defId, db().unit(defId).faction, false);
        (void)glyphClass;
    } else if (bdef != nullptr) {
        drawStructureGlyph(gfx_, icon, *bdef, pal::faction(game_.playerFaction()));
    }
    // Name on the left, price on the right; the name gives way if they clash.
    std::string price = credits(cost) + " CR";
    float priceW = static_cast<float>(Gfx::textWidth(price, F(2)));
    float nameRoom = box.right() - pad - priceW - S(12) - (icon.right() + S(10));
    std::string shownName = name;
    while (!shownName.empty() &&
           static_cast<float>(Gfx::textWidth(shownName, F(2))) > nameRoom) {
        shownName.pop_back();
    }
    gfx_.text(icon.right() + S(10), box.y + S(10), shownName, pal::kText, F(2));
    gfx_.textRight(box.right() - pad, box.y + S(10), price, pal::kWarning, F(2));
    gfx_.line(box.x + pad, box.y + S(48), box.right() - pad, box.y + S(48), kConsoleEdge);

    float y = box.y + S(54);
    for (const std::string& line : body) {
        y += wrappedText(gfx_, Rect{box.x + pad, y, box.w - pad * 2, height}, line, pal::kTextDim, F(1)) +
             S(4);
    }
    for (const auto& kv : rows) {
        gfx_.text(box.x + pad, y, kv.first, kReadoutDim, F(1));
        gfx_.text(box.x + pad + labelW, y, kv.second.substr(0, 40), pal::kText, F(1));
        y += lineH(1) + S(4);
    }
}

void App::drawQueuedTooltip() {
    Rect anchor{static_cast<float>(input_.mouseX) + S(18), static_cast<float>(input_.mouseY), S(10), S(10)};
    if (tipUnit_ != kInvalid) {
        drawInfoCard(anchor, tipUnit_, true);
    } else if (tipBuilding_ != kInvalid) {
        drawInfoCard(anchor, tipBuilding_, false);
    }

    if (!tipTitle_.empty() || (!tipLines_.empty() && tipUnit_ == kInvalid && tipBuilding_ == kInvalid)) {
        float w = static_cast<float>(Gfx::textWidth(tipTitle_, F(2)));
        for (const std::string& l : tipLines_) {
            w = std::max(w, static_cast<float>(Gfx::textWidth(l.substr(0, 74), F(1))));
        }
        float h = (tipTitle_.empty() ? 0.0f : lineH(2) + S(6)) +
                  static_cast<float>(tipLines_.size()) * (lineH(1) + S(3)) + S(12);
        Rect box{static_cast<float>(input_.mouseX) + S(18), static_cast<float>(input_.mouseY) + S(18),
                 w + S(20), h};
        if (box.right() > static_cast<float>(gfx_.width())) {
            box.x = static_cast<float>(gfx_.width()) - box.w - S(6);
        }
        if (box.bottom() > static_cast<float>(gfx_.height())) {
            box.y = static_cast<float>(gfx_.height()) - box.h - S(6);
        }
        gfx_.panel(box, Color(10, 16, 22, 246), pal::kBorderBright);
        float y = box.y + S(6);
        if (!tipTitle_.empty()) {
            gfx_.text(box.x + S(10), y, tipTitle_, pal::kAccent, F(2));
            y += lineH(2) + S(6);
        }
        for (const std::string& l : tipLines_) {
            gfx_.text(box.x + S(10), y, l.substr(0, 74), pal::kTextDim, F(1));
            y += lineH(1) + S(3);
        }
    }

    tipTitle_.clear();
    tipLines_.clear();
    tipUnit_ = kInvalid;
    tipBuilding_ = kInvalid;
}

void App::drawPlanetTooltip() {
    if (hoverPlanet_ == kInvalid) return;
    const PlanetState& p = game_.planet(hoverPlanet_);
    const PlanetDef& pd = p.def();

    std::vector<std::string> lines;
    lines.push_back(pd.region + (pd.spaceOnly ? "  -  SPACE ONLY" : "") + "   " +
                    factionShortName(p.owner));
    for (int fi = 0; fi < kFactionCount; ++fi) {
        Faction f = factionFromIndex(fi);
        int space = static_cast<int>(game_.unitsAt(hoverPlanet_, f, Domain::Space).size());
        int ground = static_cast<int>(game_.unitsAt(hoverPlanet_, f, Domain::Ground).size());
        if (space + ground == 0) continue;
        lines.push_back(std::string(factionShortName(f)) + ": " + std::to_string(space) + " SHIPS, " +
                        std::to_string(ground) + " TROOPS");
    }
    if (p.owner != Faction::Neutral) {
        lines.push_back("INCOME " + credits(game_.planetIncome(hoverPlanet_)) + " PER WEEK");
    }
    for (Trait t : pd.traits) {
        lines.push_back(std::string(traitName(t)) + " - " + traitDescription(t));
    }
    lines.push_back("CLICK AGAIN FOR THE WORLD VIEW");
    queueTooltip(pd.name, lines);
}

// ---------------------------------------------------------------------------
// Pre-battle prompt
// ---------------------------------------------------------------------------
void App::drawBattlePrompt() {
    const PendingBattle* pb = game_.pendingPlayerBattle();
    if (pb == nullptr) return;
    const BattleSetup& s = pb->setup;
    Faction me = game_.playerFaction();

    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    gfx_.rect(Rect{0, 0, w, h}, Color(0, 0, 0, 150));

    // The alert banner, straight off the EaW galactic map.
    std::string headline = game_.planet(s.planet).def().name + ": " +
                           (s.domain == Domain::Space ? "SPACE BATTLE IMMINENT!" : "GROUND BATTLE IMMINENT!");
    Rect banner{w * 0.5f - S(340), 0, S(680), S(46)};
    gfx_.rect(banner, Color(96, 18, 22, 240));
    gfx_.rectOutline(banner, pal::kDanger, 2);
    gfx_.textCentred(banner.x + banner.w * 0.5f, banner.y + (banner.h - lineH(3)) * 0.5f, headline,
                     Color(255, 200, 200), F(3));

    ButtonStyle red;
    red.fill = Color(96, 22, 26);
    red.fillHover = Color(140, 36, 40);
    red.border = pal::kDanger;
    red.text = Color(255, 220, 220);
    red.textScale = F(3);

    Rect begin{w * 0.5f - S(340), banner.bottom() + S(6), S(230), S(40)};
    Rect autoBtn{w * 0.5f + S(110), banner.bottom() + S(6), S(230), S(40)};
    // Commander portrait between the two buttons.
    Rect portrait{w * 0.5f - S(100), banner.bottom() + S(2), S(200), S(48)};
    gfx_.rect(portrait, Color(30, 16, 18, 240));
    gfx_.rectOutline(portrait, pal::kDanger);
    gfx_.textCentred(portrait.x + portrait.w * 0.5f, portrait.y + S(8),
                     s.attacker == me ? "YOU ARE ATTACKING" : "YOU ARE DEFENDING", pal::kWarning, F(1));
    gfx_.textCentred(portrait.x + portrait.w * 0.5f, portrait.y + S(24),
                     std::string(factionShortName(s.attacker)) + " VS " + factionShortName(s.defender),
                     pal::kText, F(2));

    bool fight = button(gfx_, input_, begin, "BEGIN", true, red);
    bool resolve = button(gfx_, input_, autoBtn, "AUTO-RESOLVE", true, red);

    // Force comparison below.
    Rect box{w * 0.5f - S(340), banner.bottom() + S(56), S(680), S(300)};
    gfx_.panel(box, pal::kPanel, pal::kBorderBright);

    float atkStrength = autoresolve::forceStrength(game_, s.attackerUnits, s.domain);
    float defStrength = autoresolve::forceStrength(game_, s.defenderUnits, s.domain);
    for (Id bid : s.defenderStructures) {
        const BuildingDef& bd = game_.buildingInst(bid).def();
        defStrength += bd.defenceHp * 0.02f + bd.defenceDamage * 1.5f;
    }

    auto sideBox = [&](float x, Faction f, const std::vector<Id>& units, float strength, const char* role,
                       int structures) {
        Rect r{x, box.y + S(12), box.w * 0.5f - S(24), box.h - S(24)};
        gfx_.panel(r, pal::kPanelLight, pal::kBorder);
        gfx_.text(r.x + S(10), r.y + S(8), role, pal::kTextDim, F(1));
        gfx_.text(r.x + S(10), r.y + S(22), factionShortName(f), pal::faction(f), F(2));
        float y = r.y + S(48);
        std::vector<std::pair<Id, int>> groups;
        for (Id id : units) {
            Id defId = game_.unit(id).defId;
            bool found = false;
            for (auto& g : groups) {
                if (g.first == defId) {
                    ++g.second;
                    found = true;
                }
            }
            if (!found) groups.push_back({defId, 1});
        }
        for (const auto& g : groups) {
            if (y > r.bottom() - S(34)) break;
            Rect row{r.x + S(8), y, r.w - S(16), lineH(1) + S(3)};
            if (row.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY))) {
                tipUnit_ = g.first;
                gfx_.rect(row, Color(40, 48, 62, 180));
            }
            gfx_.text(row.x + S(2), row.y + S(1),
                      std::to_string(g.second) + "x " + db().unit(g.first).name.substr(0, 26), pal::kText,
                      F(1));
            y += lineH(1) + S(4);
        }
        if (structures > 0) {
            gfx_.text(r.x + S(10), y, std::to_string(structures) + "x DEFENCE STRUCTURES", pal::kWarning,
                      F(1));
        }
        gfx_.text(r.x + S(10), r.bottom() - S(20), "STRENGTH " + std::to_string(static_cast<int>(strength)),
                  pal::kAccent, F(1));
    };
    sideBox(box.x + S(12), s.attacker, s.attackerUnits, atkStrength, "ATTACKER", 0);
    sideBox(box.x + box.w * 0.5f + S(12), s.defender, s.defenderUnits, defStrength, "DEFENDER",
            static_cast<int>(s.defenderStructures.size()));

    float odds = atkStrength / std::max(1.0f, atkStrength + defStrength);
    gfx_.textCentred(box.x + box.w * 0.5f, box.bottom() + S(8),
                     "ESTIMATED ODDS  " + std::to_string(static_cast<int>(odds * 100.0f)) + " : " +
                         std::to_string(100 - static_cast<int>(odds * 100.0f)),
                     pal::kTextDim, F(1));

    ButtonStyle danger;
    danger.fill = Color(40, 30, 30);
    danger.fillHover = Color(70, 42, 40);
    danger.border = pal::kDanger;
    danger.textScale = F(2);
    Rect withdrawBtn{box.x + box.w * 0.5f - S(110), box.bottom() + S(26), S(220), S(32)};
    if (button(gfx_, input_, withdrawBtn, "WITHDRAW", true, danger)) {
        BattleSetup taken;
        if (game_.takePendingBattle(taken)) {
            OrderResult r = game_.withdraw(taken.planet, me);
            setStatus(r.message);
        }
    }

    if (resolve) {
        lastReport_ = game_.autoResolvePendingBattle();
        haveReport_ = true;
        screen_ = Screen::Summary;
    } else if (fight) {
        startTacticalBattle();
    }
}

}  // namespace ui
}  // namespace gc
