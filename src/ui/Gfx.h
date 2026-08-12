#pragma once

#include <SDL2/SDL.h>

#include <string>
#include <vector>

#include "core/Types.h"
#include "core/Vec2.h"

namespace gc {
namespace ui {

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    Color() = default;
    Color(int r_, int g_, int b_, int a_ = 255)
        : r(static_cast<uint8_t>(r_)), g(static_cast<uint8_t>(g_)), b(static_cast<uint8_t>(b_)),
          a(static_cast<uint8_t>(a_)) {}
    Color withAlpha(int a_) const { return Color(r, g, b, a_); }
    Color scaled(float f) const {
        return Color(static_cast<int>(r * f), static_cast<int>(g * f), static_cast<int>(b * f), a);
    }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Rect inset(float d) const { return Rect{x + d, y + d, w - 2 * d, h - 2 * d}; }
};

// The palette: dark blue-grey panels with a cyan HUD, faction colours picked
// to stay readable against the star field.
namespace pal {
extern const Color kBackground;
extern const Color kPanel;
extern const Color kPanelLight;
extern const Color kBorder;
extern const Color kBorderBright;
extern const Color kText;
extern const Color kTextDim;
extern const Color kAccent;
extern const Color kWarning;
extern const Color kDanger;
extern const Color kGood;
extern const Color kLane;
extern const Color kHyperlane;
Color faction(Faction f);
}  // namespace pal

/// Mouse and keyboard state for one frame, shared by the immediate-mode widgets.
struct Input {
    int mouseX = 0, mouseY = 0;
    int wheel = 0;
    bool mouseDown = false;      ///< Left button held.
    bool mouseClicked = false;   ///< Left button released over the same frame.
    bool rightClicked = false;
    bool rightDown = false;
    bool middleDown = false;     ///< Middle button held: drag-pans the camera.
    float dragDeltaX = 0.0f;     ///< Middle-button movement this frame.
    float dragDeltaY = 0.0f;
    bool shift = false;
    bool ctrl = false;
    std::string typed;      ///< Characters entered this frame.
    bool backspace = false; ///< Backspace was pressed this frame.
    int dragStartX = 0, dragStartY = 0;
    bool dragging = false;
    std::vector<SDL_Keycode> keysPressed;

    bool keyPressed(SDL_Keycode k) const;
    void newFrame();
};

/// System cursors the HUD can ask for. Numeric order is priority: whichever
/// requested cursor ranks highest wins for the frame.
enum class CursorKind { Arrow = 0, Hand = 1, Move = 2 };

/// Thin SDL2 wrapper: primitives, bitmap text and a handful of widgets.
class Gfx {
public:
    bool init(const char* title, int width, int height);
    void shutdown();
    void toggleFullscreen();
    bool fullscreen() const { return fullscreen_; }

    /// Asks for a cursor shape this frame. Widgets and drag/pan logic call
    /// this freely; the highest-priority request wins and is applied once,
    /// at the end of the frame, so a button drawn under the dragged cargo
    /// can't steal the cursor back from the drag itself.
    void requestCursor(CursorKind kind);
    /// Clears the frame's cursor request back to the default arrow. Called
    /// once per frame before any widgets run.
    void resetCursorRequest() { pendingCursor_ = CursorKind::Arrow; }

    /// How much bigger than the 1600x900 reference layout this window is.
    /// Every panel, button and font size is multiplied by it, so the HUD keeps
    /// the same proportions on a laptop panel and on a 4K monitor.
    float uiScale() const { return uiScale_; }
    /// Scales a reference-layout length.
    float s(float referenceLength) const { return referenceLength * uiScale_; }
    /// Scales a bitmap font size; always a whole number so glyphs stay crisp.
    int fontScale(int referenceScale) const;

    SDL_Renderer* renderer() const { return renderer_; }
    SDL_Window* window() const { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }
    void updateSize();

    void beginFrame(Color clear);
    void endFrame();

    void rect(const Rect& r, Color c);
    void rectOutline(const Rect& r, Color c, int thickness = 1);
    void panel(const Rect& r, Color fill, Color border);
    void line(float x1, float y1, float x2, float y2, Color c);
    void thickLine(float x1, float y1, float x2, float y2, float w, Color c);
    void circle(float cx, float cy, float radius, Color c);
    void circleOutline(float cx, float cy, float radius, Color c);
    void triangle(Vec2 a, Vec2 b, Vec2 cc, Color col);

    void text(float x, float y, const std::string& s, Color c, int scale = 2);
    void textCentred(float cx, float y, const std::string& s, Color c, int scale = 2);
    void textRight(float rightX, float y, const std::string& s, Color c, int scale = 2);
    static int textWidth(const std::string& s, int scale = 2);
    static int textHeight(int scale = 2) { return kGlyphHeightPixels * scale; }

    /// Clipping helpers so panels never bleed into the map.
    void pushClip(const Rect& r);
    void popClip();

    /// Writes the current frame to a .bmp file. Used by --screenshot.
    bool saveScreenshot(const std::string& path) const;

private:
    static constexpr int kGlyphHeightPixels = 7;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    float uiScale_ = 1.0f;
    bool fullscreen_ = false;
    std::vector<SDL_Rect> clipStack_;

    SDL_Cursor* cursors_[3] = {nullptr, nullptr, nullptr};
    CursorKind pendingCursor_ = CursorKind::Arrow;
    CursorKind appliedCursor_ = CursorKind::Arrow;
};

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------
struct ButtonStyle {
    Color fill{28, 40, 58};
    Color fillHover{44, 66, 92};
    Color fillDisabled{22, 28, 36};
    Color border{70, 110, 150};
    Color text{210, 232, 255};
    Color textDisabled{100, 110, 122};
    int textScale = 2;
};

bool button(Gfx& g, const Input& in, const Rect& r, const std::string& label, bool enabled = true,
            const ButtonStyle& style = ButtonStyle{});
/// A button that stays lit while `active` is true (speed controls, tabs).
bool toggleButton(Gfx& g, const Input& in, const Rect& r, const std::string& label, bool active,
                  bool enabled = true);
void progressBar(Gfx& g, const Rect& r, float fraction, Color fill, Color background);

// --- editor widgets, used by the unit designer ---
/// A focusable text box. `focused` is owned by the caller so it can keep one
/// field active across frames; returns true when the value changed.
bool textField(Gfx& g, const Input& in, const Rect& r, std::string& value, bool focused, int scale);
/// Stepper with a typed value. Returns true when the value changed.
bool numberField(Gfx& g, const Input& in, const Rect& r, float& value, float step, float lo, float hi,
                 bool focused, std::string& editing, int scale);
/// [<] name [>] cycler over a list of names.
bool enumField(Gfx& g, const Input& in, const Rect& r, int& value, const char* const* names, int count,
               int scale);
/// The closed half of a dropdown: current value plus a chevron. Returns true
/// when it is clicked, which the caller turns into "open" or "close".
bool dropdownBox(Gfx& g, const Input& in, const Rect& r, const std::string& value, bool open, int scale);
/// The open half, drawn over everything else. Returns the index the player
/// picked, -2 while the list is still up, or -1 if they clicked away from it.
int dropdownList(Gfx& g, const Input& in, const Rect& anchor, const char* const* names, int count,
                 int current, int scale);
/// Draws `text` wrapped to the width of `r`; returns the height used.
float wrappedText(Gfx& g, const Rect& r, const std::string& s, Color c, int scale = 1);

}  // namespace ui
}  // namespace gc
