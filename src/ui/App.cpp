#include "ui/App.h"

#include <algorithm>

namespace gc {
namespace ui {

int App::run(const AppOptions& options) {
    if (!gfx_.init("Galactic Conquest - A Star Wars Empire at War inspired RTS", options.windowW,
                   options.windowH)) {
        SDL_Log("Failed to open a window: %s", SDL_GetError());
        return 1;
    }

    if (options.fullscreen) gfx_.toggleFullscreen();
    if (options.mouseX >= 0 && options.mouseY >= 0) {
        SDL_WarpMouseInWindow(gfx_.window(), options.mouseX, options.mouseY);
        input_.mouseX = options.mouseX;
        input_.mouseY = options.mouseY;
    }

    if (options.autostart || options.demoBattle || options.demoSummary || options.demoHud ||
        options.demoWorld) {
        startCampaign(options);
    }
    if (options.demoHud || options.demoWorld) grantDemoHeroes();
    if (options.demoWorld) {
        enterPlanetView(selectedPlanet_);
        planetViewT_ = 1.0f;
        planetViewDir_ = 0;
        updatePlanetTransition(0.0f);
    }
    if (options.demoDesigner) {
        designReturn_ = Screen::Menu;
        if (!options.designerUnit.empty()) {
            Id id = db().unitId(options.designerUnit);
            if (id != kInvalid) designUnit_ = id;
        }
        openDesigner();
    }
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
    // Running the queue out can drag the player into a border skirmish, and
    // its prompt would cover the screen we are trying to photograph.
    int guard = 0;
    while (game_.hasPendingPlayerBattle() && guard++ < 32) game_.autoResolvePendingBattle();
    haveReport_ = false;
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
                if (input_.middleDown) {
                    input_.dragDeltaX += static_cast<float>(e.motion.xrel);
                    input_.dragDeltaY += static_cast<float>(e.motion.yrel);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    input_.mouseDown = true;
                    input_.dragStartX = e.button.x;
                    input_.dragStartY = e.button.y;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    input_.rightDown = true;
                } else if (e.button.button == SDL_BUTTON_MIDDLE) {
                    input_.middleDown = true;
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
                } else if (e.button.button == SDL_BUTTON_MIDDLE) {
                    input_.middleDown = false;
                }
                input_.mouseX = e.button.x;
                input_.mouseY = e.button.y;
                break;
            case SDL_MOUSEWHEEL:
                input_.wheel += e.wheel.y;
                break;
            case SDL_TEXTINPUT:
                input_.typed += e.text.text;
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_BACKSPACE) input_.backspace = true;
                if (e.key.repeat == 0) {
                    input_.keysPressed.push_back(e.key.keysym.sym);
                    bool altEnter = e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT) != 0;
                    if (e.key.keysym.sym == SDLK_F11 || altEnter) gfx_.toggleFullscreen();
                }
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
    // Cleared here, at the top of the frame, so both the update phase (drag
    // and pan state) and the draw phase (button and tile hovers) get to ask
    // for a cursor before it is applied once in Gfx::endFrame().
    gfx_.resetCursorRequest();

    for (auto it = statusQueue_.begin(); it != statusQueue_.end();) {
        it->age += dt;
        if (it->age >= it->life) {
            it = statusQueue_.erase(it);
        } else {
            ++it;
        }
    }

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
        case Screen::Designer:
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
        case Screen::Designer: drawDesigner(); break;
    }
    gfx_.endFrame();
}

void App::setStatus(const std::string& message) {
    // Collapse an immediate repeat (e.g. spamming a disabled button) instead
    // of stacking duplicates, and just refresh its lifetime.
    if (!statusQueue_.empty() && statusQueue_.back().text == message) {
        statusQueue_.back().age = 0.0f;
        return;
    }
    statusQueue_.push_back(StatusMessage{message, 0.0f, 4.0f});
    // Keep the stack readable: drop the oldest once we have more than a
    // handful on screen at once.
    constexpr size_t kMaxToasts = 4;
    if (statusQueue_.size() > kMaxToasts) statusQueue_.erase(statusQueue_.begin());
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
    float viewH = static_cast<float>(gfx_.height()) - S(152.0f);  // bar + footer

    // Frame the combatants rather than the whole field: a skirmish between two
    // squadrons should not be shown from the far side of the system.
    Vec2 lo(field.x, field.y);
    Vec2 hi(0.0f, 0.0f);
    int present = 0;
    for (const tactical::TUnit& u : battle_.units()) {
        if (!u.alive || u.escaped) continue;
        lo.x = std::min(lo.x, u.pos.x - u.radius);
        lo.y = std::min(lo.y, u.pos.y - u.radius);
        hi.x = std::max(hi.x, u.pos.x + u.radius);
        hi.y = std::max(hi.y, u.pos.y + u.radius);
        ++present;
    }
    if (present == 0) {
        lo = Vec2(0.0f, 0.0f);
        hi = field;
    }
    // Room to manoeuvre around the deployment, so nobody fights off screen.
    float padX = std::max(field.x * 0.06f, (hi.x - lo.x) * 0.20f);
    float padY = std::max(field.y * 0.06f, (hi.y - lo.y) * 0.20f);
    float boxW = std::max(1.0f, (hi.x - lo.x) + padX * 2.0f);
    float boxH = std::max(1.0f, (hi.y - lo.y) + padY * 2.0f);

    battleZoom_ = std::min(viewW / boxW, viewH / boxH);
    battleZoom_ = std::max(0.3f, std::min(3.0f, battleZoom_));
    battleCamera_ = Vec2((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
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

    Rect frame{w * 0.5f - std::min(S(560.0f), w * 0.48f), S(60), std::min(S(1120.0f), w * 0.96f),
               h - S(140)};
    gfx_.panel(frame, pal::kPanel, pal::kBorderBright);

    const BattleReport& r = lastReport_;
    std::string title = std::string(r.domain == Domain::Space ? "SPACE BATTLE" : "GROUND BATTLE") +
                        " OVER " + r.planetName;
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + S(18), title, pal::kText, F(3));
    std::string subtitle = std::string(factionShortName(r.victor)) + " VICTORY";
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + S(18) + lineH(3) + S(8), subtitle,
                     pal::faction(r.victor), F(2));
    gfx_.textCentred(frame.x + frame.w * 0.5f, frame.y + S(18) + lineH(3) + lineH(2) + S(14),
                     r.autoResolved ? "resolved by fleet command" : "fought in person", pal::kTextDim, F(1));

    auto drawSide = [&](const SideSummary& side, float x, float width, bool attacker) {
        Rect col{x, frame.y + S(100), width, frame.h - S(170)};
        gfx_.panel(col, pal::kPanelLight, pal::kBorder);
        Color fc = pal::faction(side.faction);
        gfx_.text(col.x + S(12), col.y + S(10),
                  std::string(attacker ? "ATTACKER: " : "DEFENDER: ") + factionShortName(side.faction), fc,
                  F(2));
        float y = col.y + S(40);
        gfx_.text(col.x + S(12), y, "UNIT TYPE", pal::kTextDim, F(1));
        gfx_.text(col.right() - S(210), y, "SENT", pal::kTextDim, F(1));
        gfx_.text(col.right() - S(150), y, "LOST", pal::kTextDim, F(1));
        gfx_.text(col.right() - S(90), y, "LEFT", pal::kTextDim, F(1));
        y += lineH(1) + S(6);
        gfx_.line(col.x + S(8), y, col.right() - S(8), y, pal::kBorder);
        y += S(8);
        for (const SummaryEntry& e : side.entries) {
            const UnitDef& d = db().unit(e.defId);
            gfx_.text(col.x + S(12), y, d.name.substr(0, 30), pal::kText, F(1));
            gfx_.text(col.right() - S(210), y, std::to_string(e.committed), pal::kText, F(1));
            gfx_.text(col.right() - S(150), y, std::to_string(e.lost),
                      e.lost > 0 ? pal::kDanger : pal::kTextDim, F(1));
            gfx_.text(col.right() - S(90), y, std::to_string(e.survived),
                      e.survived > 0 ? pal::kGood : pal::kTextDim, F(1));
            if (e.retreated > 0) {
                gfx_.text(col.right() - S(40), y, "(" + std::to_string(e.retreated) + "R)", pal::kWarning,
                          F(1));
            }
            y += lineH(1) + S(4);
            if (y > col.bottom() - S(70)) break;
        }
        float footer = col.bottom() - S(58);
        gfx_.line(col.x + S(8), footer - S(8), col.right() - S(8), footer - S(8), pal::kBorder);
        gfx_.text(col.x + S(12), footer, "COMMITTED " + std::to_string(side.committed), pal::kText, F(1));
        gfx_.text(col.x + S(12), footer + lineH(1) + S(4), "DESTROYED " + std::to_string(side.lost),
                  pal::kDanger, F(1));
        gfx_.text(col.x + S(12), footer + (lineH(1) + S(4)) * 2.0f,
                  "SURVIVED " + std::to_string(side.survived) + "  (WITHDREW " +
                      std::to_string(side.retreated) + ")",
                  pal::kGood, F(1));
        gfx_.textRight(col.right() - S(12), footer + lineH(1) + S(4),
                       "LOSSES " + std::to_string(side.creditsLost) + " CR", pal::kWarning, F(1));
    };

    drawSide(r.attackerSide, frame.x + S(20), frame.w * 0.5f - S(30), true);
    drawSide(r.defenderSide, frame.x + frame.w * 0.5f + S(10), frame.w * 0.5f - S(30), false);

    if (r.structuresDestroyed > 0) {
        gfx_.textCentred(frame.x + frame.w * 0.5f, frame.bottom() - S(62),
                         std::to_string(r.structuresDestroyed) + " defence structures destroyed",
                         pal::kWarning, F(1));
    }

    Rect cont{frame.x + frame.w * 0.5f - S(110), frame.bottom() - S(46), S(220), S(34)};
    ButtonStyle st;
    st.textScale = F(2);
    if (button(gfx_, input_, cont, "CONTINUE", true, st) || input_.keyPressed(SDLK_RETURN) ||
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
                     won ? pal::kGood : pal::kDanger, F(5));
    gfx_.textCentred(w * 0.5f, h * 0.32f + lineH(5) + S(16),
                     won ? "Every world in the galaxy flies your banner."
                         : "Your faction has been driven from the galaxy.",
                     pal::kText, F(2));

    std::string tally;
    for (Faction f : playableFactions()) {
        tally += std::string(factionShortName(f)) + " " + std::to_string(game_.planetsOwned(f)) + "   ";
    }
    gfx_.textCentred(w * 0.5f, h * 0.32f + lineH(5) + lineH(2) + S(30), tally, pal::kTextDim, F(2));
    gfx_.textCentred(w * 0.5f, h * 0.32f + lineH(5) + lineH(2) * 2.0f + S(44),
                     "Campaign length: " + game_.date().toString(), pal::kTextDim, F(1));

    ButtonStyle st;
    st.textScale = F(2);
    Rect menu{w * 0.5f - S(120), h * 0.58f, S(240), S(36)};
    if (button(gfx_, input_, menu, "MAIN MENU", true, st)) screen_ = Screen::Menu;
    Rect quit{w * 0.5f - S(120), h * 0.58f + S(46), S(240), S(36)};
    if (button(gfx_, input_, quit, "QUIT", true, st)) running_ = false;
}

}  // namespace ui
}  // namespace gc
