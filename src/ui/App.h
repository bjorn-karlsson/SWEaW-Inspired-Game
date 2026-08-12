#pragma once

#include <string>
#include <vector>

#include "battle/Tactical.h"
#include "sim/GameState.h"
#include "ui/Gfx.h"

namespace gc {
namespace ui {

enum class Screen { Menu, Galaxy, Battle, Summary, GameOver, Designer };

/// Command line options. The screenshot and demo switches exist so the client
/// can be smoke-tested without a human at the keyboard.
struct AppOptions {
    bool autostart = false;   ///< Skip the menu and start the default campaign.
    bool demoBattle = false;  ///< Drop straight into a tactical battle.
    bool demoSummary = false; ///< Fight a demo battle to the end and show the report.
    bool demoHud = false;     ///< Grant the player's heroes and pause, for HUD screenshots.
    bool fullscreen = false;  ///< Start full screen (F11 toggles it at any time).
    bool demoWorld = false;   ///< Open the world view straight away (screenshots).
    bool demoDesigner = false;///< Open the unit designer straight away (screenshots).
    std::string designerUnit; ///< Unit key the designer should open on.
    int mouseX = -1;          ///< Dev aid: park the pointer here at start-up.
    int mouseY = -1;
    int windowW = 1600;       ///< Requested window size (clamped to the display).
    int windowH = 900;
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
    /// Reference-layout length scaled to this window.
    float S(float referenceLength) const { return gfx_.s(referenceLength); }
    /// Reference font size scaled to this window.
    int F(int referenceScale) const { return gfx_.fontScale(referenceScale); }
    float lineH(int referenceScale) const {
        return static_cast<float>(Gfx::textHeight(F(referenceScale)));
    }

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
    void drawDesigner();
    /// Draws a unit using its designed hull, colours and mounts.
    void drawUnitIcon(const Rect& r, Id unitDefId, Faction owner, bool showHardpoints);
    void openDesigner();
    void closeDesigner();

    // --- galaxy helpers ---
    Rect mapViewport() const;
    Vec2 worldToScreen(Vec2 world) const;
    /// Perspective factor for a point on the tilted galactic plane.
    float perspectiveAt(float worldY) const;
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
    /// Compact fleet and army badges over every world that holds forces: the
    /// three orbital slots collapse into one stack on the star map, and the
    /// ten surface cells into one. Both are drag handles.
    void drawForceBadges();
    /// Registers a world as somewhere a dragged stack can be sent.
    void addMoveTarget(const Rect& r, Id planet);
    /// Starts dragging a whole stack of units.
    void beginStackDrag(const std::vector<Id>& units, bool fromSurface);

    // --- planet (world) view ---
    void enterPlanetView(Id planet);
    void leavePlanetView();
    void updatePlanetTransition(float dt);
    void drawPlanetView();
    /// One draggable unit tile. Returns true when it was clicked (not dragged).
    bool drawUnitCell(const Rect& r, Id unitId, int fromSlot, bool fromSurface, bool compact);
    /// Registers a rectangle as somewhere units can be dropped this frame.
    void addDropTarget(const Rect& r, int slot, bool surface);
    /// Applies whatever the player dropped, once the button comes up.
    void resolveUnitDrop();
    void drawDraggedUnits();
    /// The detailed EaW-style card for a unit or a structure.
    void drawInfoCard(const Rect& anchor, Id defId, bool isUnit);
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
    Id tipUnit_ = kInvalid;      ///< Unit def shown as a full info card.
    Id tipBuilding_ = kInvalid;  ///< Structure def shown as a full info card.

    // The world view and the camera dive into it.
    Id planetViewTarget_ = kInvalid;
    float planetViewT_ = 0.0f;  ///< 0 = star map, 1 = world view.
    int planetViewDir_ = 0;     ///< +1 diving in, -1 pulling out.
    Vec2 viewCamera_;           ///< Camera actually used for drawing this frame.
    float viewZoom_ = 1.4f;
    bool inPlanetView() const { return planetViewT_ > 0.999f; }

    /// Dragging units between the orbital slots and the surface.
    struct UnitDrag {
        std::vector<Id> units;
        int fromSlot = -1;
        bool fromSurface = false;
        bool armed = false;    ///< Button down on a unit, not yet a drag.
        bool active = false;   ///< Past the drag threshold.
        Vec2 startPos;
    };
    UnitDrag drag_;
    struct DropTarget {
        Rect rect;
        int slot = -1;
        bool surface = false;
        Id planet = kInvalid;  ///< Set for "move your units to this world" targets.
        bool moveTo = false;
    };
    std::vector<DropTarget> dropTargets_;
    /// Where this frame's fleet and army badges landed, so the nameplates can
    /// step around them instead of printing through them.
    std::vector<Rect> badgeBoxes_;
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

    // Unit designer.
    Id designUnit_ = kInvalid;
    int designFilter_ = 0;
    int designHardpoint_ = -1;
    int designFocus_ = -1;       ///< Which editable field has the keyboard.
    int designWingPick_ = 0;
    float designScroll_ = 0.0f;
    float designPropScroll_ = 0.0f;
    std::string designEdit_;     ///< Text buffer for the focused number field.
    std::string designMessage_;
    Screen designReturn_ = Screen::Menu;
};

}  // namespace ui
}  // namespace gc
