#include "data/Database.h"

namespace gc {
namespace content {

namespace {

struct TechBuilder {
    Database& d;
    Faction faction;

    Id add(const std::string& key, const std::string& name, int tier, int cost, int days,
           const std::vector<std::string>& prereqKeys, const std::string& unlocks,
           const std::string& desc) {
        TechDef t;
        t.key = key;
        t.name = name;
        t.faction = faction;
        t.tier = tier;
        t.cost = cost;
        t.researchDays = days;
        t.unlocksText = unlocks;
        t.description = desc;
        for (const std::string& p : prereqKeys) {
            Id id = d.techId(p);
            if (id != kInvalid) t.prerequisites.push_back(id);
        }
        return d.addTech(std::move(t));
    }
};

}  // namespace

void registerTechs(Database& d) {
    // -----------------------------------------------------------------------
    // Galactic Republic - a war machine that scales from clone legions to
    // Star Destroyers.
    // -----------------------------------------------------------------------
    {
        TechBuilder t{d, Faction::Republic};
        t.add("rep_mobilisation", "Military Mobilisation", 1, 900, 6, {},
              "Acclamator-class Assault Ship, LAAT/i Gunship, Orbital Station II",
              "The Senate authorises full military production across the Core.");
        t.add("rep_clone_doctrine", "Advanced Clone Doctrine", 1, 1100, 7, {},
              "Clone Heavy Trooper, ARC Trooper, Heavy Vehicle Works",
              "Kaminoan flash-training regimes produce specialist troopers.");
        t.add("rep_venator_program", "Venator Programme", 2, 2200, 12, {"rep_mobilisation"},
              "Venator-class Star Destroyer, ARC-170 Squadron, Orbital Station III",
              "Kuat lays down the keels of the Republic's first true Star Destroyers.");
        t.add("rep_torpedo_doctrine", "Proton Torpedo Doctrine", 2, 1500, 9, {"rep_mobilisation"},
              "BTL-B Y-wing Squadron, improved bomber ordnance",
              "Massed torpedo bombers become standard anti-capital doctrine.");
        t.add("rep_jedi_taskforce", "Jedi Task Forces", 2, 1600, 10, {},
              "Jedi generals may be recruited, Delta-7 Aethersprite Squadron",
              "The Jedi Order formally takes command of the Grand Army.");
        t.add("rep_siege_warfare", "Siege Warfare", 2, 1800, 10, {"rep_clone_doctrine"},
              "SPHA-T Artillery Walker, AT-TE upgrades",
              "Heavy walkers and self-propelled cannons for planetary sieges.");
        t.add("rep_victory_program", "Victory Programme", 3, 3600, 16, {"rep_venator_program"},
              "Victory-class Star Destroyer",
              "A dedicated line battleship for the Outer Rim Sieges.");
        t.add("rep_juggernaut", "Armoured Column Doctrine", 3, 2600, 13, {"rep_siege_warfare"},
              "HAVw A6 Juggernaut, TX-130 Saber Tank upgrades",
              "Ten-wheeled behemoths spearhead Republic ground offensives.");
    }

    // -----------------------------------------------------------------------
    // Confederacy of Independent Systems - endless droids and monstrous hulls.
    // -----------------------------------------------------------------------
    {
        TechBuilder t{d, Faction::CIS};
        t.add("cis_mass_production", "Droid Mass Production", 1, 800, 5, {},
              "B2 Super Battle Droid, AAT upgrades, Orbital Station II",
              "The Techno Union foundries switch to a total war footing.");
        t.add("cis_munificent_line", "Munificent Line", 1, 1100, 7, {},
              "Munificent-class Star Frigate, Droid Works",
              "The Banking Clan releases its star frigates to the Separatist navy.");
        t.add("cis_droid_fighters", "Advanced Droid Fighters", 2, 1400, 8, {"cis_mass_production"},
              "Tri-Fighter Squadron, Hyena-class Bomber Squadron",
              "Colicoid designers deliver faster, deadlier droid starfighters.");
        t.add("cis_providence_program", "Providence Programme", 2, 2400, 12, {"cis_munificent_line"},
              "Providence-class Dreadnought, Orbital Station III",
              "Free Dac shipyards deliver the Confederacy's command carriers.");
        t.add("cis_techno_union", "Techno Union Contracts", 2, 1700, 10, {"cis_mass_production"},
              "OG-9 Homing Spider Droid, Octuptarra Tri-Droid, Hailfire Droid",
              "The Techno Union fields its heaviest walking weapon platforms.");
        t.add("cis_dark_acolytes", "Dark Acolytes", 2, 1600, 10, {},
              "Separatist commanders may be recruited, Magnaguard Squad",
              "Dooku gathers fallen Jedi and assassins to his cause.");
        t.add("cis_lucrehulk_refit", "Lucrehulk Battleship Refit", 3, 3800, 17,
              {"cis_providence_program"}, "Lucrehulk-class Battleship",
              "Trade Federation core ships are rebuilt as fleet battleships.");
        t.add("cis_bio_droids", "Bio-Droid Engineering", 3, 2600, 13, {"cis_techno_union"},
              "Crab Droid, improved droid armour plating",
              "Geonosian hive-tech is merged with Baktoid war machines.");
    }

    // -----------------------------------------------------------------------
    // Hutt Cartels - money buys guns, and guns buy more money.
    // -----------------------------------------------------------------------
    {
        TechBuilder t{d, Faction::Hutts};
        t.add("hutt_smuggling", "Smuggling Network", 1, 700, 5, {},
              "Smuggling Den, Spice Refinery",
              "Cartel routes bleed credits out of every neighbouring sector.");
        t.add("hutt_enforcers", "Cartel Enforcers", 1, 900, 6, {},
              "Gamorrean Heavies, Nikto Raiders, Chop Shop",
              "The Hutts arm their palace guards for open warfare.");
        t.add("hutt_pirate_fleets", "Pirate Fleet Charters", 2, 1500, 9, {"hutt_smuggling"},
              "Corellian Gunship, Kaloth Battlecruiser, Orbital Station II",
              "Every pirate lord in the sector now flies under a Hutt charter.");
        t.add("hutt_bounty_contracts", "Bounty Contracts", 2, 1600, 10, {"hutt_enforcers"},
              "Bounty hunters may be recruited, Mercenary Squad",
              "The galaxy's deadliest hunters answer the Cartel's call.");
        t.add("hutt_black_market", "Black Market Warships", 3, 3400, 15, {"hutt_pirate_fleets"},
              "Black Market Providence, Orbital Station III",
              "Stolen Separatist hulls are refitted in hidden Nal Hutta yards.");
        t.add("hutt_war_machines", "Stolen War Machines", 2, 1800, 11, {"hutt_enforcers"},
              "Black Market AAT, Mobile Proton Cannon, Heavy Chop Shop",
              "Whatever the great powers build, the Cartels eventually steal.");
        t.add("hutt_spice_empire", "Spice Empire", 3, 2600, 14, {"hutt_smuggling"},
              "Palace Complex, +credits from every Cartel world",
              "Glitterstim flows outward; credits flow back in torrents.");
    }
}

}  // namespace content
}  // namespace gc
