#pragma once

#include <string>
#include <vector>

#include "battle/Tactical.h"
#include "sim/GameState.h"
#include "ui/Gfx.h"

namespace gc {
namespace ui {

enum class Screen { Menu, Galaxy, Battle, Summary, GameOver };

/// Command line options. The screenshot and demo switches exist so the client
/// can be smoke-tested without a human at the keyboard.
struct AppOptions {
    bool autostart = false;   ///< Skip the menu and start the default campaign.
    bool demoBattle = false;  ///< Drop straight into a tactical battle.
    bool demoSummary = false; ///< Fight a demo battle to the end and show the report.
    bool demoHud = false;     ///< Grant the player's heroes and pause, for HUD screenshots.
    std::string screenshot;   ///< Write this .bmp file, then exit.
    int screenshotFrame = 40;
    int campaign = 0;
    int faction = 0;
    int difficulty = 1;
};

/// The whole graphical client: menu, galactic conquest map, tactical battles
/// and the post-battle summary.
class App {
public:
    int run(const AppOptions& options = AppOptions{});

private:
    // --- frame plumbing ---
    void startCampaign(const AppOptions& options);
    void startDemoBattle();
    void grantDemoHeroes();
    /// Zoom so the whole battlefield is visible when a battle opens.
    void fitBattleView();
    void handleEvents();
    void update(float dt);
    void draw();
    void setStatus(const std::string& message);

    // --- screens (one file each) ---
    void drawMenu();
    void updateGalaxy(float dt);
    void drawGalaxy();
    void updateBattle(float dt);
    void drawBattle();
    void drawSummary();
    void drawGameOver();

    // --- galaxy helpers ---
    Rect mapViewport() const;
    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;
    void focusOn(Id planet);
    void centreCameraOnHomeworld();
    Id planetAtScreen(float x, float y) const;
    void selectPlanet(Id planet);
    void toggleUnitSelection(Id unitId);
    void selectAllAt(Id planet, Domain domain);
    void issueMoveOrder(Id destination);
    void drawRegionLabels();
    void drawCommandBar();
    void drawMinimap(const Rect& area);
    void drawCategoryGrid(const Rect& area);
    void drawStatusLine(const Rect& area);
    void drawTray(const Rect& area);
    void drawActionCluster(const Rect& area);
    void drawHeroRoster();
    void drawPausedBanner();
    void drawBattlePrompt();
    void drawPlanetTooltip();
    /// Queues a deferred tooltip so it is drawn on top of everything else.
    void queueTooltip(const std::string& title, const std::vector<std::string>& lines);
    void drawQueuedTooltip();

    // --- battle helpers ---
    void startTacticalBattle();
    void finishBattle(const BattleResolution& res);
    Vec2 battleToScreen(Vec2 world) const;
    Vec2 screenToBattle(Vec2 screen) const;

    Gfx gfx_;
    Input input_;
    GameState game_;
    Screen screen_ = Screen::Menu;
    bool running_ = true;

    // Main menu selections.
    int menuCampaign_ = 0;
    int menuFaction_ = 0;
    int menuDifficulty_ = 1;
    uint64_t menuSeed_ = 20240501;

    // Galaxy view.
    Vec2 camera_{500.0f, 550.0f};
    float zoom_ = 1.4f;
    bool panning_ = false;
    int panLastX_ = 0, panLastY_ = 0;
    Id selectedPlanet_ = kInvalid;
    Id hoverPlanet_ = kInvalid;
    std::vector<Id> selectedUnits_;
    int category_ = 0;  ///< Index into the command bar's category grid.
    float trayScroll_ = 0.0f;
    std::string tipTitle_;
    std::vector<std::string> tipLines_;
    std::string status_;
    float statusTimer_ = 0.0f;
    bool showHelp_ = false;

    // Battle.
    tactical::Battle battle_;
    BattleSetup battleSetup_;
    std::vector<Id> battleSelection_;
    bool boxSelecting_ = false;
    Vec2 boxStart_;
    Vec2 battleCamera_{0.0f, 0.0f};
    float battleZoom_ = 0.75f;
    float battleSpeed_ = 1.0f;
    bool battlePaused_ = false;

    // Summary.
    BattleReport lastReport_;
    bool haveReport_ = false;
};

}  // namespace ui
}  // namespace gc
