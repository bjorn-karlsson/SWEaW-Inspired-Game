#include <cmath>
#include <string>
#include <vector>

#include "ui/App.h"

namespace gc {
namespace ui {

namespace {

/// A quiet parallax star field so the menu is not a flat colour.
void drawStarfield(Gfx& g, float t) {
    const int w = g.width();
    const int h = g.height();
    uint32_t seed = 12345;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    for (int i = 0; i < 420; ++i) {
        float x = rnd() * static_cast<float>(w);
        float y = rnd() * static_cast<float>(h);
        float brightness = 0.25f + rnd() * 0.75f;
        float twinkle = 0.75f + 0.25f * std::sin(t * (0.6f + rnd()) + static_cast<float>(i));
        int v = static_cast<int>(200.0f * brightness * twinkle);
        float size = brightness > 0.85f ? g.s(2.0f) : g.s(1.0f);
        g.rect(Rect{x, y, size, size},
               Color(v, v, static_cast<int>(v * 1.1f) > 255 ? 255 : static_cast<int>(v * 1.1f)));
    }
}

const char* factionBlurb(Faction f) {
    switch (f) {
        case Faction::Republic:
            return "Clone legions, Jedi generals and Kuat-built Star Destroyers. Strong "
                   "economy, excellent capital ships, expensive infantry.";
        case Faction::CIS:
            return "Endless droid armies and monstrous battleships. Cheap ground forces, "
                   "huge carriers, weaker per-unit quality.";
        case Faction::Hutts:
            return "Credits first, honour never. Superb income, cheap mercenaries and "
                   "salvaged warships bought on the black market.";
        default:
            return "";
    }
}

}  // namespace

void App::drawMenu() {
    const float w = static_cast<float>(gfx_.width());
    const float h = static_cast<float>(gfx_.height());
    static float t = 0.0f;
    t += 0.016f;
    drawStarfield(gfx_, t);

    gfx_.textCentred(w * 0.5f, S(56), "GALACTIC CONQUEST", pal::kAccent, F(6));
    gfx_.textCentred(w * 0.5f, S(56) + lineH(6) + S(10),
                     "A STAR WARS: EMPIRE AT WAR INSPIRED REAL TIME STRATEGY", pal::kTextDim, F(2));

    const std::vector<CampaignDef>& campaigns = db().campaigns();
    const std::vector<Faction>& factions = playableFactions();
    menuCampaign_ = std::max(0, std::min(menuCampaign_, static_cast<int>(campaigns.size()) - 1));
    menuFaction_ = std::max(0, std::min(menuFaction_, static_cast<int>(factions.size()) - 1));
    menuDifficulty_ = std::max(0, std::min(menuDifficulty_, static_cast<int>(Difficulty::Count) - 1));

    // Three columns, sized from the window so they always fit.
    const float gap = S(20);
    const float colW = std::min(S(420.0f), (w - gap * 4.0f) / 3.0f);
    const float left = w * 0.5f - colW * 1.5f - gap;
    const float top = S(180);
    const float colH = h - top - S(160);

    // --- campaign column ---
    Rect campaignPanel{left, top, colW, colH};
    gfx_.panel(campaignPanel, pal::kPanel, pal::kBorder);
    gfx_.text(campaignPanel.x + S(14), campaignPanel.y + S(12), "GALACTIC CAMPAIGN", pal::kAccent, F(2));
    float y = campaignPanel.y + S(46);
    for (size_t i = 0; i < campaigns.size(); ++i) {
        Rect r{campaignPanel.x + S(12), y, campaignPanel.w - S(24), S(34)};
        if (toggleButton(gfx_, input_, r, campaigns[i].name, menuCampaign_ == static_cast<int>(i))) {
            menuCampaign_ = static_cast<int>(i);
        }
        y += S(40);
    }
    y += S(10);
    const CampaignDef& c = campaigns[static_cast<size_t>(menuCampaign_)];
    y += wrappedText(gfx_, Rect{campaignPanel.x + S(14), y, campaignPanel.w - S(28), S(100)}, c.description,
                     pal::kTextDim, F(1)) +
         S(16);

    int planetCount = c.planetKeys.empty() ? static_cast<int>(db().planets().size())
                                           : static_cast<int>(c.planetKeys.size());
    gfx_.text(campaignPanel.x + S(14), y, "WORLDS: " + std::to_string(planetCount), pal::kText, F(1));
    y += lineH(1) + S(10);
    for (const FactionStart& s : c.starts) {
        gfx_.text(campaignPanel.x + S(14), y,
                  std::string(factionShortName(s.faction)) + ": " + std::to_string(s.planets.size()) +
                      " worlds, " + std::to_string(s.credits) + " CR",
                  pal::faction(s.faction), F(1));
        y += lineH(1) + S(5);
    }

    // --- faction column ---
    Rect factionPanel{left + colW + gap, top, colW, colH};
    gfx_.panel(factionPanel, pal::kPanel, pal::kBorder);
    gfx_.text(factionPanel.x + S(14), factionPanel.y + S(12), "CHOOSE YOUR FACTION", pal::kAccent, F(2));
    y = factionPanel.y + S(46);
    for (size_t i = 0; i < factions.size(); ++i) {
        Rect r{factionPanel.x + S(12), y, factionPanel.w - S(24), S(40)};
        bool active = menuFaction_ == static_cast<int>(i);
        if (toggleButton(gfx_, input_, r, factionName(factions[i]), active)) {
            menuFaction_ = static_cast<int>(i);
        }
        gfx_.rect(Rect{r.x + S(4), r.y + S(4), S(5), r.h - S(8)}, pal::faction(factions[i]));
        y += S(46);
    }
    y += S(12);
    Faction chosen = factions[static_cast<size_t>(menuFaction_)];
    gfx_.text(factionPanel.x + S(14), y, factionShortName(chosen), pal::faction(chosen), F(2));
    y += lineH(2) + S(8);
    y += wrappedText(gfx_, Rect{factionPanel.x + S(14), y, factionPanel.w - S(28), S(120)},
                     factionBlurb(chosen), pal::kTextDim, F(1)) +
         S(16);
    int unitCount = 0, techCount = 0;
    for (const UnitDef& u : db().units()) {
        if (u.faction == chosen) ++unitCount;
    }
    for (const TechDef& tech : db().techs()) {
        if (tech.faction == chosen) ++techCount;
    }
    gfx_.text(factionPanel.x + S(14), y, "UNIT TYPES: " + std::to_string(unitCount), pal::kText, F(1));
    gfx_.text(factionPanel.x + S(14), y + lineH(1) + S(5), "TECHNOLOGIES: " + std::to_string(techCount),
              pal::kText, F(1));

    // --- difficulty / start column ---
    Rect optionsPanel{left + (colW + gap) * 2, top, colW, colH};
    gfx_.panel(optionsPanel, pal::kPanel, pal::kBorder);
    gfx_.text(optionsPanel.x + S(14), optionsPanel.y + S(12), "DIFFICULTY", pal::kAccent, F(2));
    y = optionsPanel.y + S(46);
    for (int i = 0; i < static_cast<int>(Difficulty::Count); ++i) {
        Rect r{optionsPanel.x + S(12), y, optionsPanel.w - S(24), S(34)};
        Difficulty d = static_cast<Difficulty>(i);
        if (toggleButton(gfx_, input_, r, difficultyName(d), menuDifficulty_ == i)) {
            menuDifficulty_ = i;
        }
        y += S(40);
    }
    y += S(8);
    Difficulty d = static_cast<Difficulty>(menuDifficulty_);
    gfx_.text(optionsPanel.x + S(14), y, "AI INCOME  x" + std::to_string(difficultyAiIncomeMult(d)).substr(0, 4),
              pal::kTextDim, F(1));
    gfx_.text(optionsPanel.x + S(14), y + lineH(1) + S(5),
              "AI COMBAT  x" + std::to_string(difficultyAiCombatMult(d)).substr(0, 4), pal::kTextDim, F(1));
    gfx_.text(optionsPanel.x + S(14), y + (lineH(1) + S(5)) * 2.0f,
              "AGGRESSION x" + std::to_string(difficultyAiAggression(d)).substr(0, 4), pal::kTextDim, F(1));
    y += (lineH(1) + S(5)) * 3.0f + S(16);

    gfx_.text(optionsPanel.x + S(14), y, "GALAXY SEED", pal::kAccent, F(2));
    y += lineH(2) + S(10);
    Rect seedBox{optionsPanel.x + S(12), y, optionsPanel.w - S(130), S(30)};
    gfx_.panel(seedBox, pal::kPanelLight, pal::kBorder);
    gfx_.text(seedBox.x + S(10), seedBox.y + (seedBox.h - lineH(2)) * 0.5f, std::to_string(menuSeed_),
              pal::kText, F(2));
    Rect reroll{optionsPanel.right() - S(110), y, S(98), S(30)};
    if (button(gfx_, input_, reroll, "REROLL")) {
        menuSeed_ = (menuSeed_ * 6364136223846793005ull + 1442695040888963407ull) % 100000000ull;
    }
    y += S(46);
    wrappedText(gfx_, Rect{optionsPanel.x + S(14), y, optionsPanel.w - S(28), S(80)},
                "The seed drives autoresolve rolls and AI decisions. The same seed always "
                "produces the same campaign.",
                pal::kTextDim, F(1));

    // --- start / quit ---
    Rect start{w * 0.5f - S(200), h - S(130), S(400), S(48)};
    ButtonStyle big;
    big.textScale = F(3);
    if (button(gfx_, input_, start, "BEGIN CAMPAIGN", true, big) || input_.keyPressed(SDLK_RETURN)) {
        GameSetup s;
        s.campaign = campaigns[static_cast<size_t>(menuCampaign_)].id;
        s.playerFaction = factions[static_cast<size_t>(menuFaction_)];
        s.difficulty = static_cast<Difficulty>(menuDifficulty_);
        s.seed = menuSeed_;
        game_.start(s);
        selectedPlanet_ = kInvalid;
        selectedUnits_.clear();
        haveReport_ = false;
        category_ = 0;
        zoom_ = 1.4f;
        centreCameraOnHomeworld();
        screen_ = Screen::Galaxy;
        setStatus("Campaign started. Space bar pauses, F1 shows the controls.");
    }
    Rect quit{w * 0.5f - S(200), h - S(74), S(400), S(34)};
    if (button(gfx_, input_, quit, "QUIT") || input_.keyPressed(SDLK_ESCAPE)) running_ = false;

    gfx_.textCentred(w * 0.5f, h - S(28),
                     "F11 full screen.  Fan project: Star Wars and Empire at War are trademarks of "
                     "their owners.",
                     pal::kTextDim, F(1));
}

}  // namespace ui
}  // namespace gc
