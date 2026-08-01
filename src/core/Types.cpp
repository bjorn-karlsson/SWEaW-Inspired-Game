#include "core/Types.h"

namespace gc {

const char* factionName(Faction f) {
    switch (f) {
        case Faction::Republic: return "Galactic Republic";
        case Faction::CIS: return "Confederacy of Independent Systems";
        case Faction::Hutts: return "Hutt Cartels";
        case Faction::Neutral: return "Unaligned";
        default: return "Unknown";
    }
}

const char* factionShortName(Faction f) {
    switch (f) {
        case Faction::Republic: return "Republic";
        case Faction::CIS: return "CIS";
        case Faction::Hutts: return "Hutts";
        case Faction::Neutral: return "Neutral";
        default: return "?";
    }
}

const std::vector<Faction>& playableFactions() {
    static const std::vector<Faction> kPlayable = {Faction::Republic, Faction::CIS, Faction::Hutts};
    return kPlayable;
}

const char* domainName(Domain d) { return d == Domain::Space ? "Space" : "Ground"; }

const char* unitClassName(UnitClass c) {
    switch (c) {
        case UnitClass::Corvette: return "Corvette";
        case UnitClass::Frigate: return "Frigate";
        case UnitClass::Cruiser: return "Cruiser";
        case UnitClass::Capital: return "Capital Ship";
        case UnitClass::Fighter: return "Fighter Squadron";
        case UnitClass::Bomber: return "Bomber Squadron";
        case UnitClass::Infantry: return "Infantry";
        case UnitClass::Vehicle: return "Vehicle";
        case UnitClass::Artillery: return "Artillery";
        case UnitClass::AirSupport: return "Air Support";
        default: return "Unknown";
    }
}

bool isSpaceClass(UnitClass c) {
    switch (c) {
        case UnitClass::Corvette:
        case UnitClass::Frigate:
        case UnitClass::Cruiser:
        case UnitClass::Capital:
        case UnitClass::Fighter:
        case UnitClass::Bomber:
            return true;
        default:
            return false;
    }
}

bool isSquadronClass(UnitClass c) {
    return c == UnitClass::Fighter || c == UnitClass::Bomber;
}

const char* difficultyName(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return "Padawan";
        case Difficulty::Normal: return "Knight";
        case Difficulty::Hard: return "Master";
        case Difficulty::Brutal: return "Grand Master";
        default: return "?";
    }
}

float difficultyAiIncomeMult(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return 0.70f;
        case Difficulty::Normal: return 1.00f;
        case Difficulty::Hard: return 1.30f;
        case Difficulty::Brutal: return 1.70f;
        default: return 1.0f;
    }
}

float difficultyAiCombatMult(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return 0.85f;
        case Difficulty::Normal: return 1.00f;
        case Difficulty::Hard: return 1.12f;
        case Difficulty::Brutal: return 1.25f;
        default: return 1.0f;
    }
}

float difficultyAiAggression(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return 0.35f;
        case Difficulty::Normal: return 0.55f;
        case Difficulty::Hard: return 0.75f;
        case Difficulty::Brutal: return 0.90f;
        default: return 0.5f;
    }
}

std::string GalacticDate::toString() const {
    return "Week " + std::to_string(week() + 1) + ", Day " + std::to_string(dayOfWeek() + 1);
}

}  // namespace gc
