#include "data/Database.h"

namespace gc {
namespace content {

namespace {

struct B {
    BuildingDef d;
    Database* db;

    B(Database& database, Faction f, Domain dom, const char* key, const char* name) : db(&database) {
        d.key = key;
        d.name = name;
        d.faction = f;
        d.domain = dom;
    }

    B& cost(int credits, int days) {
        d.cost = credits;
        d.buildDays = days;
        return *this;
    }
    B& tier(int t) {
        d.productionTier = t;
        return *this;
    }
    B& income(int flat, float mult = 0.0f) {
        d.incomeFlat = flat;
        d.incomeMult = mult;
        return *this;
    }
    B& defence(float hp, float damage) {
        d.defenceHp = hp;
        d.defenceDamage = damage;
        return *this;
    }
    B& shield(float s) {
        d.shieldStrength = s;
        return *this;
    }
    B& slots(int n) {
        d.unitSlotBonus = n;
        return *this;
    }
    B& research(float speedup) {
        d.researchSpeed = speedup;
        return *this;
    }
    B& tech(const char* key) {
        d.requiredTech = db->techId(key);
        return *this;
    }
    B& trait(Trait t) {
        d.requiredTrait = t;
        return *this;
    }
    B& many() {
        d.uniquePerPlanet = false;
        return *this;
    }
    B& desc(const char* s) {
        d.description = s;
        return *this;
    }
    Id add() { return db->addBuilding(d); }
};

}  // namespace

void registerBuildings(Database& d) {
    const Faction N = Faction::Neutral;
    const Domain G = Domain::Ground;
    const Domain S = Domain::Space;

    // =======================================================================
    // Universal structures - any faction may build these.
    // =======================================================================
    B(d, N, G, "b_mine", "Mining Complex")
        .cost(900, 5).income(240).trait(Trait::MiningWorld)
        .desc("Strip-mines the crust. Only possible on mining worlds.").add();
    B(d, N, S, "b_space_mine", "Asteroid Mining Station")
        .cost(850, 5).income(200).trait(Trait::MiningWorld)
        .desc("Orbital ore processing over a mining world.").add();
    B(d, N, G, "b_trade_port", "Trade Port")
        .cost(1200, 6).income(0, 0.25f)
        .desc("Licensed spaceport traffic. +25% of this world's base income.").add();
    B(d, N, S, "b_trade_station", "Orbital Trade Station")
        .cost(1100, 5).income(0, 0.20f)
        .desc("Free-port docking rings. +20% of this world's base income.").add();
    B(d, N, G, "b_turret", "Ground Defence Turrets")
        .cost(700, 4).defence(600, 34).many()
        .desc("Fixed anti-armour emplacements that fight alongside the garrison.").add();
    B(d, N, S, "b_ion_platform", "Orbital Ion Cannon Platform")
        .cost(900, 4).defence(720, 46).many()
        .desc("Weapon platform in orbit. Must be destroyed before any landing.").add();
    B(d, N, G, "b_shield", "Planetary Shield Generator")
        .cost(1600, 7).shield(900).defence(400, 0)
        .desc("Theatre shield protecting the surface. Greatly aids defenders.").add();
    B(d, N, G, "b_research", "Research Facility")
        .cost(1400, 6).research(0.25f)
        .desc("Speeds up all technology research by 25%.").add();
    B(d, N, G, "b_garrison", "Garrison Headquarters")
        .cost(1000, 5).slots(2).defence(300, 12)
        .desc("Barrack complexes: +2 ground unit slots on this world.").add();
    B(d, N, S, "b_docking", "Orbital Docking Ring")
        .cost(1000, 5).slots(2)
        .desc("Fleet moorings: +2 space unit slots on this world.").add();

    // =======================================================================
    // Galactic Republic
    // =======================================================================
    const Faction R = Faction::Republic;
    B(d, R, S, "b_rep_station1", "Republic Orbital Station I")
        .cost(1200, 5).tier(1).defence(650, 26).slots(2)
        .desc("Builds fighters, bombers and corvettes in orbit.").add();
    B(d, R, S, "b_rep_station2", "Republic Orbital Station II")
        .cost(2400, 8).tier(2).defence(1500, 58).slots(2).tech("rep_mobilisation")
        .desc("Expanded yards: frigates and assault ships.").add();
    B(d, R, S, "b_rep_station3", "Republic Fleet Yards")
        .cost(4200, 12).tier(3).defence(2700, 98).slots(3).tech("rep_venator_program")
        .desc("Full capital ship slipways and drydocks.").add();
    B(d, R, G, "b_rep_barracks", "Clone Barracks")
        .cost(900, 4).tier(1).defence(250, 14)
        .desc("Trains clone infantry formations.").add();
    B(d, R, G, "b_rep_lightfac", "Light Vehicle Factory")
        .cost(1600, 6).tier(2)
        .desc("Produces walkers, repulsor tanks and gunships.").add();
    B(d, R, G, "b_rep_heavyfac", "Heavy Vehicle Works")
        .cost(2800, 9).tier(3).tech("rep_clone_doctrine")
        .desc("Assembles Juggernauts and self-propelled artillery.").add();
    B(d, R, G, "b_rep_governance", "Sector Governance Office")
        .cost(1300, 6).income(120, 0.20f)
        .desc("Senate administration draws taxes from the whole sector.").add();

    // =======================================================================
    // Confederacy of Independent Systems
    // =======================================================================
    const Faction C = Faction::CIS;
    B(d, C, S, "b_cis_station1", "Separatist Orbital Station I")
        .cost(1100, 5).tier(1).defence(620, 24).slots(2)
        .desc("Droid starfighter racks and light escort yards.").add();
    B(d, C, S, "b_cis_station2", "Separatist Orbital Station II")
        .cost(2300, 8).tier(2).defence(1450, 55).slots(2).tech("cis_mass_production")
        .desc("Banking Clan slipways for frigates and destroyers.").add();
    B(d, C, S, "b_cis_station3", "Separatist Shipworks")
        .cost(4100, 12).tier(3).defence(2600, 94).slots(3).tech("cis_providence_program")
        .desc("Vast automated yards capable of building dreadnoughts.").add();
    B(d, C, G, "b_cis_foundry", "Battle Droid Foundry")
        .cost(800, 4).tier(1).defence(240, 12)
        .desc("Stamps out battle droids by the tens of thousands.").add();
    B(d, C, G, "b_cis_works", "Droid Works")
        .cost(1500, 6).tier(2).tech("cis_munificent_line")
        .desc("Baktoid armour plant for tanks and walkers.").add();
    B(d, C, G, "b_cis_heavyworks", "Techno Union Heavy Works")
        .cost(2700, 9).tier(3).tech("cis_techno_union")
        .desc("Builds the Confederacy's heaviest droid war machines.").add();
    B(d, C, G, "b_cis_vault", "Banking Clan Vault")
        .cost(1300, 6).income(300)
        .desc("Muun financiers convert conquest into liquid credits.").add();

    // =======================================================================
    // Hutt Cartels
    // =======================================================================
    const Faction H = Faction::Hutts;
    B(d, H, S, "b_hutt_station1", "Cartel Orbital Bazaar")
        .cost(1000, 5).tier(1).defence(560, 22).slots(2).income(60)
        .desc("Smuggler docks that double as a light warship yard.").add();
    B(d, H, S, "b_hutt_station2", "Cartel Shipbreaker Yards")
        .cost(2200, 8).tier(2).defence(1350, 52).slots(2).tech("hutt_pirate_fleets")
        .desc("Salvaged hulls rebuilt into gunships and battlecruisers.").add();
    B(d, H, S, "b_hutt_station3", "Nal Hutta Refit Yards")
        .cost(4000, 12).tier(3).defence(2400, 88).slots(3).tech("hutt_black_market")
        .desc("Hidden drydocks large enough to refit a dreadnought.").add();
    B(d, H, G, "b_hutt_camp", "Mercenary Camp")
        .cost(750, 4).tier(1).defence(220, 12)
        .desc("Hiring hall for enforcers, raiders and thugs.").add();
    B(d, H, G, "b_hutt_chop", "Chop Shop")
        .cost(1400, 6).tier(2).tech("hutt_enforcers")
        .desc("Rebuilds stolen vehicles into cartel armour.").add();
    B(d, H, G, "b_hutt_heavychop", "Heavy Chop Shop")
        .cost(2500, 9).tier(3).tech("hutt_war_machines")
        .desc("Industrial-scale theft: siege guns and battle tanks.").add();
    B(d, H, G, "b_hutt_den", "Smuggling Den")
        .cost(1000, 5).income(260).tech("hutt_smuggling")
        .desc("Off-books cargo running. Pure profit, no questions.").add();
    B(d, H, G, "b_hutt_spice", "Spice Refinery")
        .cost(1500, 6).income(0, 0.35f).tech("hutt_smuggling")
        .desc("Glitterstim processing. +35% of this world's base income.").add();
    B(d, H, G, "b_hutt_palace", "Hutt Palace Complex")
        .cost(2600, 9).income(420).slots(1).defence(700, 30).tech("hutt_spice_empire")
        .desc("A Hutt lord takes up residence: credits, guards and influence.").add();
}

}  // namespace content
}  // namespace gc
