#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ui/App.h"

namespace gc {
namespace ui {

namespace {

constexpr float kBattleBarH = 56.0f;
constexpr float kBattleFooterH = 96.0f;

Color hullColour(float fraction) {
    if (fraction > 0.6f) return pal::kGood;
    if (fraction > 0.3f) return pal::kWarning;
    return pal::kDanger;
}

}  // namespace

Vec2 App::battleToScreen(Vec2 world) const {
    float cx = static_cast<float>(gfx_.width()) * 0.5f;
    float cy = kBattleBarH + (static_cast<float>(gfx_.height()) - kBattleBarH - kBattleFooterH) * 0.5f;
    return Vec2(cx + (world.x - battleCamera_.x) * battleZoom_,
                cy + (world.y - battleCamera_.y) * battleZoom_);
}

Vec2 App::screenToBattle(Vec2 screen) const {
    float cx = static_cast<float>(gfx_.width()) * 0.5f;
    float cy = kBattleBarH + (static_cast<float>(gfx_.height()) - kBattleBarH - kBattleFooterH) * 0.5f;
    return Vec2(battleCamera_.x + (screen.x - cx) / battleZoom_,
                battleCamera_.y + (screen.y - cy) / battleZoom_);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void App::updateBattle(float dt) {
    Faction me = game_.playerFaction();

    if (input_.keyPressed(SDLK_SPACE)) battlePaused_ = !battlePaused_;
    if (input_.keyPressed(SDLK_1)) battleSpeed_ = 1.0f;
    if (input_.keyPressed(SDLK_2)) battleSpeed_ = 2.0f;
    if (input_.keyPressed(SDLK_3)) battleSpeed_ = 4.0f;
    if (input_.keyPressed(SDLK_r) && !battle_.finished()) {
        battle_.beginRetreat(me);
        setStatus("Sounding the retreat - surviving units are running for the edge");
    }
    if (input_.keyPressed(SDLK_a)) {
        battleSelection_.clear();
        for (const tactical::TUnit& u : battle_.units()) {
            if (u.alive && !u.escaped && u.owner == me && !u.structure) battleSelection_.push_back(u.index);
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float pan = 700.0f * dt / battleZoom_;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) battleCamera_.x -= pan;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) battleCamera_.x += pan;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) battleCamera_.y -= pan;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) battleCamera_.y += pan;
    if (input_.wheel != 0) {
        battleZoom_ *= (input_.wheel > 0) ? 1.12f : 1.0f / 1.12f;
        battleZoom_ = std::max(0.3f, std::min(2.5f, battleZoom_));
    }

    Rect field{0, kBattleBarH, static_cast<float>(gfx_.width()),
               static_cast<float>(gfx_.height()) - kBattleBarH - kBattleFooterH};
    bool overField = field.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));

    // Box selection.
    if (overField && input_.mouseDown && !boxSelecting_) {
        boxSelecting_ = true;
        boxStart_ = Vec2(static_cast<float>(input_.dragStartX), static_cast<float>(input_.dragStartY));
    }
    if (boxSelecting_ && !input_.mouseDown) {
        boxSelecting_ = false;
        Vec2 end(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
        float x0 = std::min(boxStart_.x, end.x);
        float x1 = std::max(boxStart_.x, end.x);
        float y0 = std::min(boxStart_.y, end.y);
        float y1 = std::max(boxStart_.y, end.y);
        bool tinyBox = (x1 - x0) < 6.0f && (y1 - y0) < 6.0f;
        if (!input_.shift) battleSelection_.clear();
        for (const tactical::TUnit& u : battle_.units()) {
            if (!u.alive || u.escaped || u.owner != me || u.structure) continue;
            Vec2 s = battleToScreen(u.pos);
            bool hit = tinyBox ? (distance(s, end) < std::max(10.0f, u.radius * battleZoom_))
                               : (s.x >= x0 && s.x <= x1 && s.y >= y0 && s.y <= y1);
            if (hit && std::find(battleSelection_.begin(), battleSelection_.end(), u.index) ==
                           battleSelection_.end()) {
                battleSelection_.push_back(u.index);
            }
        }
    }

    // Orders.
    if (overField && input_.rightClicked && !battleSelection_.empty()) {
        Vec2 world = screenToBattle(Vec2(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY)));
        Id targetIndex = kInvalid;
        float best = 1e9f;
        for (const tactical::TUnit& u : battle_.units()) {
            if (!u.alive || u.escaped || u.owner == me) continue;
            float d = distance(u.pos, world);
            if (d < std::max(24.0f, u.radius * 1.6f) && d < best) {
                best = d;
                targetIndex = u.index;
            }
        }
        if (targetIndex != kInvalid) {
            battle_.orderAttack(battleSelection_, targetIndex);
        } else {
            battle_.orderMove(battleSelection_, world);
        }
    }

    if (!battlePaused_ && !battle_.finished()) {
        float step = dt * battleSpeed_;
        // Fixed sub-steps keep the simulation stable at any frame rate.
        const float fixed = 1.0f / 60.0f;
        while (step > 0.0f && !battle_.finished()) {
            float use = std::min(fixed, step);
            battle_.update(use);
            step -= use;
        }
    }

    if (battle_.finished()) {
        // Give the player a moment, then move to the summary.
        if (input_.keyPressed(SDLK_RETURN) || input_.keyPressed(SDLK_SPACE)) {
            finishBattle(battle_.resolution(game_));
        }
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void App::drawBattle() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    Faction me = game_.playerFaction();
    bool space = battle_.domain() == Domain::Space;

    Rect field{0, kBattleBarH, w, h - kBattleBarH - kBattleFooterH};
    gfx_.rect(field, space ? Color(6, 8, 18) : Color(18, 16, 12));
    gfx_.pushClip(field);

    // Backdrop.
    uint32_t seed = 424242u;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    if (space) {
        for (int i = 0; i < 500; ++i) {
            Vec2 p = battleToScreen(Vec2(rnd() * battle_.fieldSize().x, rnd() * battle_.fieldSize().y));
            int v = 60 + static_cast<int>(rnd() * 140.0f);
            gfx_.rect(Rect{p.x, p.y, 1, 1}, Color(v, v, v + 15 > 255 ? 255 : v + 15));
        }
    } else {
        for (int i = 0; i < 260; ++i) {
            Vec2 wp(rnd() * battle_.fieldSize().x, rnd() * battle_.fieldSize().y);
            Vec2 p = battleToScreen(wp);
            float r = (6.0f + rnd() * 22.0f) * battleZoom_;
            gfx_.circle(p.x, p.y, r, Color(30, 28, 20, 120));
        }
    }

    // Battlefield bounds.
    Vec2 tl = battleToScreen(Vec2(0, 0));
    Vec2 br = battleToScreen(battle_.fieldSize());
    gfx_.rectOutline(Rect{tl.x, tl.y, br.x - tl.x, br.y - tl.y}, Color(60, 80, 110, 140));

    // Weapon fire.
    for (const tactical::Shot& s : battle_.shots()) {
        Vec2 a = battleToScreen(s.from);
        Vec2 b = battleToScreen(s.to);
        Color c = pal::faction(s.owner);
        float alpha = std::max(0.0f, std::min(1.0f, s.life / 0.18f));
        c.a = static_cast<uint8_t>(220 * alpha);
        if (s.heavy) {
            gfx_.thickLine(a.x, a.y, b.x, b.y, 2.5f, c);
        } else {
            gfx_.line(a.x, a.y, b.x, b.y, c);
        }
    }

    // Units.
    for (const tactical::TUnit& u : battle_.units()) {
        if (!u.alive || u.escaped) continue;
        Vec2 p = battleToScreen(u.pos);
        float r = std::max(3.0f, u.radius * battleZoom_);
        Color c = pal::faction(u.owner);
        bool selected = std::find(battleSelection_.begin(), battleSelection_.end(), u.index) !=
                        battleSelection_.end();

        if (u.structure) {
            gfx_.rect(Rect{p.x - r, p.y - r, r * 2, r * 2}, c.scaled(0.5f));
            gfx_.rectOutline(Rect{p.x - r, p.y - r, r * 2, r * 2}, c);
        } else if (u.squadron) {
            gfx_.rect(Rect{p.x - r * 0.6f, p.y - r * 0.6f, r * 1.2f, r * 1.2f}, c);
        } else {
            // Warships and vehicles point at the enemy side.
            float dir = u.attackerSide ? 1.0f : -1.0f;
            gfx_.triangle(Vec2(p.x + dir * r * 1.5f, p.y), Vec2(p.x - dir * r * 0.8f, p.y - r * 0.8f),
                          Vec2(p.x - dir * r * 0.8f, p.y + r * 0.8f), c.scaled(0.75f));
            gfx_.triangle(Vec2(p.x + dir * r * 1.1f, p.y), Vec2(p.x - dir * r * 0.4f, p.y - r * 0.45f),
                          Vec2(p.x - dir * r * 0.4f, p.y + r * 0.45f), c);
        }

        if (u.maxShield > 0.0f && u.shield > 0.0f) {
            Color sc = c.withAlpha(static_cast<int>(60.0f * (u.shield / u.maxShield)) + 30);
            gfx_.circleOutline(p.x, p.y, r * 1.6f, sc);
        }
        if (selected) gfx_.circleOutline(p.x, p.y, r * 1.9f, pal::kAccent);
        if (u.retreating) gfx_.text(p.x - 10, p.y - r - 16, "RET", pal::kWarning, 1);

        // Health bar for anything larger than a squadron.
        if (!u.squadron && battleZoom_ > 0.4f) {
            Rect bar{p.x - r, p.y - r - 8, r * 2, 3};
            gfx_.rect(bar, Color(20, 20, 24, 200));
            gfx_.rect(Rect{bar.x, bar.y, bar.w * u.healthFraction(), bar.h},
                      hullColour(u.healthFraction()));
        }
    }

    // Selection box.
    if (boxSelecting_) {
        Vec2 end(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
        Rect box{std::min(boxStart_.x, end.x), std::min(boxStart_.y, end.y),
                 std::abs(end.x - boxStart_.x), std::abs(end.y - boxStart_.y)};
        gfx_.rect(box, Color(96, 200, 255, 40));
        gfx_.rectOutline(box, pal::kAccent);
    }

    gfx_.popClip();

    // --- top bar ---
    Rect bar{0, 0, w, kBattleBarH};
    gfx_.panel(bar, pal::kPanel, pal::kBorder);
    gfx_.text(14, 8, std::string(space ? "SPACE BATTLE" : "GROUND BATTLE") + " - " +
                         game_.planet(battleSetup_.planet).def().name,
              pal::kText, 2);
    gfx_.text(14, 32,
              std::string(battleSetup_.attacker == me ? "ATTACKING" : "DEFENDING") + "  -  " +
                  std::to_string(static_cast<int>(battle_.elapsed())) + "s",
              pal::kTextDim, 1);

    // Strength bars.
    float atk = battle_.sideHealth(true);
    float def = battle_.sideHealth(false);
    float total = std::max(1.0f, atk + def);
    Rect strength{420, 14, 520, 20};
    gfx_.rect(strength, Color(20, 24, 34));
    gfx_.rect(Rect{strength.x, strength.y, strength.w * (atk / total), strength.h},
              pal::faction(battleSetup_.attacker).scaled(0.9f));
    gfx_.rect(Rect{strength.x + strength.w * (atk / total), strength.y,
                   strength.w * (def / total), strength.h},
              pal::faction(battleSetup_.defender).scaled(0.9f));
    gfx_.rectOutline(strength, pal::kBorder);
    gfx_.text(strength.x, strength.y + 24,
              std::string(factionShortName(battleSetup_.attacker)) + "  " +
                  std::to_string(battle_.sideCount(true)) + " UNITS",
              pal::faction(battleSetup_.attacker), 1);
    gfx_.textRight(strength.right(), strength.y + 24,
                   std::to_string(battle_.sideCount(false)) + " UNITS  " +
                       factionShortName(battleSetup_.defender),
                   pal::faction(battleSetup_.defender), 1);

    float bx = 980;
    if (toggleButton(gfx_, input_, Rect{bx, 12, 76, 32}, "PAUSE", battlePaused_)) {
        battlePaused_ = !battlePaused_;
    }
    bx += 82;
    const float speeds[] = {1.0f, 2.0f, 4.0f};
    const char* speedLabels[] = {"1X", "2X", "4X"};
    for (int i = 0; i < 3; ++i) {
        if (toggleButton(gfx_, input_, Rect{bx, 12, 44, 32}, speedLabels[i],
                         std::abs(battleSpeed_ - speeds[i]) < 0.01f)) {
            battleSpeed_ = speeds[i];
        }
        bx += 48;
    }
    ButtonStyle danger;
    danger.fill = Color(62, 30, 30);
    danger.fillHover = Color(96, 42, 40);
    danger.border = pal::kDanger;
    if (button(gfx_, input_, Rect{bx + 12, 12, 150, 32}, "RETREAT (R)",
               !battle_.finished() && !battle_.isRetreating(me), danger)) {
        battle_.beginRetreat(me);
    }

    // --- footer: selected units and orders ---
    Rect footer{0, h - kBattleFooterH, w, kBattleFooterH};
    gfx_.panel(footer, pal::kPanel, pal::kBorder);
    gfx_.text(14, footer.y + 8,
              "SELECTED: " + std::to_string(battleSelection_.size()) + " UNITS", pal::kAccent, 1);

    float x = 14;
    float y = footer.y + 26;
    int shown = 0;
    for (Id idx : battleSelection_) {
        if (idx < 0 || idx >= static_cast<Id>(battle_.units().size())) continue;
        const tactical::TUnit& u = battle_.units()[static_cast<size_t>(idx)];
        if (!u.alive || u.escaped) continue;
        const UnitDef* d = u.def();
        if (d == nullptr) continue;
        Rect card{x, y, 168, 58};
        gfx_.panel(card, pal::kPanelLight, pal::kBorder);
        gfx_.text(card.x + 6, card.y + 5, d->name.substr(0, 22), pal::kText, 1);
        gfx_.text(card.x + 6, card.y + 20, unitClassName(d->unitClass), pal::kTextDim, 1);
        progressBar(gfx_, Rect{card.x + 6, card.y + 36, card.w - 12, 8}, u.healthFraction(),
                    hullColour(u.healthFraction()), Color(16, 20, 30));
        if (u.maxShield > 0.0f) {
            progressBar(gfx_, Rect{card.x + 6, card.y + 46, card.w - 12, 5}, u.shield / u.maxShield,
                        pal::kAccent, Color(16, 20, 30));
        }
        x += 174;
        if (++shown >= 6) break;
    }
    if (shown == 0) {
        gfx_.text(14, footer.y + 34,
                  "Drag a box to select your units. Right click to move or attack. A selects all.",
                  pal::kTextDim, 1);
    }
    gfx_.textRight(w - 14, footer.y + 8, "SPACE pauses   1/2/3 speed   R retreat", pal::kTextDim, 1);

    // --- end of battle overlay ---
    if (battle_.finished()) {
        Rect box{w * 0.5f - 260, h * 0.5f - 110, 520, 220};
        gfx_.rect(Rect{0, 0, w, h}, Color(0, 0, 0, 120));
        gfx_.panel(box, pal::kPanel, pal::kBorderBright);
        bool won = battle_.victor() == me;
        gfx_.textCentred(box.x + box.w * 0.5f, box.y + 26, won ? "VICTORY" : "DEFEAT",
                         won ? pal::kGood : pal::kDanger, 5);
        gfx_.textCentred(box.x + box.w * 0.5f, box.y + 86,
                         std::string(factionShortName(battle_.victor())) + " holds the field",
                         pal::kText, 2);
        gfx_.textCentred(box.x + box.w * 0.5f, box.y + 116,
                         "Battle length: " + std::to_string(static_cast<int>(battle_.elapsed())) +
                             " seconds",
                         pal::kTextDim, 1);
        Rect cont{box.x + box.w * 0.5f - 110, box.bottom() - 56, 220, 36};
        if (button(gfx_, input_, cont, "BATTLE REPORT")) {
            finishBattle(battle_.resolution(game_));
        }
    }
}

}  // namespace ui
}  // namespace gc
