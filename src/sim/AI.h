#pragma once

#include <unordered_map>
#include <vector>

#include "core/Types.h"

namespace gc {

class GameState;

/// Computer opponent. It plays by exactly the same rules as the human: it has
/// to build structures before it can build units, pay for everything out of
/// the same weekly income, research the same technologies, and move its fleets
/// along the same hyperlanes.
class AiController {
public:
    AiController(Faction faction, Difficulty difficulty);

    /// Called once per simulated day by GameState.
    void onDay(GameState& gs);

    Faction faction() const { return faction_; }

private:
    void economyPhase(GameState& gs);
    void researchPhase(GameState& gs);
    void militaryPhase(GameState& gs);
    void defencePhase(GameState& gs);
    void offensePhase(GameState& gs);
    void invasionPhase(GameState& gs);

    /// Worlds we own that border something we do not.
    std::vector<Id> frontierPlanets(const GameState& gs) const;
    bool isFrontier(const GameState& gs, Id planet) const;
    /// Credits we refuse to spend so there is always something in the bank.
    int reserve(const GameState& gs) const;
    /// Value of a planet as an attack target (higher is better).
    float targetValue(const GameState& gs, Id planet) const;

    Faction faction_;
    Difficulty difficulty_;
    float aggression_ = 0.5f;

    Id stagingPlanet_ = kInvalid;
    Id targetPlanet_ = kInvalid;
    int daysStaging_ = 0;
    int lastOffensiveDay_ = -100;
};

}  // namespace gc
