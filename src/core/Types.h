#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gc {

using Id = int;
constexpr Id kInvalid = -1;

// ---------------------------------------------------------------------------
// Factions
// ---------------------------------------------------------------------------
enum class Faction : int {
    Neutral = 0,
    Republic = 1,
    CIS = 2,
    Hutts = 3,
    Count = 4
};

constexpr int kFactionCount = static_cast<int>(Faction::Count);

inline int fidx(Faction f) { return static_cast<int>(f); }
inline Faction factionFromIndex(int i) { return static_cast<Faction>(i); }

const char* factionName(Faction f);
const char* factionShortName(Faction f);
/// Playable factions, in menu order.
const std::vector<Faction>& playableFactions();

// ---------------------------------------------------------------------------
// Domains and unit classes
// ---------------------------------------------------------------------------
enum class Domain : int { Space = 0, Ground = 1 };

const char* domainName(Domain d);

enum class UnitClass : int {
    // Space
    Corvette = 0,
    Frigate,
    Cruiser,
    Capital,
    Fighter,
    Bomber,
    // Ground
    Infantry,
    Vehicle,
    Artillery,
    AirSupport,
    Count
};

const char* unitClassName(UnitClass c);
bool isSpaceClass(UnitClass c);
inline bool isGroundClass(UnitClass c) { return !isSpaceClass(c); }
/// Fighters and bombers are squadron-scale craft: they are cheap, numerous and
/// only vulnerable to anti-fighter weaponry.
bool isSquadronClass(UnitClass c);
inline Domain domainOf(UnitClass c) { return isSpaceClass(c) ? Domain::Space : Domain::Ground; }

// ---------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------
enum class Difficulty : int { Easy = 0, Normal, Hard, Brutal, Count };

const char* difficultyName(Difficulty d);
/// Multiplier applied to AI weekly income.
float difficultyAiIncomeMult(Difficulty d);
/// Multiplier applied to AI combat strength during autoresolve.
float difficultyAiCombatMult(Difficulty d);
/// How eagerly the AI launches offensives (0..1, higher = more aggressive).
float difficultyAiAggression(Difficulty d);

// ---------------------------------------------------------------------------
// Calendar
// ---------------------------------------------------------------------------
constexpr int kDaysPerWeek = 7;

struct GalacticDate {
    int day = 0;  ///< Total elapsed days since campaign start.

    int week() const { return day / kDaysPerWeek; }
    int dayOfWeek() const { return day % kDaysPerWeek; }
    std::string toString() const;
};

}  // namespace gc
