#pragma once

#include <string>
#include <vector>

#include "core/Rng.h"
#include "core/Types.h"

namespace gc {

class GameState;

/// Everything a battle needs to know, extracted from the campaign state.
struct BattleSetup {
    Domain domain = Domain::Space;
    Id planet = kInvalid;
    Faction attacker = Faction::Neutral;
    Faction defender = Faction::Neutral;
    std::vector<Id> attackerUnits;       ///< Unit instance ids.
    std::vector<Id> defenderUnits;
    std::vector<Id> defenderStructures;  ///< Building instance ids that fight.
    float defenderBonus = 0.0f;          ///< Terrain / fortress / shield bonus.
    float attackerBonus = 0.0f;
    bool playerInvolved = false;
};

/// What happened to a single unit instance.
struct UnitOutcome {
    Id instanceId = kInvalid;
    bool destroyed = false;
    bool retreated = false;
    float health = 1.0f;  ///< Remaining fraction of hull.
};

/// Per-unit-type roll-up shown on the summary screen.
struct SummaryEntry {
    Id defId = kInvalid;
    int committed = 0;
    int lost = 0;
    int survived = 0;
    int retreated = 0;
};

struct SideSummary {
    Faction faction = Faction::Neutral;
    std::vector<SummaryEntry> entries;
    int committed = 0;
    int lost = 0;
    int survived = 0;
    int retreated = 0;
    int creditsLost = 0;

    SummaryEntry& entryFor(Id defId);
};

struct BattleReport {
    Domain domain = Domain::Space;
    Id planet = kInvalid;
    std::string planetName;
    Faction attacker = Faction::Neutral;
    Faction defender = Faction::Neutral;
    Faction victor = Faction::Neutral;
    bool autoResolved = true;
    int structuresDestroyed = 0;
    int day = 0;
    SideSummary attackerSide;
    SideSummary defenderSide;

    bool attackerWon() const { return victor == attacker; }
};

/// Result of a battle: unit outcomes plus the summary the UI displays.
struct BattleResolution {
    Faction victor = Faction::Neutral;
    std::vector<UnitOutcome> outcomes;
    std::vector<Id> destroyedStructures;
    BattleReport report;
};

namespace autoresolve {

/// Statistical resolution of a battle. Deterministic for a given rng state.
BattleResolution resolve(const GameState& gs, const BattleSetup& setup, Rng& rng);

/// Rough combat power of a set of units, used by the AI and by the
/// "estimated odds" readout in the UI.
float forceStrength(const GameState& gs, const std::vector<Id>& unitIds, Domain domain);

}  // namespace autoresolve

/// Builds the report structure from unit outcomes (shared by autoresolve and
/// the tactical battler so both produce identical-looking summaries).
BattleReport buildReport(const GameState& gs, const BattleSetup& setup,
                         const std::vector<UnitOutcome>& outcomes, Faction victor,
                         int structuresDestroyed, bool autoResolved);

}  // namespace gc
