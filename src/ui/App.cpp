#include "ui/App.h"

#include <algorithm>

namespace gc {
namespace ui {

int App::run(const AppOptions& options) {
    if (!gfx_.init("Galactic Conquest - A Star Wars Empire at War inspired RTS", 1600, 900)) {
        SDL_Log("Failed to open a window: %s", SDL_GetError());
        return 1;
    }

    if (options.autostart || options.demoBattle || options.demoSummary || options.demoHud) {
        startCampaign(options);
    }
    if (options.demoHud) grantDemoHeroes();
    if (options.demoBattle || options.demoSummary) startDemoBattle();
    if (options.demoSummary) {
        int guard = 0;
        while (!battle_.finished() && guard++ < 200000) battle_.update(1.0f / 30.0f);
        finishBattle(battle_.resolution(game_));
    }

    uint32_t last = SDL_GetTicks();
    int frame = 0;
    while (running_) {
        uint32_t now = SDL_GetTicks();
        float dt = static_cast<float>(now - last) / 1000.0f;
        last = now;
        dt = std::min(dt, 0.05f);

        handleEvents();
        update(dt);
        draw();

        ++frame;
        if (!options.screenshot.empty() && frame >= options.screenshotFrame) {
            if (!gfx_.saveScreenshot(options.screenshot)) {
                SDL_Log("Screenshot failed: %s", SDL_GetError());
            }
            running_ = false;
        }
    }

    gfx_.shutdown();
    return 0;
}

void App::startCampaign(const AppOptions& options) {
    const std::vector<CampaignDef>& campaigns = db().campaigns();
    const std::vector<Faction>& factions = playableFactions();
    GameSetup s;
    s.campaign = campaigns[static_cast<size_t>(
                               std::max(0, std::min(options.campaign,
                                                    static_cast<int>(campaigns.size()) - 1)))]
                     .id;
    s.playerFaction = factions[static_cast<size_t>(
        std::max(0, std::min(options.faction, static_cast<int>(factions.size()) - 1)))];
    s.difficulty = static_cast<Difficulty>(
        std::max(0, std::min(options.difficulty, static_cast<int>(Difficulty::Count) - 1)));
    s.seed = menuSeed_;
    game_.start(s);
    selectedPlanet_ = kInvalid;
    selectedUnits_.clear();
    centreCameraOnHomeworld();
    screen_ = Screen::Galaxy;
}

void App::grantDemoHeroes() {
    // Development aid: hand the player their commanders and pause, so the
    // hero roster and the paused banner can be inspected in a screenshot.
    Faction me = game_.playerFaction();
    Id home = selectedPlanet_;
    for (int i = 0; i < game_.planetCount() && home == kInvalid; ++i) {
        if (game_.planet(i).owner == me) home = i;
    }
    if (home == kInvalid) return;
    game_.faction(me).credits += 200000;
    for (const UnitDef& u : db().units()) {
        if (u.faction != me || !u.isHero) continue;
        if (u.requiredTech != kInvalid) game_.faction(me).techKnown[static_cast<size_t>(u.requiredTech)] = 1;
        game_.queueUnit(home, u.id, me);
    }
    // Run the queue out so the heroes actually exist.
    for (int i = 0; i < 4000 && !game_.planet(home).queue.empty(); ++i) game_.update(0.5f);
    game_.setSpeed(GameSpeed::Paused);
    selectedPlanet_ = home;
}

void App::startDemoBattle() {
    // Find the player's strongest fleet and the nearest enemy fleet, then throw
    // them at each other. Used by --demo-battle for smoke testing.
    Faction me = game_.playerFaction();
    Id mine = kInvalid;
    size_t bestCount = 0;
    for (int i = 0; i < game_.planetCount(); ++i) {
        size_t n = game_.unitsAt(i, me, Domain::Space).size();
        if (n > bestCount) {
            bestCount = n;
            mine = i;
        }
    }
    Id theirs = kInvalid;
    Faction enemyFaction = Faction::Neutral;
    for (int i = 0; i < game_.planetCount() && theirs == kInvalid; ++i) {
        for (Faction f : playableFactions()) {
            if (f == me) continue;
            if (!game_.unitsAt(i, f, Domain::Space).empty()) {
                theirs = i;
                enemyFaction = f;
                break;
            }
        }
    }
    if (mine == kInvalid || theirs == kInvalid) return;

    battleSetup_ = BattleSetup{};
    battleSetup_.domain = Domain::Space;
    battleSetup_.planet = theirs;
    battleSetup_.attacker = me;
    battleSetup_.defender = enemyFaction;
    battleSetup_.attackerUnits = game_.unitsAt(mine, me, Domain::Space);
    battleSetup_.defenderUnits = game_.unitsAt(theirs, enemyFaction, Domain::Space);
    battle_.init(game_, battleSetup_, 4242);
    fitBattleView();
    screen_ = Screen::Battle;
}

void App::handleEvents() {
    input_.newFrame();
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_QUIT:
                running_ = false;
                break;
            case SDL_MOUSEMOTION:
                input_.mouseX = e.motion.x;
                input_.mouseY = e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    input_.mouseDown = true;
                    input_.dragStartX = e.button.x;
                    input_.dragStartY = e.button.y;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    input_.rightDown = true;
                }
                input_.mouseX = e.button.x;
                input_.mouseY = e.button.y;
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    input_.mouseDown = false;
                    input_.mouseClicked = true;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    input_.rightDown = false;
                    input_.rightClicked = true;
                }
                input_.mouseX = e.button.x;
                input_.mouseY = e.button.y;
                break;
            case SDL_MOUSEWHEEL:
                input_.wheel += e.wheel.y;
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat == 0) input_.keysPressed.push_back(e.key.keysym.sym);
                break;
            default:
                break;
        }
    }
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    input_.shift = keys[SDL_SCANCODE_LSHIFT] != 0 || keys[SDL_SCANCODE_RSHIFT] != 0;
    input_.ctrl = keys[SDL_SCANCODE_LCTRL] != 0 || keys[SDL_SCANCODE_RCTRL] != 0;
}

void App::update(float dt) {
    if (statusTimer_ > 0.0f) statusTimer_ -= dt;

    switch (screen_) {
        case Screen::Menu:
            break;
        case Screen::Galaxy:
            updateGalaxy(dt);
            if (game_.outcome() != GameOutcome::InProgress) screen_ = Screen::GameOver;
            break;
        case Screen::Battle:
            updateBattle(dt);
            break;
        case Screen::Summary:
        case Screen::GameOver:
            break;
    }
}

void App::draw() {
    gfx_.beginFrame(pal::kBackground);
    switch (screen_) {
        case Screen::Menu: drawMenu(); break;
        case Screen::Galaxy: drawGalaxy(); break;
        case Screen::Battle: drawBattle(); break;
        case Screen::Summary: drawSummary(); break;
        case Screen::GameOver: drawGameOver(); break;
    }
    gfx_.endFrame();
}

void App::setStatus(const std::string& message) {
    status_ = message;
    statusTimer_ = 4.0f;
}

// ---------------------------------------------------------------------------
// Battle transitions
// ---------------------------------------------------------------------------
void App::startTacticalBattle() {
    if (!game_.takePendingBattle(battleSetup_)) return;
    battle_.init(game_, battleSetup_, game_.rng().next());
    battleSelection_.clear();
    boxSelecting_ = false;
    battlePaused_ = false;
    battleSpeed_ = 1.0f;
    battleCamera_ = battle_.fieldSize() * 0.5f;
    fitBattleView();
    screen_ = Screen::Battle;
}

void App::fitBattleView() {
    Vec2 field = battle_.fieldSize();
    float viewW = static_cast<float>(gfx_.width());
    float viewH = static_cast<float>(gfx_.height()) - 152.0f;  // bar + footer
    battleZoom_ = std::min(viewW / std::max(1.0f, field.x), viewH / std::max(1.0f, field.y)) * 0.98f;
    battleZoom_ = std::max(0.3f, std::min(2.5f, battleZoom_));
    battleCamera_ = field * 0.5f;
}

void App::finishBattle(const BattleResolution& res) {
    game_.applyBattleResolution(battleSetup_, res);
    lastReport_ = res.report;
    haveReport_ = true;
    screen_ = Screen::Summary;
}

// ---------------------------------------------------------------------------
// Summary and end of game
// ---------------------------------------------------------------------------
void App::drawSummary() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    gfx_.rect(Rect{0, 0, w, h}, Color(8, 12, 22));

    Rect frame{w * 0.5f - 560, 60, 1120, h - 140};
    gfx_.panel(frame, pal::kPanel, pal::kBorderBright);

    const BattleReport& r = lastReport_;
    std::string title = std::string(r.domain == Domain::Space ? "SPACE BATTLE" : "GROUND BATTLE") +
                        " OVER " + r.planetName;
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + 18, title, pal::kText, 3);
    std::string subtitle = std::string(factionShortName(r.victor)) + " VICTORY";
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + 52, subtitle, pal::faction(r.victor), 2);
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + 74,
                     r.autoResolved ? "resolved by fleet command" : "fought in person", pal::kTextDim, 1);

    auto drawSide = [&](const SideSummary& side, float x, float width, bool attacker) {
        Rect col{x, frame.y + 100, width, frame.h - 170};
        gfx_.panel(col, pal::kPanelLight, pal::kBorder);
        Color fc = pal::faction(side.faction);
        gfx_.text(col.x + 12, col.y + 10, std::string(attacker ? "ATTACKER: " : "DEFENDER: ") +
                                              factionShortName(side.faction), fc, 2);
        float y = col.y + 40;
        gfx_.text(col.x + 12, y, "UNIT TYPE", pal::kTextDim, 1);
        gfx_.text(col.x + width - 210, y, "SENT", pal::kTextDim, 1);
        gfx_.text(col.x + width - 150, y, "LOST", pal::kTextDim, 1);
        gfx_.text(col.x + width - 90, y, "LEFT", pal::kTextDim, 1);
        y += 16;
        gfx_.line(col.x + 8, y, col.right() - 8, y, pal::kBorder);
        y += 8;
        for (const SummaryEntry& e : side.entries) {
            const UnitDef& d = db().unit(e.defId);
            gfx_.text(col.x + 12, y, d.name.substr(0, 30), pal::kText, 1);
            gfx_.text(col.x + width - 210, y, std::to_string(e.committed), pal::kText, 1);
            gfx_.text(col.x + width - 150, y, std::to_string(e.lost),
                      e.lost > 0 ? pal::kDanger : pal::kTextDim, 1);
            gfx_.text(col.x + width - 90, y, std::to_string(e.survived),
                      e.survived > 0 ? pal::kGood : pal::kTextDim, 1);
            if (e.retreated > 0) {
                gfx_.text(col.x + width - 40, y, "(" + std::to_string(e.retreated) + "R)",
                          pal::kWarning, 1);
            }
            y += 15;
            if (y > col.bottom() - 70) break;
        }
        float footer = col.bottom() - 58;
        gfx_.line(col.x + 8, footer - 8, col.right() - 8, footer - 8, pal::kBorder);
        gfx_.text(col.x + 12, footer, "COMMITTED " + std::to_string(side.committed), pal::kText, 1);
        gfx_.text(col.x + 12, footer + 15, "DESTROYED " + std::to_string(side.lost), pal::kDanger, 1);
        gfx_.text(col.x + 12, footer + 30,
                  "SURVIVED " + std::to_string(side.survived) + "  (WITHDREW " +
                      std::to_string(side.retreated) + ")",
                  pal::kGood, 1);
        gfx_.textRight(col.right() - 12, footer + 15,
                       "LOSSES " + std::to_string(side.creditsLost) + " CR", pal::kWarning, 1);
    };

    drawSide(r.attackerSide, frame.x + 20, frame.w * 0.5f - 30, true);
    drawSide(r.defenderSide, frame.x + frame.w * 0.5f + 10, frame.w * 0.5f - 30, false);

    if (r.structuresDestroyed > 0) {
        gfx_.textCentred(frame.x + frame.w * 0.5f, frame.bottom() - 62,
                         std::to_string(r.structuresDestroyed) + " defence structures destroyed",
                         pal::kWarning, 1);
    }

    Rect cont{frame.x + frame.w * 0.5f - 110, frame.bottom() - 46, 220, 34};
    if (button(gfx_, input_, cont, "CONTINUE") || input_.keyPressed(SDLK_RETURN) ||
        input_.keyPressed(SDLK_SPACE)) {
        screen_ = game_.outcome() == GameOutcome::InProgress ? Screen::Galaxy : Screen::GameOver;
    }
}

void App::drawGameOver() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    gfx_.rect(Rect{0, 0, w, h}, Color(8, 12, 22));

    bool won = game_.outcome() == GameOutcome::Victory;
    gfx_.textCentred(w * 0.5f, h * 0.32f, won ? "TOTAL VICTORY" : "THE WAR IS LOST",
                     won ? pal::kGood : pal::kDanger, 5);
    gfx_.textCentred(w * 0.5f, h * 0.32f + 60,
                     won ? "Every world in the galaxy flies your banner."
                         : "Your faction has been driven from the galaxy.",
                     pal::kText, 2);

    std::string tally;
    for (Faction f : playableFactions()) {
        tally += std::string(factionShortName(f)) + " " + std::to_string(game_.planetsOwned(f)) + "   ";
    }
    gfx_.textCentred(w * 0.5f, h * 0.32f + 100, tally, pal::kTextDim, 2);
    gfx_.textCentred(w * 0.5f, h * 0.32f + 130, "Campaign length: " + game_.date().toString(),
                     pal::kTextDim, 1);

    Rect menu{w * 0.5f - 120, h * 0.58f, 240, 36};
    if (button(gfx_, input_, menu, "MAIN MENU")) {
        screen_ = Screen::Menu;
    }
    Rect quit{w * 0.5f - 120, h * 0.58f + 46, 240, 36};
    if (button(gfx_, input_, quit, "QUIT")) running_ = false;
}

}  // namespace ui
}  // namespace gc
