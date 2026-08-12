#include "ui/Gfx.h"

#include <algorithm>
#include <cmath>

#include "ui/Font.h"

namespace gc {
namespace ui {

namespace pal {
const Color kBackground{6, 9, 18};
const Color kPanel{16, 22, 34, 242};
const Color kPanelLight{26, 36, 52, 242};
const Color kBorder{58, 88, 122};
const Color kBorderBright{96, 156, 206};
const Color kText{214, 232, 250};
const Color kTextDim{132, 154, 178};
const Color kAccent{96, 200, 255};
const Color kWarning{240, 190, 80};
const Color kDanger{228, 84, 74};
const Color kGood{120, 214, 132};
const Color kLane{54, 74, 104};
const Color kHyperlane{92, 148, 210};

Color faction(Faction f) {
    switch (f) {
        case Faction::Republic: return Color(226, 84, 74);
        case Faction::CIS: return Color(86, 156, 255);
        case Faction::Hutts: return Color(226, 186, 78);
        default: return Color(150, 150, 156);
    }
}
}  // namespace pal

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool Input::keyPressed(SDL_Keycode k) const {
    return std::find(keysPressed.begin(), keysPressed.end(), k) != keysPressed.end();
}

void Input::newFrame() {
    typed.clear();
    backspace = false;
    wheel = 0;
    mouseClicked = false;
    rightClicked = false;
    dragDeltaX = 0.0f;
    dragDeltaY = 0.0f;
    keysPressed.clear();
}

// ---------------------------------------------------------------------------
// Gfx
// ---------------------------------------------------------------------------
bool Gfx::init(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // Open at a size that actually fits the display: 80% of the desktop, never
    // larger than the screen itself.
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0 && usable.w > 320 && usable.h > 240) {
        int fitW = static_cast<int>(static_cast<float>(usable.w) * 0.86f);
        int fitH = static_cast<int>(static_cast<float>(usable.h) * 0.86f);
        width = std::max(1024, std::min(width > fitW ? fitW : width, usable.w));
        height = std::max(600, std::min(height > fitH ? fitH : height, usable.h));
    }
    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) return false;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer_ == nullptr) return false;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    width_ = width;
    height_ = height;
    updateSize();
    return true;
}

void Gfx::toggleFullscreen() {
    if (window_ == nullptr) return;
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(window_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    updateSize();
}

int Gfx::fontScale(int referenceScale) const {
    int scaled = static_cast<int>(std::lround(static_cast<float>(referenceScale) * uiScale_));
    return std::max(1, scaled);
}

void Gfx::shutdown() {
    if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
    if (window_ != nullptr) SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
    SDL_Quit();
}

void Gfx::updateSize() {
    if (renderer_ != nullptr) {
        SDL_GetRendererOutputSize(renderer_, &width_, &height_);
    } else if (window_ != nullptr) {
        SDL_GetWindowSize(window_, &width_, &height_);
    }
    // The HUD is authored against 1600x900. Take the smaller of the two axes
    // so nothing ever runs off the edge of a narrow or a short window.
    float byHeight = static_cast<float>(height_) / 900.0f;
    float byWidth = static_cast<float>(width_) / 1520.0f;
    uiScale_ = std::max(0.62f, std::min(3.0f, std::min(byHeight, byWidth)));
}

void Gfx::beginFrame(Color clear) {
    updateSize();
    SDL_SetRenderDrawColor(renderer_, clear.r, clear.g, clear.b, 255);
    SDL_RenderClear(renderer_);
}

void Gfx::endFrame() { SDL_RenderPresent(renderer_); }

void Gfx::rect(const Rect& r, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_FRect fr{r.x, r.y, r.w, r.h};
    SDL_RenderFillRectF(renderer_, &fr);
}

void Gfx::rectOutline(const Rect& r, Color c, int thickness) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_FRect fr{r.x + static_cast<float>(i), r.y + static_cast<float>(i),
                     r.w - 2.0f * static_cast<float>(i), r.h - 2.0f * static_cast<float>(i)};
        SDL_RenderDrawRectF(renderer_, &fr);
    }
}

void Gfx::panel(const Rect& r, Color fill, Color border) {
    rect(r, fill);
    rectOutline(r, border);
}

void Gfx::line(float x1, float y1, float x2, float y2, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLineF(renderer_, x1, y1, x2, y2);
}

void Gfx::thickLine(float x1, float y1, float x2, float y2, float w, Color c) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * (w * 0.5f);
    float ny = dx / len * (w * 0.5f);
    const SDL_Color col{c.r, c.g, c.b, c.a};
    SDL_Vertex verts[4];
    verts[0] = SDL_Vertex{SDL_FPoint{x1 + nx, y1 + ny}, col, {0, 0}};
    verts[1] = SDL_Vertex{SDL_FPoint{x2 + nx, y2 + ny}, col, {0, 0}};
    verts[2] = SDL_Vertex{SDL_FPoint{x2 - nx, y2 - ny}, col, {0, 0}};
    verts[3] = SDL_Vertex{SDL_FPoint{x1 - nx, y1 - ny}, col, {0, 0}};
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer_, nullptr, verts, 4, indices, 6);
}

void Gfx::circle(float cx, float cy, float radius, Color c) {
    if (radius <= 0.0f) return;
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    int r = static_cast<int>(radius);
    for (int dy = -r; dy <= r; ++dy) {
        float dx = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(dy * dy)));
        SDL_RenderDrawLineF(renderer_, cx - dx, cy + static_cast<float>(dy), cx + dx,
                            cy + static_cast<float>(dy));
    }
}

void Gfx::circleOutline(float cx, float cy, float radius, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const int segments = std::max(10, static_cast<int>(radius * 2.0f));
    float prevX = cx + radius;
    float prevY = cy;
    for (int i = 1; i <= segments; ++i) {
        float a = 6.28318f * static_cast<float>(i) / static_cast<float>(segments);
        float x = cx + std::cos(a) * radius;
        float y = cy + std::sin(a) * radius;
        SDL_RenderDrawLineF(renderer_, prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }
}

void Gfx::triangle(Vec2 a, Vec2 b, Vec2 cc, Color col) {
    const SDL_Color fc{col.r, col.g, col.b, col.a};
    SDL_Vertex verts[3] = {
        SDL_Vertex{SDL_FPoint{a.x, a.y}, fc, {0, 0}},
        SDL_Vertex{SDL_FPoint{b.x, b.y}, fc, {0, 0}},
        SDL_Vertex{SDL_FPoint{cc.x, cc.y}, fc, {0, 0}},
    };
    SDL_RenderGeometry(renderer_, nullptr, verts, 3, nullptr, 0);
}

void Gfx::text(float x, float y, const std::string& s, Color c, int scale) {
    if (scale < 1) scale = 1;
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    float penX = x;
    const float fs = static_cast<float>(scale);
    for (char ch : s) {
        if (ch == '\n') continue;
        const uint8_t* rows = glyphRows(ch);
        for (int gy = 0; gy < kGlyphH; ++gy) {
            uint8_t mask = rows[gy];
            if (mask == 0) continue;
            int runStart = -1;
            for (int gx = 0; gx <= kGlyphW; ++gx) {
                bool on = gx < kGlyphW && (mask & (1u << (kGlyphW - 1 - gx))) != 0;
                if (on && runStart < 0) {
                    runStart = gx;
                } else if (!on && runStart >= 0) {
                    SDL_FRect px{penX + static_cast<float>(runStart) * fs,
                                 y + static_cast<float>(gy) * fs,
                                 static_cast<float>(gx - runStart) * fs, fs};
                    SDL_RenderFillRectF(renderer_, &px);
                    runStart = -1;
                }
            }
        }
        penX += static_cast<float>(kGlyphAdvance) * fs;
    }
}

void Gfx::textCentred(float cx, float y, const std::string& s, Color c, int scale) {
    text(cx - static_cast<float>(textWidth(s, scale)) * 0.5f, y, s, c, scale);
}

void Gfx::textRight(float rightX, float y, const std::string& s, Color c, int scale) {
    text(rightX - static_cast<float>(textWidth(s, scale)), y, s, c, scale);
}

int Gfx::textWidth(const std::string& s, int scale) {
    if (s.empty()) return 0;
    return (static_cast<int>(s.size()) * kGlyphAdvance - 1) * std::max(1, scale);
}

void Gfx::pushClip(const Rect& r) {
    SDL_Rect sr{static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.w),
                static_cast<int>(r.h)};
    clipStack_.push_back(sr);
    SDL_RenderSetClipRect(renderer_, &sr);
}

void Gfx::popClip() {
    if (!clipStack_.empty()) clipStack_.pop_back();
    if (clipStack_.empty()) {
        SDL_RenderSetClipRect(renderer_, nullptr);
    } else {
        SDL_RenderSetClipRect(renderer_, &clipStack_.back());
    }
}

bool Gfx::saveScreenshot(const std::string& path) const {
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, width_, height_, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) return false;
    bool ok = SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                                   surface->pitch) == 0;
    if (ok) ok = SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------
bool button(Gfx& g, const Input& in, const Rect& r, const std::string& label, bool enabled,
            const ButtonStyle& style) {
    bool hover = enabled && r.contains(static_cast<float>(in.mouseX), static_cast<float>(in.mouseY));
    Color fill = !enabled ? style.fillDisabled : (hover ? style.fillHover : style.fill);
    if (hover && in.mouseDown) fill = fill.scaled(0.75f);
    g.rect(r, fill);
    g.rectOutline(r, enabled ? (hover ? pal::kBorderBright : style.border) : Color(46, 54, 66));
    Color tc = enabled ? style.text : style.textDisabled;

    // Shrink, then trim, so a long label never spills out of its button.
    int scale = std::max(1, style.textScale);
    const float room = r.w - 10.0f;
    while (scale > 1 && static_cast<float>(Gfx::textWidth(label, scale)) > room) --scale;
    std::string shown = label;
    const int charW = 6 * scale;
    if (static_cast<float>(Gfx::textWidth(shown, scale)) > room && charW > 0) {
        size_t fits = static_cast<size_t>(std::max(1.0f, room / static_cast<float>(charW)));
        if (shown.size() > fits) shown = shown.substr(0, fits);
    }
    g.textCentred(r.x + r.w * 0.5f,
                  r.y + (r.h - static_cast<float>(Gfx::textHeight(scale))) * 0.5f, shown, tc, scale);
    return hover && in.mouseClicked;
}

bool toggleButton(Gfx& g, const Input& in, const Rect& r, const std::string& label, bool active,
                  bool enabled) {
    ButtonStyle s;
    if (active) {
        s.fill = Color(46, 88, 124);
        s.fillHover = Color(60, 108, 148);
        s.border = pal::kAccent;
        s.text = Color(240, 250, 255);
    }
    return button(g, in, r, label, enabled, s);
}

void progressBar(Gfx& g, const Rect& r, float fraction, Color fill, Color background) {
    fraction = std::max(0.0f, std::min(1.0f, fraction));
    g.rect(r, background);
    Rect inner{r.x + 1, r.y + 1, (r.w - 2) * fraction, r.h - 2};
    g.rect(inner, fill);
    g.rectOutline(r, pal::kBorder);
}

bool textField(Gfx& g, const Input& in, const Rect& r, std::string& value, bool focused, int scale) {
    bool hover = r.contains(static_cast<float>(in.mouseX), static_cast<float>(in.mouseY));
    g.rect(r, focused ? Color(30, 44, 60) : (hover ? Color(24, 34, 46) : Color(16, 22, 30)));
    g.rectOutline(r, focused ? pal::kAccent : pal::kBorder);

    bool changed = false;
    if (focused) {
        if (!in.typed.empty()) {
            value += in.typed;
            changed = true;
        }
        if (in.backspace && !value.empty()) {
            value.pop_back();
            changed = true;
        }
    }

    // While typing, show the tail so the caret stays visible; otherwise show
    // the beginning, which is what the reader wants to see.
    int room = std::max(1, static_cast<int>((r.w - 10.0f) / (6.0f * static_cast<float>(scale))));
    std::string shown = value;
    if (static_cast<int>(shown.size()) > room) {
        shown = focused ? shown.substr(shown.size() - static_cast<size_t>(room))
                        : shown.substr(0, static_cast<size_t>(std::max(1, room - 2))) + "..";
    }
    float ty = r.y + (r.h - static_cast<float>(Gfx::textHeight(scale))) * 0.5f;
    g.text(r.x + 5.0f, ty, shown, pal::kText, scale);
    if (focused) {
        float caretX = r.x + 5.0f + static_cast<float>(Gfx::textWidth(shown, scale)) + 1.0f;
        g.rect(Rect{caretX, ty, static_cast<float>(scale), static_cast<float>(Gfx::textHeight(scale))},
               pal::kAccent);
    }
    return changed;
}

bool numberField(Gfx& g, const Input& in, const Rect& r, float& value, float step, float lo, float hi,
                 bool focused, std::string& editing, int scale) {
    float bw = std::min(r.h, r.w * 0.22f);
    Rect minus{r.x, r.y, bw, r.h};
    Rect plus{r.right() - bw, r.y, bw, r.h};
    Rect box{r.x + bw + 2.0f, r.y, r.w - bw * 2.0f - 4.0f, r.h};

    bool changed = false;
    ButtonStyle st;
    st.textScale = scale;
    if (button(g, in, minus, "-", true, st)) {
        value = std::max(lo, value - step);
        changed = true;
    }
    if (button(g, in, plus, "+", true, st)) {
        value = std::min(hi, value + step);
        changed = true;
    }

    if (focused) {
        if (textField(g, in, box, editing, true, scale)) {
            try {
                value = std::max(lo, std::min(hi, std::stof(editing)));
                changed = true;
            } catch (...) {
                // Half-typed numbers are fine; keep the old value.
            }
        }
    } else {
        char buf[32];
        if (std::fabs(value - std::round(value)) < 0.001f) {
            std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::lround(value)));
        } else {
            std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
        }
        bool hover = box.contains(static_cast<float>(in.mouseX), static_cast<float>(in.mouseY));
        g.rect(box, hover ? Color(24, 34, 46) : Color(16, 22, 30));
        g.rectOutline(box, pal::kBorder);
        g.textCentred(box.x + box.w * 0.5f,
                      box.y + (box.h - static_cast<float>(Gfx::textHeight(scale))) * 0.5f, buf,
                      pal::kText, scale);
    }
    return changed;
}

bool enumField(Gfx& g, const Input& in, const Rect& r, int& value, const char* const* names, int count,
               int scale) {
    float bw = std::min(r.h, r.w * 0.2f);
    Rect prev{r.x, r.y, bw, r.h};
    Rect next{r.right() - bw, r.y, bw, r.h};
    Rect box{r.x + bw + 2.0f, r.y, r.w - bw * 2.0f - 4.0f, r.h};
    bool changed = false;
    ButtonStyle st;
    st.textScale = scale;
    if (button(g, in, prev, "<", true, st)) {
        value = (value + count - 1) % count;
        changed = true;
    }
    if (button(g, in, next, ">", true, st)) {
        value = (value + 1) % count;
        changed = true;
    }
    g.rect(box, Color(16, 22, 30));
    g.rectOutline(box, pal::kBorder);
    int idx = std::max(0, std::min(count - 1, value));
    g.textCentred(box.x + box.w * 0.5f,
                  box.y + (box.h - static_cast<float>(Gfx::textHeight(scale))) * 0.5f, names[idx],
                  pal::kText, scale);
    return changed;
}

float wrappedText(Gfx& g, const Rect& r, const std::string& s, Color c, int scale) {
    const int charW = kGlyphAdvance * std::max(1, scale);
    const int maxChars = std::max(4, static_cast<int>(r.w) / charW);
    float y = r.y;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t remaining = s.size() - pos;
        size_t take = std::min(static_cast<size_t>(maxChars), remaining);
        if (take < remaining) {
            size_t space = s.rfind(' ', pos + take);
            if (space != std::string::npos && space > pos) take = space - pos;
        }
        g.text(r.x, y, s.substr(pos, take), c, scale);
        y += static_cast<float>(Gfx::textHeight(scale)) + 3.0f;
        pos += take;
        while (pos < s.size() && s[pos] == ' ') ++pos;
        if (y > r.bottom()) break;
    }
    return y - r.y;
}

}  // namespace ui
}  // namespace gc
