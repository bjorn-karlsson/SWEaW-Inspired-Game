#pragma once

#include <cstdint>

namespace gc {
namespace ui {

/// Width and height of a glyph cell, in font pixels.
constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;
/// One blank column between characters.
constexpr int kGlyphAdvance = kGlyphW + 1;

/// Returns the 7 row bitmasks (bit 4 = leftmost pixel) for a character.
/// The font is upper case only - lower case is folded to upper case, which
/// suits the all-caps look of the HUD. Unknown characters render blank.
const uint8_t* glyphRows(char c);

}  // namespace ui
}  // namespace gc
