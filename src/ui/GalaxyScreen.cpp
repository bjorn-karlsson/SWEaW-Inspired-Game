#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ui/App.h"

namespace gc {
namespace ui {

namespace {

constexpr float kTopBarH = 54.0f;
constexpr float kSideW = 372.0f;

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

/// Short tag shown next to a unit in lists.
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

}  // namespace

// ---------------------------------------------------------------------------
// Camera helpers
// ---------------------------------------------------------------------------
Rect App::mapViewport() const {
    return Rect{0, kTopBarH, static_cast<float>(gfx_.width()) - kSideW,
                static_cast<float>(gfx_.height()) - kTopBarH};
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
        float radius = std::max(12.0f, 9.0f * zoom_);
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
    listScroll_ = 0.0f;
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
    // Keyboard.
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
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float panSpeed = 420.0f * dt / zoom_;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) camera_.x -= panSpeed;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) camera_.x += panSpeed;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) camera_.y -= panSpeed;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) camera_.y += panSpeed;

    Rect vp = mapViewport();
    bool overMap = vp.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));

    // Zoom towards the cursor.
    if (input_.wheel != 0 && overMap) {
        Vec2 before = screenToWorld(Vec2(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)));
        zoom_ *= (input_.wheel > 0) ? 1.15f : 1.0f / 1.15f;
        zoom_ = std::max(0.55f, std::min(4.5f, zoom_));
        Vec2 after = screenToWorld(Vec2(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)));
        camera_ += before - after;
    }

    hoverPlanet_ = planetAtScreen(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));

    if (!game_.hasPendingPlayerBattle() && !showHelp_) {
        if (overMap && input_.mouseClicked && hoverPlanet_ != kInvalid) selectPlanet(hoverPlanet_);
        if (overMap && input_.rightClicked && hoverPlanet_ != kInvalid) issueMoveOrder(hoverPlanet_);
    }

    game_.update(dt);
}

// ---------------------------------------------------------------------------
// Map drawing
// ---------------------------------------------------------------------------
void App::drawGalaxy() {
    Rect vp = mapViewport();
    gfx_.pushClip(vp);

    // Star field (fixed to the world so panning feels right).
    uint32_t seed = 987654321u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    for (int i = 0; i < 700; ++i) {
        Vec2 world(rnd() * 1100.0f - 50.0f, rnd() * 900.0f - 50.0f);
        Vec2 s = worldToScreen(world);
        if (!vp.contains(s.x, s.y)) continue;
        int v = 70 + static_cast<int>(rnd() * 150.0f);
        gfx_.rect(Rect{s.x, s.y, 1.0f, 1.0f}, Color(v, v, v + 20 > 255 ? 255 : v + 20));
    }

    // Lanes.
    for (const LaneDef& l : game_.lanes()) {
        Vec2 a = worldToScreen(game_.planet(l.a).def().pos);
        Vec2 b = worldToScreen(game_.planet(l.b).def().pos);
        if (l.hyperlane) {
            gfx_.thickLine(a.x, a.y, b.x, b.y, std::max(2.0f, 3.0f * zoom_ * 0.6f), pal::kHyperlane);
        } else {
            gfx_.line(a.x, a.y, b.x, b.y, pal::kLane);
        }
    }

    // Movement orders of the selected units get a highlighted route.
    if (selectedPlanet_ != kInvalid && hoverPlanet_ != kInvalid && !selectedUnits_.empty() &&
        hoverPlanet_ != selectedPlanet_) {
        std::vector<Id> path = game_.findPath(selectedPlanet_, hoverPlanet_);
        Id prev = selectedPlanet_;
        float totalDays = 0.0f;
        for (Id step : path) {
            Vec2 a = worldToScreen(game_.planet(prev).def().pos);
            Vec2 b = worldToScreen(game_.planet(step).def().pos);
            gfx_.thickLine(a.x, a.y, b.x, b.y, 2.5f, pal::kAccent.withAlpha(180));
            totalDays += game_.laneTravelDays(prev, step);
            prev = step;
        }
        if (!path.empty()) {
            Vec2 t = worldToScreen(game_.planet(hoverPlanet_).def().pos);
            gfx_.text(t.x + 14, t.y - 30, oneDecimal(totalDays) + " DAYS", pal::kAccent, 1);
        }
    }

    // Fleets in transit.
    for (const Fleet& f : game_.fleets()) {
        if (!f.alive || f.units.empty()) continue;
        Vec2 a = game_.planet(f.from).def().pos;
        Vec2 b = game_.planet(f.to).def().pos;
        Vec2 world = lerp(a, b, std::max(0.0f, std::min(1.0f, f.fraction())));
        Vec2 s = worldToScreen(world);
        Vec2 dir = (b - a).normalized();
        Vec2 perp(-dir.y, dir.x);
        float size = 7.0f;
        Color c = pal::faction(f.owner);
        gfx_.triangle(s + dir * size, s - dir * size * 0.6f + perp * size * 0.6f,
                      s - dir * size * 0.6f - perp * size * 0.6f, c);
        gfx_.text(s.x + 9, s.y - 6, std::to_string(f.units.size()), c, 1);
    }

    // Planets.
    for (int i = 0; i < game_.planetCount(); ++i) {
        const PlanetState& p = game_.planet(i);
        const PlanetDef& pd = p.def();
        Vec2 s = worldToScreen(pd.pos);
        float r = std::max(4.0f, (pd.spaceOnly ? 5.0f : 7.0f) * zoom_ * 0.8f);
        if (pd.baseIncome > 250) r *= 1.35f;
        Color c = pal::faction(p.owner);

        if (i == selectedPlanet_) {
            gfx_.circleOutline(s.x, s.y, r + 8.0f, pal::kAccent);
            gfx_.circleOutline(s.x, s.y, r + 9.0f, pal::kAccent.withAlpha(120));
        }
        if (i == hoverPlanet_) gfx_.circleOutline(s.x, s.y, r + 5.0f, Color(255, 255, 255, 160));

        if (pd.spaceOnly) {
            // Space-only systems are drawn as a diamond: no surface to take.
            gfx_.triangle(Vec2(s.x, s.y - r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c);
            gfx_.triangle(Vec2(s.x, s.y + r * 1.4f), Vec2(s.x + r * 1.2f, s.y),
                          Vec2(s.x - r * 1.2f, s.y), c.scaled(0.75f));
        } else {
            gfx_.circle(s.x, s.y, r, c.scaled(0.55f));
            gfx_.circleOutline(s.x, s.y, r, c);
        }

        // Presence markers: a pip per faction with forces here.
        float pipX = s.x - r;
        for (int fi = 1; fi < kFactionCount; ++fi) {
            Faction f = factionFromIndex(fi);
            int units = static_cast<int>(game_.allUnitsAt(i, f).size());
            if (units == 0) continue;
            gfx_.rect(Rect{pipX, s.y + r + 3.0f, 4.0f, 4.0f}, pal::faction(f));
            pipX += 6.0f;
        }
        if (game_.isContested(i)) {
            gfx_.circleOutline(s.x, s.y, r + 12.0f, pal::kDanger);
            gfx_.text(s.x + r + 6.0f, s.y - r - 12.0f, "!", pal::kDanger, 2);
        }
        if (!p.queue.empty() && p.owner == game_.playerFaction()) {
            gfx_.rect(Rect{s.x + r + 3.0f, s.y - r - 2.0f, 4.0f, 4.0f}, pal::kWarning);
        }

        if (zoom_ > 0.9f) {
            gfx_.textCentred(s.x, s.y + r + 12.0f, pd.name,
                             i == selectedPlanet_ ? pal::kText : pal::kTextDim, 1);
        }
    }

    gfx_.popClip();

    drawTopBar();
    drawSidePanel();
    drawMinimap();
    drawEventLog();
    drawPlanetTooltip();

    if (statusTimer_ > 0.0f) {
        Rect r{vp.x + 16, vp.bottom() - 178, static_cast<float>(Gfx::textWidth(status_, 2)) + 24, 28};
        gfx_.panel(r, pal::kPanel, pal::kBorderBright);
        gfx_.text(r.x + 12, r.y + 8, status_, pal::kText, 2);
    }

    if (showHelp_) {
        Rect r{vp.x + vp.w * 0.5f - 300, vp.y + 60, 600, 330};
        gfx_.panel(r, pal::kPanel, pal::kBorderBright);
        gfx_.textCentred(r.x + r.w * 0.5f, r.y + 14, "CONTROLS", pal::kAccent, 3);
        const char* lines[] = {
            "LEFT CLICK PLANET      select a world",
            "RIGHT CLICK PLANET     send the selected units there",
            "MOUSE WHEEL            zoom      WASD/ARROWS  pan",
            "SPACE                  pause / resume",
            "1 2 3                  normal / fast / fastest",
            "F1                     this help      ESC  clear selection",
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
}

// ---------------------------------------------------------------------------
// Top bar
// ---------------------------------------------------------------------------
void App::drawTopBar() {
    const float w = static_cast<float>(gfx_.width());
    Rect bar{0, 0, w, kTopBarH};
    gfx_.panel(bar, pal::kPanel, pal::kBorder);

    Faction me = game_.playerFaction();
    const FactionState& fs = game_.faction(me);
    gfx_.rect(Rect{0, 0, 6, kTopBarH}, pal::faction(me));
    gfx_.text(16, 8, factionShortName(me), pal::faction(me), 3);
    gfx_.text(16, 32, std::to_string(game_.planetsOwned(me)) + " WORLDS", pal::kTextDim, 1);

    gfx_.text(200, 8, "CREDITS", pal::kTextDim, 1);
    gfx_.text(200, 24, credits(fs.credits), pal::kWarning, 3);

    gfx_.text(400, 8, "WEEKLY INCOME", pal::kTextDim, 1);
    gfx_.text(400, 24, "+" + credits(game_.factionIncome(me)), pal::kGood, 2);

    // Calendar with the days ticking towards payday.
    gfx_.text(600, 8, game_.date().toString(), pal::kText, 2);
    Rect weekBar{600, 30, 190, 12};
    float weekFrac = (static_cast<float>(game_.date().dayOfWeek()) + game_.dayFraction()) /
                     static_cast<float>(kDaysPerWeek);
    progressBar(gfx_, weekBar, weekFrac, pal::kAccent, Color(20, 26, 38));
    gfx_.text(796, 31, "PAYDAY", pal::kTextDim, 1);

    // Speed controls.
    float x = 890;
    struct SpeedButton {
        const char* label;
        GameSpeed speed;
    };
    const SpeedButton buttons[] = {{"PAUSE", GameSpeed::Paused},
                                   {"1X", GameSpeed::Normal},
                                   {"2X", GameSpeed::Fast},
                                   {"4X", GameSpeed::Fastest}};
    for (const SpeedButton& b : buttons) {
        Rect r{x, 10, b.speed == GameSpeed::Paused ? 76.0f : 46.0f, 32};
        if (toggleButton(gfx_, input_, r, b.label, game_.speed() == b.speed)) game_.setSpeed(b.speed);
        x += r.w + 6;
    }

    // Faction standings.
    x += 14;
    for (Faction f : playableFactions()) {
        if (game_.faction(f).defeated) continue;
        gfx_.text(x, 10, factionShortName(f), pal::faction(f), 1);
        gfx_.text(x, 26, std::to_string(game_.planetsOwned(f)) + " WORLDS", pal::kTextDim, 1);
        x += 96;
    }

    Rect help{w - 190, 10, 84, 32};
    if (button(gfx_, input_, help, "HELP F1")) showHelp_ = !showHelp_;
    Rect menu{w - 100, 10, 88, 32};
    if (button(gfx_, input_, menu, "MENU")) screen_ = Screen::Menu;

    if (game_.paused()) {
        gfx_.textCentred(w * 0.5f, kTopBarH + 10, "- PAUSED -", pal::kWarning, 2);
    }
}

// ---------------------------------------------------------------------------
// Side panel: planet detail, garrison, build tabs
// ---------------------------------------------------------------------------
void App::drawSidePanel() {
    const float h = static_cast<float>(gfx_.height());
    Rect panel{static_cast<float>(gfx_.width()) - kSideW, kTopBarH, kSideW, h - kTopBarH};
    gfx_.panel(panel, pal::kPanel, pal::kBorder);

    if (selectedPlanet_ == kInvalid) {
        gfx_.text(panel.x + 14, panel.y + 16, "NO WORLD SELECTED", pal::kTextDim, 2);
        wrappedText(gfx_, Rect{panel.x + 14, panel.y + 44, panel.w - 28, 200},
                    "Click a world on the galactic map to inspect it, build there and give "
                    "orders to its garrison.",
                    pal::kTextDim, 1);
        return;
    }

    const PlanetState& p = game_.planet(selectedPlanet_);
    const PlanetDef& pd = p.def();
    Faction me = game_.playerFaction();
    bool mine = p.owner == me;

    // --- header ---
    float y = panel.y + 12;
    gfx_.text(panel.x + 14, y, pd.name, pal::faction(p.owner), 3);
    y += 28;
    gfx_.text(panel.x + 14, y, pd.region + "  -  " + factionShortName(p.owner), pal::kTextDim, 1);
    y += 18;
    if (pd.spaceOnly) {
        gfx_.text(panel.x + 14, y, "SPACE-ONLY SYSTEM: HOLD ORBIT TO CAPTURE", pal::kWarning, 1);
        y += 16;
    }

    // Traits
    for (Trait t : pd.traits) {
        gfx_.text(panel.x + 14, y, std::string("+ ") + traitName(t), pal::kAccent, 1);
        gfx_.textRight(panel.right() - 14, y, traitDescription(t), pal::kTextDim, 1);
        y += 15;
    }
    y += 4;

    // Economy and slots
    gfx_.text(panel.x + 14, y,
              "INCOME " + credits(game_.planetIncome(selectedPlanet_)) + " / WEEK", pal::kGood, 1);
    if (game_.isContested(selectedPlanet_)) {
        gfx_.textRight(panel.right() - 14, y, "UNDER SIEGE", pal::kDanger, 1);
    }
    y += 18;

    auto slotLine = [&](const char* label, Domain domain) {
        int used = game_.usedUnitSlots(selectedPlanet_, mine ? me : p.owner, domain);
        int cap = game_.unitSlotCapacity(selectedPlanet_, domain);
        int bUsed = game_.usedBuildSlots(selectedPlanet_, domain);
        int bCap = game_.buildSlotCapacity(selectedPlanet_, domain);
        gfx_.text(panel.x + 14, y, label, pal::kTextDim, 1);
        gfx_.text(panel.x + 90, y, "UNITS " + std::to_string(used) + "/" + std::to_string(cap),
                  used >= cap ? pal::kWarning : pal::kText, 1);
        gfx_.text(panel.x + 210, y, "BUILDINGS " + std::to_string(bUsed) + "/" + std::to_string(bCap),
                  bUsed >= bCap ? pal::kWarning : pal::kText, 1);
        y += 16;
    };
    slotLine("SPACE", Domain::Space);
    if (!pd.spaceOnly) slotLine("GROUND", Domain::Ground);

    // Structures present
    if (!p.buildings.empty()) {
        std::string list;
        for (Id bid : p.buildings) {
            const BuildingInstance& b = game_.buildingInst(bid);
            if (!b.alive) continue;
            if (!list.empty()) list += ", ";
            list += b.def().name;
        }
        y += 2;
        float used = wrappedText(gfx_, Rect{panel.x + 14, y, panel.w - 28, 44}, list, pal::kTextDim, 1);
        y += used + 2;
    }

    gfx_.line(panel.x + 8, y, panel.right() - 8, y, pal::kBorder);
    y += 6;

    // --- garrison ---
    Rect garrison{panel.x + 8, y, panel.w - 16, 214};
    drawGarrisonList(garrison);
    y = garrison.bottom() + 8;

    // --- tabs ---
    const char* tabs[] = {"SPACE", "GROUND", "BUILD", "TECH", "HEROES"};
    float tabW = (panel.w - 16) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        Rect r{panel.x + 8 + tabW * static_cast<float>(i), y, tabW - 2, 26};
        if (toggleButton(gfx_, input_, r, tabs[i], buildTab_ == i)) buildTab_ = i;
    }
    y += 32;

    Rect content{panel.x + 8, y, panel.w - 16, panel.bottom() - y - 120};
    drawBuildTab(content);

    // --- production queue ---
    Rect queue{panel.x + 8, panel.bottom() - 114, panel.w - 16, 106};
    gfx_.panel(queue, pal::kPanelLight, pal::kBorder);
    gfx_.text(queue.x + 8, queue.y + 6, "PRODUCTION QUEUE", pal::kAccent, 1);
    if (p.queue.empty()) {
        gfx_.text(queue.x + 8, queue.y + 24, mine ? "IDLE" : "-", pal::kTextDim, 1);
    } else {
        float qy = queue.y + 22;
        for (size_t i = 0; i < p.queue.size() && i < 4; ++i) {
            const BuildOrder& o = p.queue[i];
            std::string name = o.kind == BuildKind::Unit ? db().unit(o.defId).name
                                                         : db().building(o.defId).name;
            gfx_.text(queue.x + 8, qy, name.substr(0, 26), pal::kText, 1);
            float frac = o.totalDays > 0 ? 1.0f - o.daysRemaining / o.totalDays : 0.0f;
            progressBar(gfx_, Rect{queue.x + 200, qy, 100, 10}, frac, pal::kAccent, Color(18, 24, 34));
            if (mine) {
                Rect cancel{queue.right() - 44, qy - 2, 38, 14};
                if (button(gfx_, input_, cancel, "X")) {
                    OrderResult r = game_.cancelBuildOrder(selectedPlanet_, static_cast<int>(i), me);
                    setStatus(r.message);
                    break;
                }
            }
            qy += 20;
        }
    }
}

// ---------------------------------------------------------------------------
// Garrison list and unit orders
// ---------------------------------------------------------------------------
void App::drawGarrisonList(const Rect& area) {
    gfx_.panel(area, pal::kPanelLight, pal::kBorder);
    Faction me = game_.playerFaction();
    const PlanetState& p = game_.planet(selectedPlanet_);

    gfx_.text(area.x + 8, area.y + 6, "FORCES IN SYSTEM", pal::kAccent, 1);

    // Enemy strength readout.
    float mySpace = game_.forceStrengthAt(selectedPlanet_, me, Domain::Space);
    float myGround = game_.forceStrengthAt(selectedPlanet_, me, Domain::Ground);
    float enemySpace = 0.0f, enemyGround = 0.0f;
    for (int fi = 0; fi < kFactionCount; ++fi) {
        Faction f = factionFromIndex(fi);
        if (f == me) continue;
        enemySpace += game_.forceStrengthAt(selectedPlanet_, f, Domain::Space);
        enemyGround += game_.forceStrengthAt(selectedPlanet_, f, Domain::Ground);
    }
    gfx_.textRight(area.right() - 8, area.y + 6,
                   "YOU " + std::to_string(static_cast<int>(mySpace + myGround)) + " / ENEMY " +
                       std::to_string(static_cast<int>(enemySpace + enemyGround)),
                   enemySpace + enemyGround > mySpace + myGround ? pal::kDanger : pal::kGood, 1);

    Rect list{area.x + 6, area.y + 22, area.w - 12, area.h - 60};
    gfx_.pushClip(list);

    float y = list.y - listScroll_;
    int rows = 0;
    for (Id id : p.units) {
        const UnitInstance& u = game_.unit(id);
        if (!u.alive) continue;
        const UnitDef& d = u.def();
        bool isMine = u.owner == me;
        bool selected = std::find(selectedUnits_.begin(), selectedUnits_.end(), id) != selectedUnits_.end();
        Rect row{list.x, y, list.w, 17};
        if (y + 17 >= list.y && y <= list.bottom()) {
            bool hover = isMine && row.contains(static_cast<float>(input_.mouseX),
                                                static_cast<float>(input_.mouseY)) &&
                         list.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
            if (selected) gfx_.rect(row, Color(40, 78, 110, 200));
            else if (hover) gfx_.rect(row, Color(30, 42, 58, 160));

            Color c = pal::faction(u.owner);
            gfx_.text(row.x + 4, row.y + 4, classTag(d.unitClass), c.scaled(0.85f), 1);
            std::string name = d.name;
            if (d.isHero) name = "* " + name;
            gfx_.text(row.x + 30, row.y + 4, name.substr(0, 26), isMine ? pal::kText : pal::kTextDim, 1);
            if (d.domain() == Domain::Ground) {
                gfx_.text(row.x + row.w - 96, row.y + 4, u.landed ? "SURFACE" : "ORBIT",
                          u.landed ? pal::kGood : pal::kWarning, 1);
            }
            progressBar(gfx_, Rect{row.x + row.w - 44, row.y + 5, 38, 8}, u.health,
                        u.health > 0.6f ? pal::kGood : (u.health > 0.3f ? pal::kWarning : pal::kDanger),
                        Color(16, 20, 30));
            if (hover && input_.mouseClicked) toggleUnitSelection(id);
        }
        y += 17;
        ++rows;
    }
    if (rows == 0) gfx_.text(list.x + 6, list.y + 6, "NO FORCES PRESENT", pal::kTextDim, 1);
    gfx_.popClip();

    // Scrolling inside the list.
    if (list.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
        input_.wheel != 0) {
        listScroll_ -= static_cast<float>(input_.wheel) * 30.0f;
        float maxScroll = std::max(0.0f, static_cast<float>(rows) * 17.0f - list.h);
        listScroll_ = std::max(0.0f, std::min(maxScroll, listScroll_));
    }

    // Order buttons.
    float by = area.bottom() - 32;
    float bw = (area.w - 24) / 3.0f;
    if (button(gfx_, input_, Rect{area.x + 8, by, bw, 24}, "ALL SHIPS")) {
        selectAllAt(selectedPlanet_, Domain::Space);
    }
    if (button(gfx_, input_, Rect{area.x + 12 + bw, by, bw, 24}, "ALL TROOPS")) {
        selectAllAt(selectedPlanet_, Domain::Ground);
    }

    // Landing: only possible once the orbit is clear.
    std::vector<Id> landing;
    for (Id id : selectedUnits_) {
        const UnitInstance& u = game_.unit(id);
        if (u.alive && u.owner == me && u.planet == selectedPlanet_ &&
            u.def().domain() == Domain::Ground && !u.landed) {
            landing.push_back(id);
        }
    }
    bool canLand = !landing.empty() && game_.orbitClearFor(selectedPlanet_, me) &&
                   !game_.planet(selectedPlanet_).def().spaceOnly;
    ButtonStyle invade;
    invade.fill = Color(70, 34, 34);
    invade.fillHover = Color(110, 48, 44);
    invade.border = pal::kDanger;
    if (button(gfx_, input_, Rect{area.x + 16 + bw * 2, by, bw, 24},
               p.owner == me ? "DEPLOY" : "INVADE", canLand, invade)) {
        OrderResult r = game_.invade(selectedPlanet_, landing, me);
        setStatus(r.message);
        selectedUnits_.clear();
    }
}

// ---------------------------------------------------------------------------
// Build / research tabs
// ---------------------------------------------------------------------------
void App::drawBuildTab(const Rect& area) {
    gfx_.panel(area, pal::kPanelLight, pal::kBorder);
    Faction me = game_.playerFaction();
    const PlanetState& p = game_.planet(selectedPlanet_);
    bool mine = p.owner == me;

    gfx_.pushClip(area);
    float y = area.y + 6;

    if (buildTab_ == 3) {  // research
        const FactionState& fs = game_.faction(me);
        if (!fs.research.empty()) {
            const ResearchOrder& r = fs.research.front();
            gfx_.text(area.x + 8, y, "RESEARCHING: " + db().tech(r.techId).name, pal::kAccent, 1);
            y += 16;
            progressBar(gfx_, Rect{area.x + 8, y, area.w - 16, 10},
                        r.totalDays > 0 ? 1.0f - r.daysRemaining / r.totalDays : 0.0f, pal::kAccent,
                        Color(18, 24, 34));
            y += 18;
        }
        for (Id tid : game_.researchableTechs(me)) {
            const TechDef& t = db().tech(tid);
            if (y > area.bottom() - 40) break;
            Rect row{area.x + 6, y, area.w - 12, 38};
            gfx_.rect(row, Color(22, 30, 44, 160));
            gfx_.text(row.x + 6, row.y + 4, t.name, pal::kText, 1);
            gfx_.text(row.x + 6, row.y + 18, t.unlocksText.substr(0, 44), pal::kTextDim, 1);
            int cost = game_.techCost(me, tid);
            gfx_.textRight(row.right() - 60, row.y + 4, credits(cost) + " CR", pal::kWarning, 1);
            gfx_.textRight(row.right() - 60, row.y + 18,
                           oneDecimal(game_.techDays(me, tid)) + " DAYS", pal::kTextDim, 1);
            Rect go{row.right() - 54, row.y + 8, 48, 22};
            bool afford = game_.faction(me).credits >= cost;
            if (button(gfx_, input_, go, "GO", afford)) {
                setStatus(game_.startResearch(me, tid).message);
            }
            y += 42;
        }
        if (game_.researchableTechs(me).empty() && fs.research.empty()) {
            gfx_.text(area.x + 8, y, "ALL TECHNOLOGY RESEARCHED", pal::kGood, 1);
        }
        gfx_.popClip();
        return;
    }

    if (buildTab_ == 4) {  // heroes
        gfx_.text(area.x + 8, y, "COMMANDERS IN THE FIELD", pal::kAccent, 1);
        y += 18;
        bool any = false;
        for (const UnitInstance& u : game_.units()) {
            if (!u.alive || u.owner != me || !u.def().isHero) continue;
            any = true;
            const UnitDef& d = u.def();
            std::string where = u.planet != kInvalid ? game_.planet(u.planet).def().name : "IN TRANSIT";
            gfx_.text(area.x + 8, y, d.name.substr(0, 26), pal::kText, 1);
            gfx_.textRight(area.right() - 8, y, where, pal::kTextDim, 1);
            y += 14;
            std::string bonus;
            if (d.heroCombatBonus > 0.0f) {
                bonus += "+" + std::to_string(static_cast<int>(d.heroCombatBonus * 100.0f)) + "% COMBAT ";
            }
            if (d.heroIncomeBonus > 0) bonus += "+" + std::to_string(d.heroIncomeBonus) + " CR/WEEK";
            gfx_.text(area.x + 20, y, bonus, pal::kAccent, 1);
            y += 18;
            if (y > area.bottom() - 60) break;
        }
        if (!any) gfx_.text(area.x + 8, y, "NONE RECRUITED", pal::kTextDim, 1);

        y += 10;
        gfx_.text(area.x + 8, y, "AWAITING ORDERS", pal::kAccent, 1);
        y += 16;
        for (const auto& kv : game_.faction(me).heroRespawnTimer) {
            if (kv.second <= 0) continue;
            gfx_.text(area.x + 8, y, db().unit(kv.first).name.substr(0, 24), pal::kTextDim, 1);
            gfx_.textRight(area.right() - 8, y, std::to_string(kv.second) + " DAYS", pal::kWarning, 1);
            y += 14;
            if (y > area.bottom() - 10) break;
        }
        gfx_.popClip();
        return;
    }

    if (!mine) {
        gfx_.text(area.x + 8, y, "YOU DO NOT CONTROL THIS WORLD", pal::kTextDim, 1);
        gfx_.popClip();
        return;
    }

    if (buildTab_ == 2) {  // structures
        for (Id bid : game_.buildableBuildings(selectedPlanet_, me)) {
            const BuildingDef& b = db().building(bid);
            if (y > area.bottom() - 34) break;
            OrderResult can = game_.canQueueBuilding(selectedPlanet_, bid, me);
            Rect row{area.x + 6, y, area.w - 12, 32};
            bool hover = row.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
            gfx_.rect(row, hover ? Color(32, 46, 66, 200) : Color(22, 30, 44, 160));
            Color nameColour = can.ok ? pal::kText : pal::kTextDim;
            gfx_.text(row.x + 6, row.y + 3, b.name.substr(0, 30), nameColour, 1);
            gfx_.text(row.x + 6, row.y + 17,
                      std::string(b.domain == Domain::Space ? "ORBIT" : "SURFACE") +
                          (can.ok ? "" : "  -  " + can.message).substr(0, 40),
                      can.ok ? pal::kTextDim : pal::kDanger, 1);
            gfx_.textRight(row.right() - 56, row.y + 3, credits(game_.buildingCost(selectedPlanet_, bid)),
                           pal::kWarning, 1);
            gfx_.textRight(row.right() - 56, row.y + 17,
                           oneDecimal(game_.buildingBuildDays(selectedPlanet_, bid)) + "D",
                           pal::kTextDim, 1);
            Rect go{row.right() - 50, row.y + 5, 44, 22};
            if (button(gfx_, input_, go, "BUILD", can.ok)) {
                setStatus(game_.queueBuilding(selectedPlanet_, bid, me).message);
            }
            y += 36;
        }
        gfx_.popClip();
        return;
    }

    // Unit tabs (space / ground).
    Domain domain = buildTab_ == 0 ? Domain::Space : Domain::Ground;
    int tier = game_.bestProductionTier(selectedPlanet_, me, domain);
    gfx_.text(area.x + 8, y,
              std::string(domain == Domain::Space ? "ORBITAL YARDS TIER " : "PRODUCTION TIER ") +
                  std::to_string(tier),
              tier > 0 ? pal::kAccent : pal::kDanger, 1);
    y += 16;
    if (tier == 0) {
        wrappedText(gfx_, Rect{area.x + 8, y, area.w - 16, 60},
                    domain == Domain::Space
                        ? "Build an orbital station here before any ships can be laid down."
                        : "Build a barracks or factory here before any troops can be trained.",
                    pal::kTextDim, 1);
        gfx_.popClip();
        return;
    }

    for (Id uid : game_.buildableUnits(selectedPlanet_, me)) {
        const UnitDef& u = db().unit(uid);
        if (u.domain() != domain) continue;
        if (y > area.bottom() - 34) break;
        OrderResult can = game_.canQueueUnit(selectedPlanet_, uid, me);
        Rect row{area.x + 6, y, area.w - 12, 32};
        bool hover = row.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
        gfx_.rect(row, hover ? Color(32, 46, 66, 200) : Color(22, 30, 44, 160));
        gfx_.text(row.x + 6, row.y + 3, classTag(u.unitClass), pal::kAccent.scaled(0.9f), 1);
        gfx_.text(row.x + 32, row.y + 3, (u.isHero ? "* " + u.name : u.name).substr(0, 28),
                  can.ok ? pal::kText : pal::kTextDim, 1);
        std::string detail = "HP " + std::to_string(static_cast<int>(u.hull));
        if (u.shield > 0.0f) detail += " SH " + std::to_string(static_cast<int>(u.shield));
        detail += " DMG " + std::to_string(static_cast<int>(u.damageAntiCapital)) + "/" +
                  std::to_string(static_cast<int>(u.damageAntiFighter));
        if (!u.wings.empty()) {
            int wings = 0;
            for (const CarriedWing& wg : u.wings) wings += wg.count;
            detail += " +" + std::to_string(wings) + "SQ";
        }
        gfx_.text(row.x + 32, row.y + 17, can.ok ? detail : can.message.substr(0, 40),
                  can.ok ? pal::kTextDim : pal::kDanger, 1);
        gfx_.textRight(row.right() - 56, row.y + 3, credits(game_.unitCost(selectedPlanet_, uid)),
                       pal::kWarning, 1);
        gfx_.textRight(row.right() - 56, row.y + 17,
                       oneDecimal(game_.unitBuildDays(selectedPlanet_, uid)) + "D  " +
                           std::to_string(u.popCost) + "SLT",
                       pal::kTextDim, 1);
        Rect go{row.right() - 50, row.y + 5, 44, 22};
        if (button(gfx_, input_, go, "BUILD", can.ok)) {
            setStatus(game_.queueUnit(selectedPlanet_, uid, me).message);
        }
        y += 36;
    }
    gfx_.popClip();
}

// ---------------------------------------------------------------------------
// Minimap, event log, tooltip, battle prompt
// ---------------------------------------------------------------------------
void App::drawMinimap() {
    Rect vp = mapViewport();
    Rect map{vp.right() - 268, vp.bottom() - 208, 260, 200};
    gfx_.panel(map, Color(10, 14, 24, 235), pal::kBorderBright);
    gfx_.text(map.x + 6, map.y + 4, "GALACTIC OVERVIEW", pal::kAccent, 1);

    // Fit all planets into the minimap.
    Vec2 lo(1e9f, 1e9f), hi(-1e9f, -1e9f);
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = game_.planet(i).def().pos;
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
    }
    Rect inner = Rect{map.x + 8, map.y + 18, map.w - 16, map.h - 26};
    float sx = inner.w / std::max(1.0f, hi.x - lo.x);
    float sy = inner.h / std::max(1.0f, hi.y - lo.y);
    float s = std::min(sx, sy);
    auto toMini = [&](Vec2 p) {
        return Vec2(inner.x + (p.x - lo.x) * s + (inner.w - (hi.x - lo.x) * s) * 0.5f,
                    inner.y + (p.y - lo.y) * s + (inner.h - (hi.y - lo.y) * s) * 0.5f);
    };

    for (const LaneDef& l : game_.lanes()) {
        Vec2 a = toMini(game_.planet(l.a).def().pos);
        Vec2 b = toMini(game_.planet(l.b).def().pos);
        gfx_.line(a.x, a.y, b.x, b.y, l.hyperlane ? pal::kHyperlane.scaled(0.6f) : pal::kLane.scaled(0.7f));
    }
    for (int i = 0; i < game_.planetCount(); ++i) {
        Vec2 p = toMini(game_.planet(i).def().pos);
        Color c = pal::faction(game_.planet(i).owner);
        float r = game_.isContested(i) ? 3.5f : 2.5f;
        gfx_.rect(Rect{p.x - r, p.y - r, r * 2, r * 2}, c);
        if (i == selectedPlanet_) gfx_.rectOutline(Rect{p.x - 4, p.y - 4, 8, 8}, pal::kAccent);
    }

    // Viewport rectangle.
    Vec2 topLeft = screenToWorld(Vec2(mapViewport().x, mapViewport().y));
    Vec2 bottomRight = screenToWorld(Vec2(mapViewport().right(), mapViewport().bottom()));
    Vec2 a = toMini(topLeft);
    Vec2 b = toMini(bottomRight);
    gfx_.rectOutline(Rect{a.x, a.y, b.x - a.x, b.y - a.y}, Color(255, 255, 255, 110));

    // Click to jump.
    if (map.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)) &&
        input_.mouseClicked) {
        float wx = lo.x + (static_cast<float>(input_.mouseX) - inner.x -
                           (inner.w - (hi.x - lo.x) * s) * 0.5f) / s;
        float wy = lo.y + (static_cast<float>(input_.mouseY) - inner.y -
                           (inner.h - (hi.y - lo.y) * s) * 0.5f) / s;
        camera_ = Vec2(wx, wy);
    }
}

void App::drawEventLog() {
    Rect vp = mapViewport();
    Rect log{vp.x + 8, vp.bottom() - 148, 560, 140};
    gfx_.panel(log, Color(10, 14, 24, 215), pal::kBorder);
    gfx_.text(log.x + 8, log.y + 5, "HOLONET REPORTS", pal::kAccent, 1);
    const std::deque<GameEvent>& events = game_.events();
    float y = log.bottom() - 18;
    for (auto it = events.rbegin(); it != events.rend() && y > log.y + 20; ++it) {
        Color c = it->faction == Faction::Neutral ? pal::kTextDim : pal::faction(it->faction);
        gfx_.text(log.x + 8, y, ("D" + std::to_string(it->day) + "  " + it->text).substr(0, 88), c, 1);
        y -= 14;
    }
}

void App::drawPlanetTooltip() {
    if (hoverPlanet_ == kInvalid || hoverPlanet_ == selectedPlanet_) return;
    const PlanetState& p = game_.planet(hoverPlanet_);
    const PlanetDef& pd = p.def();
    Vec2 s = worldToScreen(pd.pos);

    std::vector<std::string> lines;
    lines.push_back(pd.name + "  (" + factionShortName(p.owner) + ")");
    lines.push_back(pd.region + (pd.spaceOnly ? "  -  SPACE ONLY" : ""));
    for (int fi = 0; fi < kFactionCount; ++fi) {
        Faction f = factionFromIndex(fi);
        int space = static_cast<int>(game_.unitsAt(hoverPlanet_, f, Domain::Space).size());
        int ground = static_cast<int>(game_.unitsAt(hoverPlanet_, f, Domain::Ground).size());
        if (space + ground == 0) continue;
        lines.push_back(std::string(factionShortName(f)) + ": " + std::to_string(space) + " SHIPS, " +
                        std::to_string(ground) + " TROOPS");
    }
    if (p.owner != Faction::Neutral) {
        lines.push_back("INCOME " + credits(game_.planetIncome(hoverPlanet_)) + "/WEEK");
    }

    float wMax = 0;
    for (const std::string& l : lines) wMax = std::max(wMax, static_cast<float>(Gfx::textWidth(l, 1)));
    Rect box{s.x + 16, s.y - 10, wMax + 20, static_cast<float>(lines.size()) * 14.0f + 12.0f};
    Rect vp = mapViewport();
    if (box.right() > vp.right()) box.x = s.x - box.w - 16;
    gfx_.panel(box, Color(12, 18, 30, 240), pal::kBorderBright);
    float y = box.y + 6;
    bool first = true;
    for (const std::string& l : lines) {
        gfx_.text(box.x + 10, y, l, first ? pal::faction(p.owner) : pal::kTextDim, 1);
        y += 14;
        first = false;
    }
}

void App::drawBattlePrompt() {
    const PendingBattle* pb = game_.pendingPlayerBattle();
    if (pb == nullptr) return;
    const BattleSetup& s = pb->setup;
    Faction me = game_.playerFaction();

    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    gfx_.rect(Rect{0, 0, w, h}, Color(0, 0, 0, 150));

    Rect box{w * 0.5f - 320, h * 0.5f - 190, 640, 380};
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
        // Group identical units.
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
