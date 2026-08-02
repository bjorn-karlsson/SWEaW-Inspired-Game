#pragma once

#include <string>
#include <vector>

#include "core/Types.h"
#include "core/Vec2.h"

namespace gc {

// ---------------------------------------------------------------------------
// Planet traits: the "Kuat Drive Yards" style bonuses.
// ---------------------------------------------------------------------------
enum class Trait : int {
    CoreWorld = 0,     ///< Wealthy, well developed.
    TradeHub,          ///< Percentage income bonus.
    Shipyards,         ///< Capital ships cheaper and faster (Kuat, Fondor, Rothana).
    MiningWorld,       ///< Mines may be constructed here.
    ForgeWorld,        ///< Ground vehicles cheaper and faster.
    FortressWorld,     ///< Defensive structures cheaper, garrison fights harder.
    AgriWorld,         ///< Flat income bonus.
    CloneFacility,     ///< Republic infantry cheaper and faster (Kamino).
    DroidFoundry,      ///< CIS droid units cheaper and faster (Geonosis).
    CriminalHub,       ///< Hutt income bonus and cheap mercenaries.
    ResearchCentre,    ///< Technology research is cheaper here.
    DeepSpace,         ///< Remote station: no local economy.
    Count
};

const char* traitName(Trait t);
const char* traitDescription(Trait t);

/// Aggregated modifiers produced by the set of traits on a planet.
struct TraitMods {
    float incomeMult = 1.0f;
    int incomeFlat = 0;
    float capitalCostMult = 1.0f;
    float capitalTimeMult = 1.0f;
    float groundVehicleCostMult = 1.0f;
    float groundVehicleTimeMult = 1.0f;
    float infantryCostMult = 1.0f;
    float infantryTimeMult = 1.0f;
    float structureCostMult = 1.0f;
    float researchCostMult = 1.0f;
    float defenceBonus = 0.0f;  ///< Additive combat bonus for the defender.
    bool allowsMines = false;
};

// ---------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------

/// A wing of squadrons carried by a capital ship / carrier and launched during
/// a space battle (Venator, Acclamator, Providence ...).
struct CarriedWing {
    std::string unitKey;  ///< Key of the fighter/bomber def.
    int count = 0;        ///< Number of squadrons carried.
    Id unitId = kInvalid; ///< Resolved after the database is built.
};

struct UnitDef {
    Id id = kInvalid;
    std::string key;
    std::string name;
    Faction faction = Faction::Neutral;
    UnitClass unitClass = UnitClass::Corvette;

    int cost = 100;
    int buildDays = 3;
    int popCost = 1;  ///< Unit slots consumed on a planet.

    // Combat statistics. "AntiCapital" damage is heavy, slow-firing weaponry
    // (turbolasers, mass drivers); "AntiFighter" is point defence / anti
    // infantry fire.
    float hull = 100.0f;
    float shield = 0.0f;
    float shieldRegen = 2.0f;  ///< Per second, tactical battles only.
    float damageAntiCapital = 10.0f;
    float damageAntiFighter = 5.0f;
    float range = 220.0f;   ///< Tactical weapon range.
    float speed = 45.0f;    ///< Tactical movement speed.
    float accuracy = 1.0f;  ///< Scales damage output in autoresolve.

    std::vector<CarriedWing> wings;  ///< Launched squadrons (carriers only).

    Id requiredTech = kInvalid;
    int requiredTier = 1;  ///< Production building tier needed to build this.

    /// Flavour shown on the unit info card, EaW style.
    std::string role;          ///< "Picket", "Line", "Carrier", ...
    std::string manufacturer;  ///< Who builds it.

    bool isHero = false;
    int heroIncomeBonus = 0;     ///< Weekly credits while the hero is alive.
    float heroCombatBonus = 0.f; ///< Additive force multiplier in its domain.
    int respawnDays = 0;         ///< 0 = gone for good once killed.

    std::string description;

    Domain domain() const { return domainOf(unitClass); }
    bool isSquadron() const { return isSquadronClass(unitClass); }
};

// ---------------------------------------------------------------------------
// Buildings
// ---------------------------------------------------------------------------
struct BuildingDef {
    Id id = kInvalid;
    std::string key;
    std::string name;
    Faction faction = Faction::Neutral;  ///< Neutral = every faction may build it.
    Domain domain = Domain::Ground;

    int cost = 500;
    int buildDays = 4;

    int productionTier = 0;  ///< >0 lets the planet build units of that tier.
    int incomeFlat = 0;      ///< Extra credits per week.
    float incomeMult = 0.0f; ///< Extra fraction of the planet's base income.

    float defenceHp = 0.0f;      ///< Acts as a combatant when the planet is attacked.
    float defenceDamage = 0.0f;
    float shieldStrength = 0.0f; ///< Planetary/orbital shield added to defenders.
    int unitSlotBonus = 0;       ///< Extra unit slots in its own domain.
    float researchSpeed = 0.0f;  ///< Fraction of research time removed.

    Id requiredTech = kInvalid;
    Trait requiredTrait = Trait::Count;  ///< Count = no trait requirement.
    bool uniquePerPlanet = true;

    std::string description;
};

// ---------------------------------------------------------------------------
// Technology
// ---------------------------------------------------------------------------
struct TechDef {
    Id id = kInvalid;
    std::string key;
    std::string name;
    Faction faction = Faction::Neutral;
    int cost = 1000;
    int researchDays = 7;
    int tier = 1;
    std::vector<Id> prerequisites;
    std::string unlocksText;
    std::string description;
};

// ---------------------------------------------------------------------------
// Planets and lanes
// ---------------------------------------------------------------------------
struct PlanetDef {
    Id id = kInvalid;
    std::string key;
    std::string name;
    std::string region;
    Vec2 pos;  ///< Galactic map coordinates.

    bool spaceOnly = false;  ///< No surface: captured by taking orbit.

    int spaceUnitSlots = 8;
    int groundUnitSlots = 6;
    int spaceBuildSlots = 2;
    int groundBuildSlots = 3;

    int baseIncome = 100;  ///< Credits per week before modifiers.

    std::vector<Trait> traits;
    std::string description;

    TraitMods mods() const;
    bool hasTrait(Trait t) const;
};

struct LaneDef {
    Id a = kInvalid;
    Id b = kInvalid;
    bool hyperlane = false;  ///< Major trade route: much faster travel.
};

// ---------------------------------------------------------------------------
// Campaign setup
// ---------------------------------------------------------------------------
struct StartingForce {
    std::string planetKey;
    std::string unitKey;
    int count = 1;
};

struct StartingBuilding {
    std::string planetKey;
    std::string buildingKey;
};

struct FactionStart {
    Faction faction = Faction::Neutral;
    int credits = 5000;
    std::vector<std::string> planets;
    std::vector<StartingForce> forces;
    std::vector<StartingBuilding> buildings;
};

struct CampaignDef {
    Id id = kInvalid;
    std::string key;
    std::string name;
    std::string description;
    /// Planet keys taking part. Empty means "the whole galaxy".
    std::vector<std::string> planetKeys;
    std::vector<FactionStart> starts;
};

}  // namespace gc
