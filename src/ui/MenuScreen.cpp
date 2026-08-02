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
        g.rect(Rect{x, y, brightness > 0.85f ? 2.0f : 1.0f, brightness > 0.85f ? 2.0f : 1.0f},
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

    // Title
    gfx_.textCentred(w * 0.5f, 60, "GALACTIC CONQUEST", pal::kAccent, 6);
    gfx_.textCentred(w * 0.5f, 118, "A STAR WARS: EMPIRE AT WAR INSPIRED REAL TIME STRATEGY",
                     pal::kTextDim, 2);

    const std::vector<CampaignDef>& campaigns = db().campaigns();
    const std::vector<Faction>& factions = playableFactions();
    menuCampaign_ = std::max(0, std::min(menuCampaign_, static_cast<int>(campaigns.size()) - 1));
    menuFaction_ = std::max(0, std::min(menuFaction_, static_cast<int>(factions.size()) - 1));
    menuDifficulty_ = std::max(0, std::min(menuDifficulty_, static_cast<int>(Difficulty::Count) - 1));

    const float colW = 420.0f;
    const float left = w * 0.5f - colW * 1.5f - 20.0f;
    const float top = 180.0f;
    const float colH = h - top - 160.0f;

    // --- campaign column ---
    Rect campaignPanel{left, top, colW, colH};
    gfx_.panel(campaignPanel, pal::kPanel, pal::kBorder);
    gfx_.text(campaignPanel.x + 14, campaignPanel.y + 12, "GALACTIC CAMPAIGN", pal::kAccent, 2);
    float y = campaignPanel.y + 46;
    for (size_t i = 0; i < campaigns.size(); ++i) {
        Rect r{campaignPanel.x + 12, y, campaignPanel.w - 24, 34};
        if (toggleButton(gfx_, input_, r, campaigns[i].name, menuCampaign_ == static_cast<int>(i))) {
            menuCampaign_ = static_cast<int>(i);
        }
        y += 40;
    }
    y += 10;
    const CampaignDef& c = campaigns[static_cast<size_t>(menuCampaign_)];
    wrappedText(gfx_, Rect{campaignPanel.x + 14, y, campaignPanel.w - 28, 100}, c.description,
                pal::kTextDim, 1);
    y += 90;

    int planetCount = c.planetKeys.empty() ? static_cast<int>(db().planets().size())
                                           : static_cast<int>(c.planetKeys.size());
    gfx_.text(campaignPanel.x + 14, y, "WORLDS: " + std::to_string(planetCount), pal::kText, 1);
    y += 22;
    for (const FactionStart& s : c.starts) {
        gfx_.text(campaignPanel.x + 14, y,
                  std::string(factionShortName(s.faction)) + ": " + std::to_string(s.planets.size()) +
                      " worlds, " + std::to_string(s.credits) + " CR",
                  pal::faction(s.faction), 1);
        y += 18;
    }

    // --- faction column ---
    Rect factionPanel{left + colW + 20, top, colW, colH};
    gfx_.panel(factionPanel, pal::kPanel, pal::kBorder);
    gfx_.text(factionPanel.x + 14, factionPanel.y + 12, "CHOOSE YOUR FACTION", pal::kAccent, 2);
    y = factionPanel.y + 46;
    for (size_t i = 0; i < factions.size(); ++i) {
        Rect r{factionPanel.x + 12, y, factionPanel.w - 24, 40};
        bool active = menuFaction_ == static_cast<int>(i);
        if (toggleButton(gfx_, input_, r, factionName(factions[i]), active)) {
            menuFaction_ = static_cast<int>(i);
        }
        gfx_.rect(Rect{r.x + 4, r.y + 4, 5, r.h - 8}, pal::faction(factions[i]));
        y += 46;
    }
    y += 12;
    Faction chosen = factions[static_cast<size_t>(menuFaction_)];
    gfx_.text(factionPanel.x + 14, y, factionShortName(chosen), pal::faction(chosen), 2);
    y += 26;
    wrappedText(gfx_, Rect{factionPanel.x + 14, y, factionPanel.w - 28, 120}, factionBlurb(chosen),
                pal::kTextDim, 1);
    y += 90;
    int unitCount = 0, techCount = 0;
    for (const UnitDef& u : db().units()) {
        if (u.faction == chosen) ++unitCount;
    }
    for (const TechDef& tech : db().techs()) {
        if (tech.faction == chosen) ++techCount;
    }
    gfx_.text(factionPanel.x + 14, y, "UNIT TYPES: " + std::to_string(unitCount), pal::kText, 1);
    gfx_.text(factionPanel.x + 14, y + 18, "TECHNOLOGIES: " + std::to_string(techCount), pal::kText, 1);

    // --- difficulty / start column ---
    Rect optionsPanel{left + (colW + 20) * 2, top, colW, colH};
    gfx_.panel(optionsPanel, pal::kPanel, pal::kBorder);
    gfx_.text(optionsPanel.x + 14, optionsPanel.y + 12, "DIFFICULTY", pal::kAccent, 2);
    y = optionsPanel.y + 46;
    for (int i = 0; i < static_cast<int>(Difficulty::Count); ++i) {
        Rect r{optionsPanel.x + 12, y, optionsPanel.w - 24, 34};
        Difficulty d = static_cast<Difficulty>(i);
        if (toggleButton(gfx_, input_, r, difficultyName(d), menuDifficulty_ == i)) {
            menuDifficulty_ = i;
        }
        y += 40;
    }
    y += 8;
    Difficulty d = static_cast<Difficulty>(menuDifficulty_);
    gfx_.text(optionsPanel.x + 14, y,
              "AI INCOME  x" + std::to_string(difficultyAiIncomeMult(d)).substr(0, 4), pal::kTextDim, 1);
    gfx_.text(optionsPanel.x + 14, y + 18,
              "AI COMBAT  x" + std::to_string(difficultyAiCombatMult(d)).substr(0, 4), pal::kTextDim, 1);
    gfx_.text(optionsPanel.x + 14, y + 36,
              "AGGRESSION x" + std::to_string(difficultyAiAggression(d)).substr(0, 4), pal::kTextDim, 1);
    y += 68;

    gfx_.text(optionsPanel.x + 14, y, "GALAXY SEED", pal::kAccent, 2);
    y += 26;
    Rect seedBox{optionsPanel.x + 12, y, optionsPanel.w - 130, 30};
    gfx_.panel(seedBox, pal::kPanelLight, pal::kBorder);
    gfx_.text(seedBox.x + 10, seedBox.y + 8, std::to_string(menuSeed_), pal::kText, 2);
    Rect reroll{optionsPanel.right() - 110, y, 98, 30};
    if (button(gfx_, input_, reroll, "REROLL")) {
        menuSeed_ = (menuSeed_ * 6364136223846793005ull + 1442695040888963407ull) % 100000000ull;
    }
    y += 46;
    wrappedText(gfx_, Rect{optionsPanel.x + 14, y, optionsPanel.w - 28, 80},
                "The seed drives autoresolve rolls and AI decisions. The same seed always "
                "produces the same campaign.",
                pal::kTextDim, 1);

    // --- start / quit ---
    Rect start{w * 0.5f - 200, h - 130, 400, 48};
    if (button(gfx_, input_, start, "BEGIN CAMPAIGN") || input_.keyPressed(SDLK_RETURN)) {
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
    Rect quit{w * 0.5f - 200, h - 74, 400, 34};
    if (button(gfx_, input_, quit, "QUIT") || input_.keyPressed(SDLK_ESCAPE)) running_ = false;

    gfx_.textCentred(w * 0.5f, h - 28,
                     "Fan project. Star Wars and Empire at War are trademarks of their owners.",
                     pal::kTextDim, 1);
}

}  // namespace ui
}  // namespace gc
