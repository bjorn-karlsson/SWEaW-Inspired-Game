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
// ---------------------------------------------------------------------------
namespace {

constexpr float kBarH = 208.0f;     ///< Height of the bottom command console.
constexpr float kIconStripW = 40.0f;
constexpr float kMinimapW = 214.0f;

// Console styling: EaW's HUD is a dark metal frame lit by green readouts.
const Color kConsoleFill{14, 20, 20, 246};
const Color kConsoleEdge{58, 104, 74};
const Color kConsoleInner{10, 16, 16, 240};
const Color kReadout{126, 226, 132};
const Color kReadoutDim{72, 132, 84};

const char* kCategoryNames[] = {"FLEET",  "ARMY",   "ORBIT",  "SURFACE",
                                "RESEARCH", "HEROES", "WORLD",  "HOLONET"};
constexpr int kCategoryCount = 8;

enum Category {
    CatFleet = 0,    ///< Buildable space units.
    CatArmy,         ///< Buildable ground units.
    CatOrbit,        ///< Orbital structures.
    CatSurface,      ///< Surface structures.
    CatResearch,
    CatHeroes,
    CatWorld,        ///< Planet dossier and garrison.
    CatHolonet       ///< Event log.
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

/// Two-letter badge for hero portraits and unit icons.
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
            // Wedge-shaped hull, bigger for heavier classes.
            float len = s * (c == UnitClass::Capital ? 1.0f : c == UnitClass::Cruiser ? 0.85f
                                                          : c == UnitClass::Frigate   ? 0.7f
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
            g.rect(Rect{cx - s * 0.7f + static_cast<float>(i) * s * 0.5f, cy + s * 0.72f, s * 0.34f, 3.0f},
                   col);
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
Rect App::mapViewport() const {
    return Rect{0, 0, static_cast<float>(gfx_.width()), static_cast<float>(gfx_.height()) - kBarH};
}

Vec2 App::worldToScreen(Vec2 world) const {
    Rect vp = mapViewport();
    return Vec2(vp.x + vp.w * 0.5f + (world.x - camera_.x) * zoom_,
                vp.y + vp.h * 0.5f + (world.y - camera_.y) * zoom_);
}

Vec2 App::screenToWorld(Vec2 screen) const {
    Rect vp = mapViewport();
    return Vec2(camera_.x + (screen.x - (vp.x + vp.w * 0.5f)) / zoom_,
                camera_.y + (screen.y - (vp.y + vp.h * 0.5f)) / zoom_);
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
        float radius = std::max(14.0f, 10.0f * zoom_);
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
    if (input_.keyPressed(SDLK_SPACE)) game_.togglePause();
    if (input_.keyPressed(SDLK_1)) game_.setSpeed(GameSpeed::Normal);
    if (input_.keyPressed(SDLK_2)) game_.setSpeed(GameSpeed::Fast);
    if (input_.keyPressed(SDLK_3)) game_.setSpeed(GameSpeed::Fastest);
    if (input_.keyPressed(SDLK_F1)) showHelp_ = !showHelp_;
    if (input_.keyPressed(SDLK_ESCAPE)) {
        if (showHelp_) {
            showHelp_ = false;
        } else if (!selectedUnits_.empty()) {
            selectedUnits_.clear();
        }
    }
    // Number-free hotkeys for the console categories.
    if (input_.keyPressed(SDLK_q)) category_ = CatFleet;
    if (input_.keyPressed(SDLK_e)) category_ = CatArmy;
    if (input_.keyPressed(SDLK_r)) category_ = CatResearch;
    if (input_.keyPressed(SDLK_f)) category_ = CatWorld;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float panSpeed = 460.0f * dt / zoom_;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) camera_.x -= panSpeed;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) camera_.x += panSpeed;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) camera_.y -= panSpeed;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) camera_.y += panSpeed;

    Rect vp = mapViewport();
    float mx = static_cast<float>(input_.mouseX);
    float my = static_cast<float>(input_.mouseY);
    bool overMap = vp.contains(mx, my);
    // The hero roster floats above the map, so it steals its own clicks.
    if (my < 120.0f && mx > static_cast<float>(gfx_.width()) - 360.0f) overMap = false;

    if (input_.wheel != 0 && overMap) {
        Vec2 before = screenToWorld(Vec2(mx, my));
        zoom_ *= (input_.wheel > 0) ? 1.15f : 1.0f / 1.15f;
        zoom_ = std::max(0.55f, std::min(4.5f, zoom_));
        Vec2 after = screenToWorld(Vec2(mx, my));
        camera_ += before - after;
    }

    hoverPlanet_ = overMap ? planetAtScreen(mx, my) : kInvalid;

    if (!game_.hasPendingPlayerBattle() && !showHelp_) {
        if (overMap && input_.mouseClicked && hoverPlanet_ != kInvalid) selectPlanet(hoverPlanet_);
        if (overMap && input_.rightClicked && hoverPlanet_ != kInvalid) issueMoveOrder(hoverPlanet_);
    }

    game_.update(dt);
}

// ---------------------------------------------------------------------------
// Star map
// ---------------------------------------------------------------------------
void App::drawRegionLabels() {
    // One faint nameplate per region - the "DEEP CORE" lettering EaW paints
    // across the background. Each is pushed away from the middle of the galaxy
    // so it lands in empty space, and any that still collide are dropped.
    if (zoom_ > 2.2f) return;  // too close in for background lettering

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
        int scale = zoom_ > 1.2f ? 5 : 4;
        float w = static_cast<float>(Gfx::textWidth(spaced, scale));
        float h = static_cast<float>(Gfx::textHeight(scale));
        Rect box{s.x - w * 0.5f, s.y - h * 0.5f, w, h};

        bool clash = false;
        for (const Rect& r : placed) {
            if (box.x < r.right() + 30 && box.right() + 30 > r.x && box.y < r.bottom() + 24 &&
                box.bottom() + 24 > r.y) {
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
    Rect vp = mapViewport();
    gfx_.pushClip(vp);

    // Star field, anchored to the world so panning reads correctly.
    uint32_t seed = 987654321u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    for (int i = 0; i < 900; ++i) {
        Vec2 world(rnd() * 1200.0f - 100.0f, rnd() * 1000.0f - 100.0f);
        Vec2 s = worldToScreen(world);
        if (!vp.contains(s.x, s.y)) continue;
        int v = 60 + static_cast<int>(rnd() * 160.0f);
        float size = rnd() > 0.93f ? 2.0f : 1.0f;
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
            // Contested hyperlanes glow in the colour of whoever holds the ends.
            Color c = (oa == ob && oa != Faction::Neutral) ? pal::faction(oa).scaled(0.9f)
                                                           : pal::kHyperlane;
            gfx_.thickLine(a.x, a.y, b.x, b.y, std::max(3.0f, 4.0f * zoom_ * 0.55f), c.withAlpha(90));
            gfx_.thickLine(a.x, a.y, b.x, b.y, std::max(1.5f, 2.0f * zoom_ * 0.55f), c);
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
            gfx_.thickLine(a.x, a.y, b.x, b.y, 2.5f, pal::kAccent.withAlpha(190));
            totalDays += game_.laneTravelDays(prev, step);
            prev = step;
        }
        if (!path.empty()) {
            Vec2 t = worldToScreen(game_.planet(hoverPlanet_).def().pos);
            gfx_.text(t.x + 16, t.y - 34, oneDecimal(totalDays) + " DAYS", pal::kAccent, 1);
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
        float size = 8.0f;
        Color c = pal::faction(f.owner);
        gfx_.triangle(s + dir * size, s - dir * size * 0.6f + perp * size * 0.6f,
                      s - dir * size * 0.6f - perp * size * 0.6f, c);
        gfx_.text(s.x + 10, s.y - 6, std::to_string(f.units.size()), c, 1);
    }

    // Worlds. Nameplates are placed in priority order and any that would
    // overlap an already-placed one are dropped, so the Core does not turn
    // into a wall of text.
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

    for (int i = 0; i < game_.planetCount(); ++i) {
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 s = worldToScreen(pd.pos);
        float r = std::max(5.0f, (pd.spaceOnly ? 6.0f : 9.0f) * zoom_ * 0.8f);
        if (pd.baseIncome > 250) r *= 1.3f;
        Color c = pal::faction(p.owner);

        if (i == selectedPlanet_) {
            gfx_.circleOutline(s.x, s.y, r + 9.0f, pal::kAccent);
            gfx_.circleOutline(s.x, s.y, r + 10.0f, pal::kAccent.withAlpha(110));
        }
        if (i == hoverPlanet_) gfx_.circleOutline(s.x, s.y, r + 5.0f, Color(255, 255, 255, 170));

        if (pd.spaceOnly) {
            gfx_.triangle(Vec2(s.x, s.y - r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c);
            gfx_.triangle(Vec2(s.x, s.y + r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c.scaled(0.7f));
        } else {
            // A little shaded globe rather than a flat disc.
            gfx_.circle(s.x, s.y, r, c.scaled(0.42f));
            gfx_.circle(s.x - r * 0.28f, s.y - r * 0.28f, r * 0.55f, c.scaled(0.78f));
            gfx_.circleOutline(s.x, s.y, r, c);
        }

        // Garrison pips: one row of dots per faction present, as in EaW.
        float pipY = s.y + r + 3.0f;
        for (int fi = 1; fi < kFactionCount; ++fi) {
            Faction f = factionFromIndex(fi);
            int units = static_cast<int>(game_.allUnitsAt(i, f).size());
            if (units == 0) continue;
            int pips = std::min(6, (units + 1) / 2);
            for (int k = 0; k < pips; ++k) {
                gfx_.rect(Rect{s.x - r + static_cast<float>(k) * 6.0f, pipY, 4.0f, 4.0f},
                          pal::faction(f));
            }
            pipY += 6.0f;
        }
        if (game_.isContested(i)) {
            gfx_.circleOutline(s.x, s.y, r + 13.0f, pal::kDanger);
            gfx_.text(s.x + r + 8.0f, s.y - r - 14.0f, "!", pal::kDanger, 2);
        }
        if (!p.queue.empty() && p.owner == game_.playerFaction()) {
            gfx_.rect(Rect{s.x + r + 4.0f, s.y - r - 2.0f, 4.0f, 4.0f}, pal::kWarning);
        }

    }

    // Nameplates: world name with its weekly income beneath, EaW style.
    for (const auto& entry : byPriority) {
        int i = entry.second;
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 s = worldToScreen(pd.pos);
        if (!vp.contains(s.x, s.y)) continue;
        float r = std::max(5.0f, (pd.spaceOnly ? 6.0f : 9.0f) * zoom_ * 0.8f);
        if (pd.baseIncome > 250) r *= 1.3f;

        bool showIncome = p.owner != Faction::Neutral;
        float w = static_cast<float>(Gfx::textWidth(pd.name, 2));
        Rect plate{s.x - w * 0.5f - 4, s.y - r - 22.0f, w + 8, showIncome ? 26.0f : 14.0f};

        bool clash = false;
        for (const Rect& other : nameplates) {
            if (plate.x < other.right() + 2 && plate.right() + 2 > other.x &&
                plate.y < other.bottom() + 1 && plate.bottom() + 1 > other.y) {
                clash = true;
                break;
            }
        }
        // Whatever you are looking at is always labelled.
        if (clash && i != selectedPlanet_ && i != hoverPlanet_) continue;
        nameplates.push_back(plate);

        Color c = pal::faction(p.owner);
        gfx_.textCentred(s.x, plate.y, pd.name, c, 2);
        if (showIncome) {
            gfx_.textCentred(s.x, plate.y + 13.0f, "+" + std::to_string(game_.planetIncome(i)),
                             c.scaled(0.85f), 1);
        }
    }

    gfx_.popClip();

    drawHeroRoster();
    drawCommandBar();
    drawPlanetTooltip();
    drawPausedBanner();

    if (statusTimer_ > 0.0f) {
        Rect r{16, vp.bottom() - 40, static_cast<float>(Gfx::textWidth(status_, 2)) + 24, 28};
        gfx_.panel(r, kConsoleFill, kConsoleEdge);
        gfx_.text(r.x + 12, r.y + 8, status_, kReadout, 2);
    }

    if (showHelp_) {
        Rect r{vp.x + vp.w * 0.5f - 310, vp.y + 90, 620, 340};
        gfx_.panel(r, kConsoleFill, pal::kBorderBright);
        gfx_.textCentred(r.x + r.w * 0.5f, r.y + 14, "CONTROLS", pal::kAccent, 3);
        const char* lines[] = {
            "LEFT CLICK PLANET      select a world",
            "RIGHT CLICK PLANET     send the selected units there",
            "MOUSE WHEEL            zoom      WASD/ARROWS  pan",
            "SPACE                  pause / resume     1 2 3  speed",
            "Q E R F                fleet / army / research / world panels",
            "F1                     this help          ESC  clear selection",
            "",
            "RULES OF CONQUEST",
            "- Destroy every enemy ship and orbital gun before landing.",
            "- A world only changes hands when your ground troops hold it.",
            "- Space-only systems are taken by holding orbit alone.",
            "- Income arrives once a week; besieged worlds pay nothing.",
        };
        float y = r.y + 56;
        for (const char* line : lines) {
            gfx_.text(r.x + 20, y, line, pal::kText, 1);
            y += 20;
        }
        Rect close{r.x + r.w * 0.5f - 70, r.bottom() - 40, 140, 28};
        if (button(gfx_, input_, close, "CLOSE")) showHelp_ = false;
    }

    if (game_.hasPendingPlayerBattle()) drawBattlePrompt();
    drawQueuedTooltip();
}

// ---------------------------------------------------------------------------
// Paused banner (top centre, like EaW's)
// ---------------------------------------------------------------------------
void App::drawPausedBanner() {
    if (!game_.paused() || game_.hasPendingPlayerBattle()) return;
    const float w = static_cast<float>(gfx_.width());
    Rect banner{w * 0.5f - 280, 0, 560, 46};
    gfx_.rect(banner, Color(78, 16, 20, 230));
    gfx_.rectOutline(banner, pal::kDanger, 2);
    gfx_.textCentred(banner.x + banner.w * 0.5f, banner.y + 14, "GAME PAUSED", Color(255, 190, 190), 3);

    Rect resume{w * 0.5f - 150, banner.bottom() + 6, 300, 36};
    ButtonStyle s;
    s.fill = Color(96, 22, 26);
    s.fillHover = Color(132, 34, 38);
    s.border = pal::kDanger;
    s.text = Color(255, 214, 214);
    s.textScale = 3;
    if (button(gfx_, input_, resume, "RESUME GAME", true, s)) game_.setSpeed(GameSpeed::Normal);
}

// ---------------------------------------------------------------------------
// Hero roster (top right)
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

    const float r = 24.0f;
    const float pitch = 52.0f;
    const int perRow = 7;
    float right = static_cast<float>(gfx_.width()) - 16.0f;

    for (size_t i = 0; i < portraits.size() && i < 14; ++i) {
        int row = static_cast<int>(i) / perRow;
        int col = static_cast<int>(i) % perRow;
        int rowCount = std::min(perRow, static_cast<int>(portraits.size()) - row * perRow);
        float x = right - static_cast<float>(rowCount - col) * pitch + pitch * 0.5f;
        float y = 32.0f + static_cast<float>(row) * pitch;
        const Portrait& p = portraits[i];
        const UnitDef& d = db().unit(p.defId);
        bool available = p.unitId != kInvalid;
        Color ring = available ? pal::faction(me) : Color(90, 90, 96);

        Rect hit{x - r, y - r, r * 2, r * 2};
        bool hover = hit.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));

        gfx_.circle(x, y, r, Color(18, 24, 34, 235));
        gfx_.circle(x, y, r - 3.0f, available ? Color(34, 46, 64) : Color(26, 28, 32));
        gfx_.circleOutline(x, y, r, hover ? pal::kAccent : ring);
        gfx_.circleOutline(x, y, r - 1.0f, (hover ? pal::kAccent : ring).withAlpha(120));
        gfx_.textCentred(x, y - 7.0f, initials(d.name), available ? pal::kText : Color(120, 120, 128), 2);

        if (!available) {
            gfx_.textCentred(x, y + 10.0f, std::to_string(p.respawn) + "D", pal::kWarning, 1);
        } else if (p.planet != kInvalid && game_.isContested(p.planet)) {
            gfx_.circleOutline(x, y, r + 3.0f, pal::kDanger);
        }

        if (hover) {
            std::vector<std::string> lines;
            lines.push_back(unitClassName(d.unitClass));
            if (available) {
                lines.push_back(p.planet != kInvalid ? "AT " + game_.planet(p.planet).def().name
                                                     : "IN TRANSIT");
            } else {
                lines.push_back("RETURNS IN " + std::to_string(p.respawn) + " DAYS");
            }
            if (d.heroCombatBonus > 0.0f) {
                lines.push_back("+" + std::to_string(static_cast<int>(d.heroCombatBonus * 100.0f)) +
                                "% COMBAT STRENGTH");
            }
            if (d.heroIncomeBonus > 0) {
                lines.push_back("+" + std::to_string(d.heroIncomeBonus) + " CREDITS PER WEEK");
            }
            lines.push_back(d.description);
            queueTooltip(d.name, lines);
            if (input_.mouseClicked && available && p.planet != kInvalid) {
                selectPlanet(p.planet);
                focusOn(p.planet);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The command console
// ---------------------------------------------------------------------------
void App::drawCommandBar() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    Rect bar{0, h - kBarH, w, kBarH};

    // Frame.
    gfx_.rect(bar, kConsoleFill);
    gfx_.rect(Rect{0, bar.y, w, 3}, kConsoleEdge);
    gfx_.rect(Rect{0, bar.y + 3, w, 1}, Color(96, 168, 118, 120));

    // --- far left: system buttons ---
    float iy = bar.y + 10;
    struct StripButton {
        const char* label;
        const char* tip;
    };
    const StripButton strip[] = {{"?", "Controls and rules (F1)"},
                                 {"H", "Holonet reports"},
                                 {"W", "World dossier"},
                                 {"X", "Main menu"}};
    for (int i = 0; i < 4; ++i) {
        Rect r{6, iy, 32, 32};
        if (button(gfx_, input_, r, strip[i].label)) {
            switch (i) {
                case 0: showHelp_ = !showHelp_; break;
                case 1: category_ = CatHolonet; break;
                case 2: category_ = CatWorld; break;
                case 3: screen_ = Screen::Menu; break;
            }
        }
        if (r.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY))) {
            queueTooltip(strip[i].tip, {});
        }
        iy += 38;
    }

    // --- minimap and time controls ---
    Rect mini{kIconStripW + 6, bar.y + 10, kMinimapW, 148};
    drawMinimap(mini);

    Rect speeds{mini.x, mini.bottom() + 6, mini.w, 32};
    struct SpeedButton {
        const char* label;
        GameSpeed speed;
    };
    const SpeedButton sb[] = {{"||", GameSpeed::Paused},
                              {"1X", GameSpeed::Normal},
                              {"2X", GameSpeed::Fast},
                              {"4X", GameSpeed::Fastest}};
    float bw = speeds.w / 4.0f - 3.0f;
    for (int i = 0; i < 4; ++i) {
        Rect r{speeds.x + static_cast<float>(i) * (bw + 4.0f), speeds.y, bw, speeds.h};
        if (toggleButton(gfx_, input_, r, sb[i].label, game_.speed() == sb[i].speed)) {
            game_.setSpeed(sb[i].speed);
        }
    }

    // --- centre column ---
    const float centreX = mini.right() + 10.0f;
    const float rightBlockW = 250.0f;
    const float centreW = w - centreX - rightBlockW - 10.0f;

    // Row 1: production readout, category grid, research readout.
    Rect prod{centreX, bar.y + 10, centreW * 0.34f, 58};
    Rect grid{prod.right() + 8, bar.y + 8, 268, 62};
    Rect research{grid.right() + 8, bar.y + 10, centreX + centreW - (grid.right() + 8), 58};

    // Production readout for the selected world.
    gfx_.panel(prod, kConsoleInner, kConsoleEdge);
    gfx_.text(prod.x + 8, prod.y + 6, "PRODUCTION", kReadoutDim, 1);
    if (selectedPlanet_ != kInvalid && !game_.planet(selectedPlanet_).queue.empty()) {
        const PlanetState& p = game_.planet(selectedPlanet_);
        const BuildOrder& o = p.queue.front();
        std::string name = o.kind == BuildKind::Unit ? db().unit(o.defId).name : db().building(o.defId).name;
        gfx_.text(prod.x + 8, prod.y + 20, name.substr(0, 30), kReadout, 1);
        float frac = o.totalDays > 0 ? 1.0f - o.daysRemaining / o.totalDays : 0.0f;
        progressBar(gfx_, Rect{prod.x + 8, prod.y + 36, prod.w - 60, 12}, frac, kReadout,
                    Color(10, 20, 16));
        gfx_.textRight(prod.right() - 8, prod.y + 37, std::to_string(p.queue.size()) + " QUEUED",
                       kReadoutDim, 1);
        if (p.owner == game_.playerFaction()) {
            Rect cancel{prod.right() - 30, prod.y + 18, 24, 14};
            if (button(gfx_, input_, cancel, "X")) {
                setStatus(game_.cancelBuildOrder(selectedPlanet_, 0, game_.playerFaction()).message);
            }
        }
    } else {
        gfx_.text(prod.x + 8, prod.y + 26, "NO ORDERS", kReadoutDim, 2);
    }

    drawCategoryGrid(grid);

    // Research readout.
    gfx_.panel(research, kConsoleInner, kConsoleEdge);
    gfx_.text(research.x + 8, research.y + 6, "RESEARCH", kReadoutDim, 1);
    const FactionState& fs = game_.faction(game_.playerFaction());
    if (!fs.research.empty()) {
        const ResearchOrder& ro = fs.research.front();
        gfx_.text(research.x + 8, research.y + 20, db().tech(ro.techId).name.substr(0, 30), kReadout, 1);
        progressBar(gfx_, Rect{research.x + 8, research.y + 36, research.w - 16, 12},
                    ro.totalDays > 0 ? 1.0f - ro.daysRemaining / ro.totalDays : 0.0f, kReadout,
                    Color(10, 20, 16));
    } else {
        gfx_.text(research.x + 8, research.y + 26, "IDLE", kReadoutDim, 2);
    }

    // Row 2: status line.
    Rect status{centreX, bar.y + 74, centreW, 22};
    drawStatusLine(status);

    // Row 3: the item tray.
    Rect tray{centreX, bar.y + 100, centreW, kBarH - 108};
    drawTray(tray);

    // --- right: action cluster ---
    Rect actions{w - rightBlockW, bar.y + 8, rightBlockW - 8, kBarH - 16};
    drawActionCluster(actions);
}

void App::drawCategoryGrid(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    float bw = (area.w - 10) / 4.0f;
    float bh = (area.h - 8) / 2.0f;
    for (int i = 0; i < kCategoryCount; ++i) {
        int row = i / 4;
        int col = i % 4;
        Rect r{area.x + 4 + static_cast<float>(col) * bw, area.y + 4 + static_cast<float>(row) * bh,
               bw - 2, bh - 2};
        bool enabled = true;
        if (selectedPlanet_ != kInvalid && game_.planet(selectedPlanet_).def().spaceOnly &&
            (i == CatArmy || i == CatSurface)) {
            enabled = false;
        }
        if (toggleButton(gfx_, input_, r, kCategoryNames[i], category_ == i, enabled)) {
            category_ = i;
            trayScroll_ = 0.0f;
        }
    }
}

void App::drawStatusLine(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    Faction me = game_.playerFaction();
    float x = area.x + 8;

    if (selectedPlanet_ != kInvalid) {
        const PlanetState& p = game_.planet(selectedPlanet_);
        gfx_.text(x, area.y + 6, p.def().name, pal::faction(p.owner), 2);
        x += static_cast<float>(Gfx::textWidth(p.def().name, 2)) + 16;
        gfx_.text(x, area.y + 7, "+" + std::to_string(game_.planetIncome(selectedPlanet_)), kReadout, 1);
        x += 60;
        int su = game_.usedUnitSlots(selectedPlanet_, p.owner, Domain::Space);
        int sc = game_.unitSlotCapacity(selectedPlanet_, Domain::Space);
        int gu = game_.usedUnitSlots(selectedPlanet_, p.owner, Domain::Ground);
        int gc = game_.unitSlotCapacity(selectedPlanet_, Domain::Ground);
        gfx_.text(x, area.y + 7,
                  "ORBIT " + std::to_string(su) + "/" + std::to_string(sc) + "   SURFACE " +
                      std::to_string(gu) + "/" + std::to_string(gc),
                  kReadoutDim, 1);
        x += 200;
        if (game_.isContested(selectedPlanet_)) {
            gfx_.text(x, area.y + 7, "UNDER SIEGE", pal::kDanger, 1);
        }
    }

    // Week counter, treasury and income on the right, EaW style.
    float rx = area.right() - 8;
    gfx_.textRight(rx, area.y + 6, credits(game_.faction(me).credits), pal::kWarning, 2);
    rx -= static_cast<float>(Gfx::textWidth(credits(game_.faction(me).credits), 2)) + 18;
    gfx_.textRight(rx, area.y + 7, "+" + credits(game_.factionIncome(me)) + "/WK", kReadout, 1);
    rx -= 120;
    Rect weekBar{rx - 90, area.y + 6, 84, 11};
    float weekFrac = (static_cast<float>(game_.date().dayOfWeek()) + game_.dayFraction()) /
                     static_cast<float>(kDaysPerWeek);
    progressBar(gfx_, weekBar, weekFrac, kReadout, Color(10, 20, 16));
    rx -= 100;
    gfx_.textRight(rx, area.y + 6, "WEEK " + std::to_string(game_.date().week() + 1), pal::kText, 2);
    rx -= static_cast<float>(Gfx::textWidth("WEEK " + std::to_string(game_.date().week() + 1), 2)) + 18;
    std::string standings;
    for (Faction f : playableFactions()) {
        if (game_.faction(f).defeated) continue;
        standings += std::string(factionShortName(f)) + " " + std::to_string(game_.planetsOwned(f)) + "  ";
    }
    gfx_.textRight(rx, area.y + 7, standings, kReadoutDim, 1);
}

// ---------------------------------------------------------------------------
// The tray: build items, forces, research, heroes, dossier, holonet
// ---------------------------------------------------------------------------
void App::drawTray(const Rect& area) {
    gfx_.panel(area, kConsoleInner, kConsoleEdge);
    Faction me = game_.playerFaction();
    gfx_.pushClip(area.inset(2));

    float mx = static_cast<float>(input_.mouseX);
    float my = static_cast<float>(input_.mouseY);
    bool overTray = area.contains(mx, my);

    // Text panes (research, dossier, holonet, heroes) scroll vertically; the
    // card strips scroll horizontally.
    auto scrollAxis = [&](bool horizontal, float contentSize) {
        if (!overTray || input_.wheel == 0) return;
        trayScroll_ -= static_cast<float>(input_.wheel) * (horizontal ? 64.0f : 34.0f);
        float visible = horizontal ? area.w : area.h;
        trayScroll_ = std::max(0.0f, std::min(std::max(0.0f, contentSize - visible + 12.0f), trayScroll_));
    };

    if (selectedPlanet_ == kInvalid) {
        gfx_.text(area.x + 10, area.y + 10, "SELECT A WORLD ON THE STAR MAP", kReadoutDim, 2);
        gfx_.popClip();
        return;
    }

    const PlanetState& p = game_.planet(selectedPlanet_);
    const PlanetDef& pd = p.def();
    bool mine = p.owner == me;

    // ---- card strips: buildable units and structures ----
    if (category_ == CatFleet || category_ == CatArmy || category_ == CatOrbit ||
        category_ == CatSurface) {
        if (!mine) {
            gfx_.text(area.x + 10, area.y + 10, "YOU DO NOT CONTROL THIS WORLD", kReadoutDim, 2);
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

        const float cardW = 104.0f;
        const float cardH = area.h - 8.0f;
        scrollAxis(true, static_cast<float>(ids.size()) * (cardW + 6.0f));

        if (ids.empty() || (units && game_.bestProductionTier(selectedPlanet_, me, domain) == 0)) {
            std::string msg = units ? (domain == Domain::Space
                                           ? "BUILD AN ORBITAL STATION BEFORE LAYING DOWN SHIPS"
                                           : "BUILD A BARRACKS OR FACTORY BEFORE TRAINING TROOPS")
                                    : "NOTHING AVAILABLE HERE";
            gfx_.text(area.x + 10, area.y + 10, msg, kReadoutDim, 1);
            gfx_.popClip();
            return;
        }

        float x = area.x + 4 - trayScroll_;
        for (Id id : ids) {
            Rect card{x, area.y + 4, cardW, cardH};
            x += cardW + 6.0f;
            if (card.right() < area.x || card.x > area.right()) continue;

            OrderResult can = units ? game_.canQueueUnit(selectedPlanet_, id, me)
                                    : game_.canQueueBuilding(selectedPlanet_, id, me);
            bool hover = card.contains(mx, my) && overTray;
            gfx_.rect(card, hover ? Color(26, 44, 34, 235) : Color(16, 26, 24, 220));
            gfx_.rectOutline(card, can.ok ? (hover ? pal::kAccent : kConsoleEdge) : Color(78, 44, 44));

            Rect icon{card.x + 4, card.y + 4, card.w - 8, 40};
            gfx_.rect(icon, Color(8, 14, 14, 200));
            Color glyphColour = can.ok ? pal::faction(me) : Color(96, 96, 100);
            std::string name;
            int cost;
            float days;
            if (units) {
                const UnitDef& u = db().unit(id);
                drawUnitGlyph(gfx_, icon, u.unitClass, glyphColour);
                gfx_.text(icon.x + 3, icon.y + 3, classTag(u.unitClass), kReadoutDim, 1);
                if (u.isHero) gfx_.textRight(icon.right() - 3, icon.y + 3, "*", pal::kWarning, 1);
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

            // Two short lines of name, broken on a space where possible.
            std::string line1 = name;
            std::string line2;
            if (name.size() > 17) {
                size_t cut = name.rfind(' ', 17);
                if (cut == std::string::npos || cut < 6) cut = 17;
                line1 = name.substr(0, cut);
                size_t rest = (cut < name.size() && name[cut] == ' ') ? cut + 1 : cut;
                line2 = name.substr(rest, 17);
            }
            gfx_.text(card.x + 5, card.y + 48, line1, can.ok ? pal::kText : Color(120, 110, 110), 1);
            gfx_.text(card.x + 5, card.y + 60, line2, can.ok ? pal::kText : Color(120, 110, 110), 1);
            gfx_.text(card.x + 5, card.bottom() - 14, credits(cost), pal::kWarning, 2);
            gfx_.textRight(card.right() - 5, card.bottom() - 12, oneDecimal(days) + "D", kReadoutDim, 1);

            if (hover) {
                std::vector<std::string> lines;
                if (units) {
                    const UnitDef& u = db().unit(id);
                    lines.push_back(std::string(unitClassName(u.unitClass)) + "   " +
                                    std::to_string(u.popCost) + " SLOTS");
                    lines.push_back("HULL " + std::to_string(static_cast<int>(u.hull)) +
                                    (u.shield > 0 ? "   SHIELD " + std::to_string(static_cast<int>(u.shield))
                                                  : ""));
                    lines.push_back("DAMAGE " + std::to_string(static_cast<int>(u.damageAntiCapital)) +
                                    " vs CAPITAL / " + std::to_string(static_cast<int>(u.damageAntiFighter)) +
                                    " vs SQUADRONS");
                    if (!u.wings.empty()) {
                        std::string wings = "CARRIES ";
                        for (const CarriedWing& wg : u.wings) {
                            wings += std::to_string(wg.count) + "x " + db().unit(wg.unitId).name + "  ";
                        }
                        lines.push_back(wings);
                    }
                    lines.push_back(u.description);
                } else {
                    const BuildingDef& b = db().building(id);
                    lines.push_back(b.domain == Domain::Space ? "ORBITAL STRUCTURE" : "SURFACE STRUCTURE");
                    if (b.productionTier > 0) {
                        lines.push_back("UNLOCKS PRODUCTION TIER " + std::to_string(b.productionTier));
                    }
                    if (b.incomeFlat > 0 || b.incomeMult > 0.0f) {
                        lines.push_back("INCOME +" + std::to_string(b.incomeFlat) + " AND +" +
                                        std::to_string(static_cast<int>(b.incomeMult * 100.0f)) + "%");
                    }
                    if (b.defenceHp > 0.0f) {
                        lines.push_back("DEFENCE " + std::to_string(static_cast<int>(b.defenceHp)) +
                                        " HP, " + std::to_string(static_cast<int>(b.defenceDamage)) +
                                        " DAMAGE");
                    }
                    if (b.unitSlotBonus > 0) {
                        lines.push_back("+" + std::to_string(b.unitSlotBonus) + " UNIT SLOTS");
                    }
                    lines.push_back(b.description);
                }
                if (!can.ok) lines.push_back("UNAVAILABLE: " + can.message);
                queueTooltip(name, lines);
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
        scrollAxis(false, static_cast<float>(options.size()) * 30.0f);
        float y = area.y + 6 - trayScroll_;
        if (options.empty()) {
            gfx_.text(area.x + 10, area.y + 10, "ALL TECHNOLOGY RESEARCHED", kReadout, 2);
        }
        for (Id tid : options) {
            const TechDef& t = db().tech(tid);
            Rect row{area.x + 4, y, area.w - 8, 28};
            y += 30;
            if (row.bottom() < area.y || row.y > area.bottom()) continue;
            bool hover = row.contains(mx, my) && overTray;
            int cost = game_.techCost(me, tid);
            bool afford = game_.faction(me).credits >= cost && game_.faction(me).research.empty();
            gfx_.rect(row, hover ? Color(26, 44, 34, 230) : Color(16, 26, 24, 200));
            gfx_.text(row.x + 8, row.y + 3, "T" + std::to_string(t.tier) + "  " + t.name, pal::kText, 1);
            gfx_.text(row.x + 8, row.y + 15, t.unlocksText.substr(0, 74), kReadoutDim, 1);
            gfx_.textRight(row.right() - 120, row.y + 9, credits(cost) + " CR", pal::kWarning, 1);
            gfx_.textRight(row.right() - 70, row.y + 9, oneDecimal(game_.techDays(me, tid)) + "D",
                           kReadoutDim, 1);
            Rect go{row.right() - 62, row.y + 4, 56, 20};
            if (button(gfx_, input_, go, "RESEARCH", afford)) {
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
        float x = area.x + 6;
        gfx_.text(x, area.y + 6, "RECRUIT AT " + pd.name, kReadoutDim, 1);
        float y = area.y + 22;
        for (Id uid : recruitable) {
            const UnitDef& u = db().unit(uid);
            OrderResult can = game_.canQueueUnit(selectedPlanet_, uid, me);
            Rect row{area.x + 4, y, area.w * 0.5f - 8, 24};
            y += 26;
            if (row.bottom() > area.bottom()) break;
            bool hover = row.contains(mx, my) && overTray;
            gfx_.rect(row, hover ? Color(26, 44, 34, 230) : Color(16, 26, 24, 200));
            gfx_.circle(row.x + 14, row.y + 12, 9, Color(34, 46, 64));
            gfx_.circleOutline(row.x + 14, row.y + 12, 9, pal::faction(me));
            gfx_.textCentred(row.x + 14, row.y + 8, initials(u.name), pal::kText, 1);
            gfx_.text(row.x + 30, row.y + 8, u.name.substr(0, 26), can.ok ? pal::kText : kReadoutDim, 1);
            gfx_.textRight(row.right() - 56, row.y + 8, credits(game_.unitCost(selectedPlanet_, uid)),
                           pal::kWarning, 1);
            Rect go{row.right() - 50, row.y + 3, 46, 18};
            if (button(gfx_, input_, go, "HIRE", can.ok)) {
                setStatus(game_.queueUnit(selectedPlanet_, uid, me).message);
            }
            if (hover && !can.ok) queueTooltip(u.name, {can.message, u.description});
            else if (hover) queueTooltip(u.name, {u.description});
        }
        if (recruitable.empty()) {
            gfx_.text(area.x + 10, area.y + 26, mine ? "NO COMMANDERS AVAILABLE HERE" : "-", kReadoutDim, 1);
        }

        // Right half: the commanders already in the field.
        float rx = area.x + area.w * 0.5f + 8;
        gfx_.text(rx, area.y + 6, "IN THE FIELD", kReadoutDim, 1);
        float ry = area.y + 22;
        for (const UnitInstance& u : game_.units()) {
            if (!u.alive || u.owner != me || !u.def().isHero) continue;
            if (ry + 16 > area.bottom()) break;
            gfx_.text(rx, ry, u.def().name.substr(0, 26), pal::kText, 1);
            gfx_.textRight(area.right() - 8, ry,
                           u.planet != kInvalid ? game_.planet(u.planet).def().name : "IN TRANSIT",
                           kReadoutDim, 1);
            ry += 15;
        }
        gfx_.popClip();
        return;
    }

    // ---- world dossier and garrison ----
    if (category_ == CatWorld) {
        float y = area.y + 6;
        gfx_.text(area.x + 8, y, pd.name + "  -  " + pd.region, pal::faction(p.owner), 2);
        gfx_.textRight(area.right() - 8, y + 2,
                       pd.spaceOnly ? "SPACE-ONLY SYSTEM" : "OWNER: " + std::string(factionShortName(p.owner)),
                       pd.spaceOnly ? pal::kWarning : kReadoutDim, 1);
        y += 20;
        for (Trait t : pd.traits) {
            gfx_.text(area.x + 8, y, std::string("+ ") + traitName(t), pal::kAccent, 1);
            gfx_.text(area.x + 160, y, traitDescription(t), kReadoutDim, 1);
            y += 13;
        }
        if (!p.buildings.empty()) {
            std::string list = "BUILT: ";
            for (Id bid : p.buildings) {
                const BuildingInstance& b = game_.buildingInst(bid);
                if (b.alive) list += b.def().name + ", ";
            }
            gfx_.text(area.x + 8, y, list.substr(0, 96), kReadoutDim, 1);
            y += 14;
        }

        // Garrison, clickable for selection.
        float listY = y + 2;
        float colW = area.w * 0.5f - 12;
        int column = 0;
        for (Id id : p.units) {
            const UnitInstance& u = game_.unit(id);
            if (!u.alive) continue;
            if (listY + 14 > area.bottom()) {
                ++column;
                listY = y + 2;
                if (column > 1) break;
            }
            Rect row{area.x + 8 + static_cast<float>(column) * (colW + 8), listY, colW, 13};
            listY += 14;
            bool isMine = u.owner == me;
            bool selected =
                std::find(selectedUnits_.begin(), selectedUnits_.end(), id) != selectedUnits_.end();
            bool hover = isMine && row.contains(mx, my) && overTray;
            if (selected) gfx_.rect(row, Color(34, 74, 52, 220));
            else if (hover) gfx_.rect(row, Color(24, 40, 32, 200));
            gfx_.text(row.x + 2, row.y + 2, classTag(u.def().unitClass), pal::faction(u.owner), 1);
            gfx_.text(row.x + 28, row.y + 2, u.def().name.substr(0, 24),
                      isMine ? pal::kText : kReadoutDim, 1);
            if (u.def().domain() == Domain::Ground) {
                gfx_.text(row.right() - 96, row.y + 2, u.landed ? "SURFACE" : "ORBIT",
                          u.landed ? kReadout : pal::kWarning, 1);
            }
            progressBar(gfx_, Rect{row.right() - 44, row.y + 3, 40, 7}, u.health,
                        u.health > 0.6f ? kReadout : (u.health > 0.3f ? pal::kWarning : pal::kDanger),
                        Color(10, 18, 16));
            if (hover && input_.mouseClicked) toggleUnitSelection(id);
        }
        gfx_.popClip();
        return;
    }

    // ---- holonet ----
    if (category_ == CatHolonet) {
        const std::deque<GameEvent>& events = game_.events();
        float y = area.y + 6;
        int shown = 0;
        for (auto it = events.rbegin(); it != events.rend() && y < area.bottom() - 12; ++it) {
            Color c = it->faction == Faction::Neutral ? kReadoutDim : pal::faction(it->faction);
            gfx_.text(area.x + 8, y, "D" + std::to_string(it->day) + "  " + it->text.substr(0, 110), c, 1);
            y += 13;
            ++shown;
        }
        if (shown == 0) gfx_.text(area.x + 8, y, "NO REPORTS YET", kReadoutDim, 1);
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

    gfx_.text(area.x + 8, area.y + 6, "SELECTED " + std::to_string(selectedUnits_.size()), kReadoutDim, 1);

    Rect allShips{area.x + 8, area.y + 22, area.w * 0.5f - 12, 26};
    Rect allTroops{area.x + area.w * 0.5f, area.y + 22, area.w * 0.5f - 12, 26};
    if (button(gfx_, input_, allShips, "ALL SHIPS", selectedPlanet_ != kInvalid)) {
        selectAllAt(selectedPlanet_, Domain::Space);
        category_ = CatWorld;
    }
    if (button(gfx_, input_, allTroops, "ALL TROOPS", selectedPlanet_ != kInvalid)) {
        selectAllAt(selectedPlanet_, Domain::Ground);
        category_ = CatWorld;
    }

    // Withdraw from a contested world.
    bool canWithdraw = selectedPlanet_ != kInvalid && game_.isContested(selectedPlanet_) &&
                       !game_.allUnitsAt(selectedPlanet_, me).empty();
    Rect withdraw{area.x + 8, area.y + 52, area.w - 16, 24};
    ButtonStyle danger;
    danger.fill = Color(62, 30, 30);
    danger.fillHover = Color(96, 42, 40);
    danger.border = pal::kDanger;
    if (button(gfx_, input_, withdraw, "WITHDRAW", canWithdraw, danger)) {
        setStatus(game_.withdraw(selectedPlanet_, me).message);
        selectedUnits_.clear();
    }

    // The big contextual button: land the troops you have in orbit.
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
    float cy = area.bottom() - 54;
    float r = 44.0f;
    bool hover = distance(Vec2(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)),
                          Vec2(cx, cy)) < r;
    Color ring = canLand ? (hostile ? pal::kDanger : pal::kAccent) : Color(70, 74, 80);
    gfx_.circle(cx, cy, r, canLand ? (hover ? Color(52, 30, 30) : Color(30, 40, 40)) : Color(20, 24, 28));
    gfx_.circleOutline(cx, cy, r, ring);
    gfx_.circleOutline(cx, cy, r - 3.0f, ring.withAlpha(120));
    gfx_.textCentred(cx, cy - 14, hostile ? "INVADE" : "DEPLOY", canLand ? pal::kText : Color(110, 114, 120),
                     2);
    gfx_.textCentred(cx, cy + 2, std::to_string(landing.size()) + " UNITS",
                     canLand ? kReadoutDim : Color(90, 94, 100), 1);
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
// Minimap
// ---------------------------------------------------------------------------
void App::drawMinimap(const Rect& area) {
    gfx_.panel(area, Color(8, 14, 14, 240), kConsoleEdge);

    Vec2 lo(1e9f, 1e9f), hi(-1e9f, -1e9f);
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = game_.planet(i).def().pos;
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
    }
    Rect inner = area.inset(6);
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
        float r = game_.isContested(i) ? 3.0f : 2.0f;
        gfx_.rect(Rect{p.x - r, p.y - r, r * 2, r * 2}, c);
        if (i == selectedPlanet_) gfx_.rectOutline(Rect{p.x - 4, p.y - 4, 8, 8}, pal::kAccent);
    }

    Vec2 topLeft = screenToWorld(Vec2(0, 0));
    Vec2 bottomRight = screenToWorld(Vec2(mapViewport().right(), mapViewport().bottom()));
    Vec2 a = toMini(topLeft);
    Vec2 b = toMini(bottomRight);
    gfx_.rectOutline(Rect{a.x, a.y, b.x - a.x, b.y - a.y}, Color(255, 255, 255, 120));

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
// Tooltips
// ---------------------------------------------------------------------------
void App::queueTooltip(const std::string& title, const std::vector<std::string>& lines) {
    tipTitle_ = title;
    tipLines_ = lines;
}

void App::drawQueuedTooltip() {
    if (tipTitle_.empty()) return;
    float w = static_cast<float>(Gfx::textWidth(tipTitle_, 2));
    for (const std::string& l : tipLines_) {
        w = std::max(w, static_cast<float>(Gfx::textWidth(l.substr(0, 74), 1)));
    }
    float h = 26.0f + static_cast<float>(tipLines_.size()) * 14.0f;
    Rect box{static_cast<float>(input_.mouseX) + 18, static_cast<float>(input_.mouseY) - h - 10, w + 20,
             h + 10};
    if (box.right() > static_cast<float>(gfx_.width())) box.x = static_cast<float>(gfx_.width()) - box.w - 6;
    if (box.y < 0) box.y = static_cast<float>(input_.mouseY) + 20;
    gfx_.panel(box, Color(10, 16, 22, 246), pal::kBorderBright);
    gfx_.text(box.x + 10, box.y + 8, tipTitle_, pal::kAccent, 2);
    float y = box.y + 28;
    for (const std::string& l : tipLines_) {
        gfx_.text(box.x + 10, y, l.substr(0, 74), pal::kTextDim, 1);
        y += 14;
    }
    tipTitle_.clear();
    tipLines_.clear();
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

    Rect box{w * 0.5f - 320, h * 0.5f - 200, 640, 380};
    gfx_.panel(box, pal::kPanel, pal::kBorderBright);
    gfx_.textCentred(box.x + box.w * 0.5f, box.y + 16,
                     std::string(s.domain == Domain::Space ? "SPACE BATTLE" : "GROUND BATTLE"),
                     pal::kDanger, 3);
    gfx_.textCentred(box.x + box.w * 0.5f, box.y + 48, "OVER " + game_.planet(s.planet).def().name,
                     pal::kText, 2);
    gfx_.textCentred(box.x + box.w * 0.5f, box.y + 72,
                     s.attacker == me ? "YOU ARE ATTACKING" : "YOU ARE DEFENDING",
                     s.attacker == me ? pal::kWarning : pal::kAccent, 1);

    float atkStrength = autoresolve::forceStrength(game_, s.attackerUnits, s.domain);
    float defStrength = autoresolve::forceStrength(game_, s.defenderUnits, s.domain);
    for (Id bid : s.defenderStructures) {
        const BuildingDef& bd = game_.buildingInst(bid).def();
        defStrength += bd.defenceHp * 0.02f + bd.defenceDamage * 1.5f;
    }

    auto sideBox = [&](float x, Faction f, const std::vector<Id>& units, float strength,
                       const char* role, int structures) {
        Rect r{x, box.y + 96, box.w * 0.5f - 30, 190};
        gfx_.panel(r, pal::kPanelLight, pal::kBorder);
        gfx_.text(r.x + 10, r.y + 8, role, pal::kTextDim, 1);
        gfx_.text(r.x + 10, r.y + 22, factionShortName(f), pal::faction(f), 2);
        float y = r.y + 48;
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
            if (y > r.bottom() - 34) break;
            gfx_.text(r.x + 10, y, std::to_string(g.second) + "x " + db().unit(g.first).name.substr(0, 24),
                      pal::kText, 1);
            y += 14;
        }
        if (structures > 0) {
            gfx_.text(r.x + 10, y, std::to_string(structures) + "x DEFENCE STRUCTURES", pal::kWarning, 1);
        }
        gfx_.text(r.x + 10, r.bottom() - 20, "STRENGTH " + std::to_string(static_cast<int>(strength)),
                  pal::kAccent, 1);
    };
    sideBox(box.x + 20, s.attacker, s.attackerUnits, atkStrength, "ATTACKER", 0);
    sideBox(box.x + box.w * 0.5f + 10, s.defender, s.defenderUnits, defStrength, "DEFENDER",
            static_cast<int>(s.defenderStructures.size()));

    float odds = atkStrength / std::max(1.0f, atkStrength + defStrength);
    gfx_.textCentred(box.x + box.w * 0.5f, box.y + 292,
                     "ESTIMATED ODDS  " + std::to_string(static_cast<int>(odds * 100.0f)) + " : " +
                         std::to_string(100 - static_cast<int>(odds * 100.0f)),
                     pal::kTextDim, 1);

    Rect autoBtn{box.x + 20, box.bottom() - 62, 190, 40};
    Rect fightBtn{box.x + 225, box.bottom() - 62, 190, 40};
    Rect withdrawBtn{box.x + 430, box.bottom() - 62, 190, 40};

    if (button(gfx_, input_, autoBtn, "AUTO-RESOLVE")) {
        lastReport_ = game_.autoResolvePendingBattle();
        haveReport_ = true;
        screen_ = Screen::Summary;
    }
    if (button(gfx_, input_, fightBtn, "FIGHT IN PERSON")) {
        startTacticalBattle();
    }
    ButtonStyle danger;
    danger.fill = Color(62, 30, 30);
    danger.fillHover = Color(96, 42, 40);
    danger.border = pal::kDanger;
    if (button(gfx_, input_, withdrawBtn, "WITHDRAW", true, danger)) {
        BattleSetup taken;
        if (game_.takePendingBattle(taken)) {
            OrderResult r = game_.withdraw(taken.planet, me);
            setStatus(r.message);
        }
    }
}

}  // namespace ui
}  // namespace gc
