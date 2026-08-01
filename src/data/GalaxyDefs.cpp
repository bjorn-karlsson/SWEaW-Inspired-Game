#include "data/Database.h"

namespace gc {
namespace content {

namespace {

struct P {
    PlanetDef d;
    Database* db;

    P(Database& database, const char* key, const char* name, const char* region, float x, float y)
        : db(&database) {
        d.key = key;
        d.name = name;
        d.region = region;
        d.pos = Vec2(x, y);
    }

    /// spaceUnits / groundUnits / spaceStructures / groundStructures
    P& slots(int su, int gu, int sb, int gb) {
        d.spaceUnitSlots = su;
        d.groundUnitSlots = gu;
        d.spaceBuildSlots = sb;
        d.groundBuildSlots = gb;
        return *this;
    }
    P& income(int credits) {
        d.baseIncome = credits;
        return *this;
    }
    P& traits(std::vector<Trait> t) {
        d.traits = std::move(t);
        return *this;
    }
    P& spaceOnly() {
        d.spaceOnly = true;
        d.groundUnitSlots = 0;
        d.groundBuildSlots = 0;
        return *this;
    }
    P& desc(const char* s) {
        d.description = s;
        return *this;
    }
    Id add() { return db->addPlanet(d); }
};

}  // namespace

void registerGalaxy(Database& d) {
    // =======================================================================
    // CORE WORLDS - rich, heavily developed, hard to take.
    // =======================================================================
    P(d, "coruscant", "Coruscant", "Core Worlds", 500, 500)
        .slots(10, 10, 3, 5).income(420).traits({Trait::CoreWorld, Trait::TradeHub})
        .desc("Capital of the Republic. The ecumenopolis at the centre of everything.").add();
    P(d, "kuat", "Kuat", "Core Worlds", 574, 455)
        .slots(9, 6, 3, 4).income(300).traits({Trait::CoreWorld, Trait::Shipyards})
        .desc("Kuat Drive Yards ring the entire planet: capital ships cost less and build faster.").add();
    P(d, "corellia", "Corellia", "Core Worlds", 449, 535)
        .slots(9, 7, 3, 4).income(320).traits({Trait::CoreWorld, Trait::Shipyards, Trait::TradeHub})
        .desc("Corellian Engineering Corporation yards and the busiest port in the Core.").add();
    P(d, "alderaan", "Alderaan", "Core Worlds", 423, 439)
        .slots(6, 7, 2, 4).income(260).traits({Trait::CoreWorld, Trait::AgriWorld})
        .desc("Peaceful, wealthy, and politically indispensable.").add();
    P(d, "chandrila", "Chandrila", "Core Worlds", 537, 577)
        .slots(6, 6, 2, 4).income(230).traits({Trait::CoreWorld, Trait::AgriWorld})
        .desc("Garden world and senatorial power base.").add();
    P(d, "duro", "Duro", "Core Worlds", 478, 423)
        .slots(8, 4, 3, 3).income(250).traits({Trait::CoreWorld, Trait::Shipyards})
        .desc("Orbital cities and shipyards above a poisoned surface.").add();
    P(d, "brentaal", "Brentaal IV", "Core Worlds", 545, 398)
        .slots(7, 5, 2, 4).income(240).traits({Trait::CoreWorld, Trait::TradeHub})
        .desc("Where the Perlemian Trade Route meets the Hydian Way.").add();
    P(d, "anaxes", "Anaxes", "Core Worlds", 462, 486)
        .slots(8, 8, 3, 4).income(210).traits({Trait::CoreWorld, Trait::FortressWorld})
        .desc("The Republic Naval Academy and its fortress moons.").add();
    P(d, "rendili", "Rendili", "Core Worlds", 548, 513)
        .slots(8, 5, 3, 3).income(230).traits({Trait::Shipyards})
        .desc("Rendili StarDrive: dreadnought hulls by the dozen.").add();
    P(d, "foerost", "Foerost", "Core Worlds", 583, 423)
        .slots(8, 4, 3, 2).income(210).traits({Trait::Shipyards})
        .desc("Vast military slipways, second only to Kuat.").add();
    P(d, "fondor", "Fondor", "Core Worlds", 606, 551)
        .slots(8, 5, 3, 3).income(250).traits({Trait::Shipyards, Trait::TradeHub})
        .desc("Independent shipyards that build for whoever pays.").add();

    // =======================================================================
    // COLONIES AND INNER RIM
    // =======================================================================
    P(d, "cato_neimoidia", "Cato Neimoidia", "Colonies", 447, 408)
        .slots(7, 6, 2, 4).income(250).traits({Trait::TradeHub})
        .desc("Bridge-cities heavy with Trade Federation purse worlds.").add();
    P(d, "bothawui", "Bothawui", "Colonies", 622, 428)
        .slots(6, 5, 2, 3).income(210).traits({Trait::TradeHub, Trait::ResearchCentre})
        .desc("Neutral spynet hub. Everyone's information passes through here.").add();
    P(d, "sluis_van", "Sluis Van", "Colonies", 302, 520)
        .slots(8, 0, 4, 0).income(190).traits({Trait::Shipyards}).spaceOnly()
        .desc("A shipyard complex in open space. Take orbit and the system is yours.").add();
    P(d, "mygeeto", "Mygeeto", "Inner Rim", 300, 378)
        .slots(6, 6, 2, 3).income(220).traits({Trait::MiningWorld, Trait::CoreWorld})
        .desc("Frozen crystal cities owned outright by the Banking Clan.").add();
    P(d, "muunilinst", "Muunilinst", "Inner Rim", 620, 332)
        .slots(7, 6, 2, 4).income(300).traits({Trait::TradeHub, Trait::MiningWorld})
        .desc("The Muun homeworld: the bank vault of the Confederacy.").add();
    P(d, "jabiim", "Jabiim", "Inner Rim", 340, 320)
        .slots(5, 6, 1, 3).income(180).traits({Trait::MiningWorld})
        .desc("Endless rain, endless mud, endless ore.").add();

    // =======================================================================
    // MID RIM
    // =======================================================================
    P(d, "naboo", "Naboo", "Mid Rim", 402, 640)
        .slots(6, 6, 2, 3).income(210).traits({Trait::AgriWorld, Trait::TradeHub})
        .desc("Plasma-rich and lightly defended; invaded once already.").add();
    P(d, "malastare", "Malastare", "Mid Rim", 432, 690)
        .slots(5, 5, 2, 3).income(190).traits({Trait::MiningWorld})
        .desc("Fuel refineries and Dug politics.").add();
    P(d, "rodia", "Rodia", "Mid Rim", 622, 688)
        .slots(5, 5, 2, 3).income(180).traits({Trait::TradeHub})
        .desc("Rodian arms markets on the edge of Hutt Space.").add();
    P(d, "ord_mantell", "Ord Mantell", "Mid Rim", 600, 606)
        .slots(6, 6, 2, 3).income(180).traits({Trait::FortressWorld})
        .desc("Old Republic ordnance depot, now a smuggler's haven.").add();
    P(d, "kashyyyk", "Kashyyyk", "Mid Rim", 556, 614)
        .slots(5, 7, 2, 3).income(190).traits({Trait::AgriWorld})
        .desc("Wroshyr forests and fiercely loyal Wookiee defenders.").add();
    P(d, "praesitlyn", "Praesitlyn", "Mid Rim", 566, 646)
        .slots(5, 4, 2, 2).income(150).traits({Trait::ResearchCentre})
        .desc("The Intergalactic Communications Centre. Whoever holds it, hears everything.").add();
    P(d, "toydaria", "Toydaria", "Mid Rim", 655, 540)
        .slots(5, 4, 2, 3).income(170).traits({Trait::TradeHub})
        .desc("Neutral trade world wedged against Hutt Space.").add();
    P(d, "onderon", "Onderon", "Mid Rim", 706, 650)
        .slots(5, 6, 2, 3).income(180).traits({Trait::FortressWorld})
        .desc("Walled city of Iziz, ringed by jungle and rebellion.").add();
    P(d, "felucia", "Felucia", "Mid Rim", 640, 724)
        .slots(4, 6, 1, 3).income(160).traits({Trait::AgriWorld})
        .desc("Fungal jungle; a nightmare for armoured columns.").add();
    P(d, "sullust", "Sullust", "Mid Rim", 352, 598)
        .slots(6, 5, 2, 3).income(200).traits({Trait::MiningWorld, Trait::ForgeWorld})
        .desc("Volcanic industry running at full capacity for any buyer.").add();

    // =======================================================================
    // OUTER RIM
    // =======================================================================
    P(d, "eriadu", "Eriadu", "Outer Rim", 330, 722)
        .slots(7, 6, 2, 4).income(230).traits({Trait::TradeHub, Trait::FortressWorld})
        .desc("Grand Moff country: the industrial capital of the Seswenna sector.").add();
    P(d, "utapau", "Utapau", "Outer Rim", 252, 602)
        .slots(5, 6, 2, 3).income(180).traits({Trait::MiningWorld})
        .desc("Sinkhole cities honeycombing a windswept world.").add();
    P(d, "mustafar", "Mustafar", "Outer Rim", 232, 702)
        .slots(4, 5, 1, 3).income(210).traits({Trait::MiningWorld, Trait::ForgeWorld})
        .desc("Lava mines and Techno Union facilities.").add();
    P(d, "geonosis", "Geonosis", "Outer Rim", 762, 700)
        .slots(7, 8, 2, 4).income(200).traits({Trait::DroidFoundry, Trait::ForgeWorld})
        .desc("Hive foundries that started the war. Droids roll out by the million.").add();
    P(d, "hypori", "Hypori", "Outer Rim", 764, 562)
        .slots(5, 6, 2, 3).income(180).traits({Trait::DroidFoundry})
        .desc("Baktoid Armour Workshop's hidden assembly plants.").add();
    P(d, "dathomir", "Dathomir", "Outer Rim", 768, 482)
        .slots(4, 5, 1, 2).income(140).traits({})
        .desc("Nightsister territory. Outsiders are not welcome.").add();
    P(d, "kessel", "Kessel", "Outer Rim", 740, 540)
        .slots(5, 4, 2, 2).income(200).traits({Trait::MiningWorld, Trait::CriminalHub})
        .desc("Spice mines worked by prisoners, guarded by the Cartels.").add();
    P(d, "tatooine", "Tatooine", "Outer Rim", 742, 662)
        .slots(4, 5, 1, 3).income(130).traits({Trait::CriminalHub})
        .desc("Twin suns, no water, and a Hutt in every palace.").add();
    P(d, "ryloth", "Ryloth", "Outer Rim", 700, 704)
        .slots(5, 6, 2, 3).income(180).traits({Trait::MiningWorld, Trait::CriminalHub})
        .desc("Ryll mines the Cartels will fight very hard to keep.").add();
    P(d, "christophsis", "Christophsis", "Outer Rim", 662, 752)
        .slots(5, 5, 2, 3).income(180).traits({Trait::MiningWorld})
        .desc("Crystal spires and a supply line worth a battle.").add();
    P(d, "saleucami", "Saleucami", "Outer Rim", 692, 762)
        .slots(4, 5, 1, 3).income(150).traits({})
        .desc("Hidden Separatist cloning and repair facilities.").add();
    P(d, "umbara", "Umbara", "Outer Rim", 598, 764)
        .slots(5, 6, 2, 3).income(190).traits({Trait::ForgeWorld})
        .desc("The Shadow World. Native militia technology is decades ahead.").add();
    P(d, "mon_cala", "Mon Cala", "Outer Rim", 585, 300)
        .slots(7, 5, 3, 3).income(240).traits({Trait::Shipyards})
        .desc("Mon Calamari floating yards; magnificent hulls, contested loyalties.").add();

    // =======================================================================
    // HUTT SPACE AND WILD SPACE
    // =======================================================================
    P(d, "nal_hutta", "Nal Hutta", "Hutt Space", 678, 600)
        .slots(8, 7, 3, 5).income(340).traits({Trait::CriminalHub, Trait::TradeHub})
        .desc("Glorious Jewel of the Hutts. The Cartel capital.").add();
    P(d, "nar_shaddaa", "Nar Shaddaa", "Hutt Space", 690, 584)
        .slots(7, 6, 3, 4).income(310).traits({Trait::CriminalHub, Trait::TradeHub})
        .desc("The Smuggler's Moon: every crime in the galaxy has an office here.").add();
    P(d, "sleheyron", "Sleheyron", "Hutt Space", 750, 606)
        .slots(5, 5, 2, 3).income(200).traits({Trait::CriminalHub, Trait::ForgeWorld})
        .desc("Gladiator pits above smelting works owned by minor Hutt lords.").add();
    P(d, "vergesso", "Vergesso Asteroids", "Hutt Space", 712, 520)
        .slots(7, 0, 3, 0).income(170).traits({Trait::MiningWorld}).spaceOnly()
        .desc("Hollowed-out asteroid yards. There is no ground to invade.").add();
    P(d, "rishi", "Rishi Station", "Wild Space", 800, 700)
        .slots(6, 0, 3, 0).income(100).traits({Trait::DeepSpace}).spaceOnly()
        .desc("Deep space listening post above a barren moon.").add();
    P(d, "kamino", "Kamino", "Wild Space", 826, 762)
        .slots(6, 6, 2, 4).income(200).traits({Trait::CloneFacility})
        .desc("Endless ocean and the cloning vats of the Grand Army.").add();

    // =======================================================================
    // LANES. Hyperlanes (the great trade routes) allow much faster travel.
    // =======================================================================
    struct L { const char* a; const char* b; bool hyper; };
    static const L kLanes[] = {
        // --- Core cluster ---
        {"coruscant", "kuat", true},
        {"coruscant", "corellia", true},
        {"coruscant", "alderaan", true},
        {"coruscant", "chandrila", true},
        {"coruscant", "anaxes", false},
        {"coruscant", "duro", false},
        {"kuat", "foerost", false},
        {"kuat", "brentaal", true},
        {"kuat", "fondor", false},
        {"corellia", "duro", false},
        {"corellia", "rendili", false},
        {"rendili", "chandrila", false},
        {"anaxes", "rendili", false},
        {"duro", "brentaal", false},
        {"fondor", "chandrila", false},
        {"foerost", "brentaal", false},
        {"alderaan", "cato_neimoidia", true},

        // --- Perlemian / northern arm ---
        {"brentaal", "muunilinst", true},
        {"muunilinst", "bothawui", true},
        {"muunilinst", "mon_cala", false},
        {"bothawui", "dathomir", false},
        {"mon_cala", "jabiim", false},
        {"cato_neimoidia", "mygeeto", true},
        {"mygeeto", "jabiim", false},
        {"mygeeto", "sluis_van", false},

        // --- Corellian Trade Spine / southwest ---
        {"corellia", "sluis_van", true},
        {"sluis_van", "sullust", true},
        {"sullust", "utapau", false},
        {"sullust", "naboo", false},
        {"sullust", "eriadu", true},
        {"utapau", "mustafar", false},
        {"mustafar", "eriadu", false},
        {"eriadu", "malastare", true},
        {"naboo", "malastare", true},
        {"naboo", "chandrila", true},
        {"naboo", "rodia", false},

        // --- Mid Rim belt ---
        {"chandrila", "kashyyyk", true},
        {"kashyyyk", "praesitlyn", false},
        {"praesitlyn", "ord_mantell", false},
        {"praesitlyn", "umbara", false},
        {"ord_mantell", "rodia", false},
        {"ord_mantell", "toydaria", false},
        {"toydaria", "nar_shaddaa", true},
        {"toydaria", "kessel", false},
        {"rodia", "nal_hutta", true},
        {"felucia", "rodia", false},
        {"felucia", "umbara", false},
        {"felucia", "onderon", false},
        {"onderon", "nal_hutta", false},

        // --- Hutt Space ---
        {"nal_hutta", "nar_shaddaa", true},
        {"nal_hutta", "sleheyron", false},
        {"nal_hutta", "tatooine", true},
        {"nar_shaddaa", "vergesso", false},
        {"vergesso", "kessel", false},
        {"vergesso", "dathomir", false},
        {"kessel", "hypori", false},
        {"sleheyron", "hypori", false},
        {"sleheyron", "geonosis", false},
        {"tatooine", "geonosis", true},
        {"tatooine", "ryloth", false},
        {"tatooine", "rishi", false},
        {"ryloth", "nal_hutta", false},
        {"ryloth", "saleucami", false},
        {"saleucami", "christophsis", false},
        {"christophsis", "umbara", false},
        {"christophsis", "geonosis", false},
        {"geonosis", "rishi", false},
        {"rishi", "kamino", true},
        {"kamino", "geonosis", false},
        {"hypori", "dathomir", false},
    };

    for (const L& l : kLanes) d.addLane(l.a, l.b, l.hyper);
}

// ===========================================================================
// Campaign setups
// ===========================================================================
namespace {

void addForces(FactionStart& s, const char* planet, std::initializer_list<std::pair<const char*, int>> units) {
    for (const auto& u : units) {
        StartingForce f;
        f.planetKey = planet;
        f.unitKey = u.first;
        f.count = u.second;
        s.forces.push_back(f);
    }
}

void addBuildings(FactionStart& s, const char* planet, std::initializer_list<const char*> buildings) {
    for (const char* b : buildings) {
        StartingBuilding sb;
        sb.planetKey = planet;
        sb.buildingKey = b;
        s.buildings.push_back(sb);
    }
}

}  // namespace

void registerCampaigns(Database& d) {
    // -----------------------------------------------------------------------
    // 1. Total galactic war - every world in the database.
    // -----------------------------------------------------------------------
    {
        CampaignDef c;
        c.key = "clone_wars_total";
        c.name = "The Clone Wars: Total Galactic War";
        c.description =
            "The whole galaxy, from Coruscant to the Rim. Long campaign: build an economy, "
            "research a war fleet, and take every world.";

        FactionStart rep;
        rep.faction = Faction::Republic;
        rep.credits = 9000;
        rep.planets = {"coruscant", "kuat",     "corellia",   "alderaan", "chandrila",
                       "duro",      "brentaal", "anaxes",     "rendili",  "foerost",
                       "fondor",    "kamino",   "naboo",      "kashyyyk", "praesitlyn"};
        addBuildings(rep, "coruscant", {"b_rep_station2", "b_rep_barracks", "b_rep_lightfac", "b_rep_governance"});
        addBuildings(rep, "kuat", {"b_rep_station2", "b_rep_barracks"});
        addBuildings(rep, "anaxes", {"b_rep_station1", "b_rep_barracks", "b_turret"});
        addBuildings(rep, "kamino", {"b_rep_barracks", "b_rep_station1"});
        addBuildings(rep, "corellia", {"b_rep_station1", "b_trade_port"});
        addForces(rep, "coruscant", {{"rep_acclamator", 1}, {"rep_consular", 2}, {"rep_v19", 2}, {"rep_clone", 3}, {"rep_atte", 1}});
        addForces(rep, "kuat", {{"rep_arquitens", 1}, {"rep_consular", 1}, {"rep_v19", 1}});
        addForces(rep, "anaxes", {{"rep_consular", 2}, {"rep_clone", 2}, {"rep_atrt", 1}});
        addForces(rep, "kamino", {{"rep_consular", 1}, {"rep_clone", 3}});
        addForces(rep, "naboo", {{"rep_clone", 2}});
        addForces(rep, "kashyyyk", {{"rep_clone", 2}, {"rep_atrt", 1}});
        addForces(rep, "corellia", {{"rep_consular", 1}, {"rep_v19", 1}, {"rep_clone", 1}});
        c.starts.push_back(rep);

        FactionStart cis;
        cis.faction = Faction::CIS;
        cis.credits = 9000;
        cis.planets = {"geonosis",  "hypori",  "muunilinst", "cato_neimoidia", "mygeeto",
                       "jabiim",    "umbara",  "saleucami",  "christophsis",   "felucia",
                       "dathomir",  "utapau",  "mustafar",   "onderon"};
        addBuildings(cis, "geonosis", {"b_cis_station2", "b_cis_foundry", "b_cis_works", "b_turret"});
        addBuildings(cis, "muunilinst", {"b_cis_station2", "b_cis_vault", "b_cis_foundry"});
        addBuildings(cis, "hypori", {"b_cis_station1", "b_cis_foundry"});
        addBuildings(cis, "cato_neimoidia", {"b_cis_station1", "b_trade_port"});
        addBuildings(cis, "mustafar", {"b_cis_foundry", "b_mine"});
        addForces(cis, "geonosis", {{"cis_recusant", 1}, {"cis_diamond", 2}, {"cis_vulture", 3}, {"cis_b1", 4}, {"cis_aat", 1}});
        addForces(cis, "muunilinst", {{"cis_munificent", 1}, {"cis_diamond", 1}, {"cis_vulture", 2}, {"cis_b1", 2}});
        addForces(cis, "hypori", {{"cis_diamond", 2}, {"cis_b1", 3}});
        addForces(cis, "cato_neimoidia", {{"cis_diamond", 1}, {"cis_vulture", 1}, {"cis_b1", 2}});
        addForces(cis, "utapau", {{"cis_b1", 2}});
        addForces(cis, "onderon", {{"cis_b1", 2}, {"cis_aat", 1}});
        c.starts.push_back(cis);

        FactionStart hutt;
        hutt.faction = Faction::Hutts;
        hutt.credits = 11000;
        hutt.planets = {"nal_hutta", "nar_shaddaa", "sleheyron", "tatooine",
                        "ryloth",    "kessel",      "vergesso",  "toydaria", "rishi"};
        addBuildings(hutt, "nal_hutta", {"b_hutt_station2", "b_hutt_camp", "b_hutt_chop", "b_hutt_den"});
        addBuildings(hutt, "nar_shaddaa", {"b_hutt_station1", "b_hutt_camp", "b_trade_port"});
        addBuildings(hutt, "vergesso", {"b_hutt_station1", "b_space_mine"});
        addBuildings(hutt, "kessel", {"b_mine", "b_hutt_camp"});
        addForces(hutt, "nal_hutta", {{"hutt_kaloth", 1}, {"hutt_cr90", 2}, {"hutt_z95", 2}, {"hutt_thug", 4}});
        addForces(hutt, "nar_shaddaa", {{"hutt_cr90", 1}, {"hutt_z95", 2}, {"hutt_thug", 3}});
        addForces(hutt, "vergesso", {{"hutt_cr90", 1}, {"hutt_skipray", 2}});
        addForces(hutt, "tatooine", {{"hutt_thug", 2}, {"hutt_hover", 1}});
        addForces(hutt, "ryloth", {{"hutt_thug", 2}});
        addForces(hutt, "kessel", {{"hutt_z95", 1}, {"hutt_thug", 2}});
        c.starts.push_back(hutt);

        d.addCampaign(std::move(c));
    }

    // -----------------------------------------------------------------------
    // 2. Outer Rim Sieges - a medium campaign fought on the Rim.
    // -----------------------------------------------------------------------
    {
        CampaignDef c;
        c.key = "outer_rim_sieges";
        c.name = "The Outer Rim Sieges";
        c.description =
            "The war's final phase on the Rim. Fewer worlds, richer starting forces, "
            "and the Hutts sitting on the supply lines.";
        c.planetKeys = {"geonosis",   "hypori",       "kamino",    "rishi",     "tatooine",
                        "ryloth",     "saleucami",    "christophsis", "umbara",  "felucia",
                        "onderon",    "nal_hutta",    "nar_shaddaa", "sleheyron", "vergesso",
                        "kessel",     "toydaria",     "dathomir",  "rodia",     "ord_mantell"};

        FactionStart rep;
        rep.faction = Faction::Republic;
        rep.credits = 12000;
        rep.planets = {"kamino", "rishi", "christophsis", "umbara", "ord_mantell", "rodia"};
        addBuildings(rep, "kamino", {"b_rep_station2", "b_rep_barracks", "b_rep_lightfac"});
        addBuildings(rep, "christophsis", {"b_rep_station1", "b_rep_barracks", "b_mine"});
        addBuildings(rep, "ord_mantell", {"b_rep_station1", "b_turret"});
        addForces(rep, "kamino", {{"rep_venator", 1}, {"rep_acclamator", 1}, {"rep_consular", 2}, {"rep_clone", 4}, {"rep_atte", 2}});
        addForces(rep, "christophsis", {{"rep_arquitens", 1}, {"rep_v19", 2}, {"rep_clone", 3}, {"rep_saber", 1}});
        addForces(rep, "ord_mantell", {{"rep_consular", 2}, {"rep_clone", 2}});
        addForces(rep, "umbara", {{"rep_clone", 2}, {"rep_atrt", 1}});
        c.starts.push_back(rep);

        FactionStart cis;
        cis.faction = Faction::CIS;
        cis.credits = 12000;
        cis.planets = {"geonosis", "hypori", "saleucami", "felucia", "onderon", "dathomir"};
        addBuildings(cis, "geonosis", {"b_cis_station2", "b_cis_foundry", "b_cis_works", "b_turret"});
        addBuildings(cis, "hypori", {"b_cis_station1", "b_cis_foundry", "b_cis_works"});
        addBuildings(cis, "saleucami", {"b_cis_foundry", "b_cis_station1"});
        addForces(cis, "geonosis", {{"cis_providence", 1}, {"cis_munificent", 1}, {"cis_vulture", 3}, {"cis_b1", 5}, {"cis_aat", 2}});
        addForces(cis, "hypori", {{"cis_recusant", 1}, {"cis_diamond", 2}, {"cis_b1", 3}, {"cis_hailfire", 1}});
        addForces(cis, "saleucami", {{"cis_diamond", 2}, {"cis_b1", 3}});
        addForces(cis, "onderon", {{"cis_b1", 3}, {"cis_aat", 1}});
        c.starts.push_back(cis);

        FactionStart hutt;
        hutt.faction = Faction::Hutts;
        hutt.credits = 14000;
        hutt.planets = {"nal_hutta", "nar_shaddaa", "sleheyron", "tatooine", "ryloth",
                        "kessel",    "vergesso",    "toydaria"};
        addBuildings(hutt, "nal_hutta", {"b_hutt_station2", "b_hutt_camp", "b_hutt_chop", "b_hutt_den", "b_hutt_spice"});
        addBuildings(hutt, "nar_shaddaa", {"b_hutt_station2", "b_hutt_camp", "b_trade_port"});
        addBuildings(hutt, "vergesso", {"b_hutt_station2", "b_space_mine"});
        addForces(hutt, "nal_hutta", {{"hutt_dreadnaught", 1}, {"hutt_kaloth", 1}, {"hutt_cr90", 2}, {"hutt_z95", 3}, {"hutt_thug", 5}, {"hutt_hover", 2}});
        addForces(hutt, "nar_shaddaa", {{"hutt_kaloth", 1}, {"hutt_dp20", 1}, {"hutt_thug", 3}});
        addForces(hutt, "vergesso", {{"hutt_cr90", 2}, {"hutt_skipray", 2}});
        addForces(hutt, "tatooine", {{"hutt_thug", 3}, {"hutt_hover", 1}});
        c.starts.push_back(hutt);

        d.addCampaign(std::move(c));
    }

    // -----------------------------------------------------------------------
    // 3. Battle for the Core - a short, dense campaign.
    // -----------------------------------------------------------------------
    {
        CampaignDef c;
        c.key = "core_conflict";
        c.name = "Battle for the Core";
        c.description =
            "A short, brutal campaign fought over the richest worlds in the galaxy. "
            "Good for a quick game.";
        c.planetKeys = {"coruscant", "kuat",      "corellia", "alderaan",       "chandrila",
                        "duro",      "brentaal",  "anaxes",   "rendili",        "foerost",
                        "fondor",    "sluis_van", "mygeeto",  "cato_neimoidia", "muunilinst",
                        "bothawui",  "mon_cala"};

        FactionStart rep;
        rep.faction = Faction::Republic;
        rep.credits = 10000;
        rep.planets = {"coruscant", "anaxes", "chandrila", "rendili", "corellia"};
        addBuildings(rep, "coruscant", {"b_rep_station2", "b_rep_barracks", "b_rep_lightfac", "b_rep_governance"});
        addBuildings(rep, "anaxes", {"b_rep_station2", "b_rep_barracks", "b_turret", "b_ion_platform"});
        addBuildings(rep, "corellia", {"b_rep_station1", "b_trade_port"});
        addForces(rep, "coruscant", {{"rep_acclamator", 1}, {"rep_arquitens", 1}, {"rep_consular", 2}, {"rep_v19", 2}, {"rep_clone", 4}, {"rep_atte", 1}});
        addForces(rep, "anaxes", {{"rep_arquitens", 1}, {"rep_consular", 2}, {"rep_clone", 3}, {"rep_saber", 1}});
        addForces(rep, "corellia", {{"rep_consular", 2}, {"rep_v19", 1}, {"rep_clone", 2}});
        c.starts.push_back(rep);

        FactionStart cis;
        cis.faction = Faction::CIS;
        cis.credits = 10000;
        cis.planets = {"cato_neimoidia", "muunilinst", "mygeeto", "foerost", "brentaal"};
        addBuildings(cis, "muunilinst", {"b_cis_station2", "b_cis_foundry", "b_cis_vault", "b_cis_works"});
        addBuildings(cis, "cato_neimoidia", {"b_cis_station2", "b_cis_foundry", "b_trade_port"});
        addBuildings(cis, "foerost", {"b_cis_station1"});
        addForces(cis, "muunilinst", {{"cis_recusant", 1}, {"cis_munificent", 1}, {"cis_vulture", 3}, {"cis_b1", 4}, {"cis_aat", 1}});
        addForces(cis, "cato_neimoidia", {{"cis_munificent", 1}, {"cis_diamond", 2}, {"cis_vulture", 2}, {"cis_b1", 3}});
        addForces(cis, "foerost", {{"cis_diamond", 2}, {"cis_b1", 2}});
        addForces(cis, "mygeeto", {{"cis_b1", 3}, {"cis_hailfire", 1}});
        c.starts.push_back(cis);

        FactionStart hutt;
        hutt.faction = Faction::Hutts;
        hutt.credits = 12000;
        hutt.planets = {"fondor", "sluis_van", "bothawui", "mon_cala"};
        addBuildings(hutt, "fondor", {"b_hutt_station2", "b_hutt_camp", "b_hutt_den", "b_hutt_chop"});
        addBuildings(hutt, "sluis_van", {"b_hutt_station2", "b_ion_platform"});
        addBuildings(hutt, "mon_cala", {"b_hutt_station1"});
        addForces(hutt, "fondor", {{"hutt_kaloth", 1}, {"hutt_cr90", 2}, {"hutt_z95", 2}, {"hutt_thug", 4}, {"hutt_hover", 1}});
        addForces(hutt, "sluis_van", {{"hutt_kaloth", 1}, {"hutt_cr90", 2}, {"hutt_skipray", 2}});
        addForces(hutt, "bothawui", {{"hutt_thug", 3}, {"hutt_hover", 1}});
        addForces(hutt, "mon_cala", {{"hutt_cr90", 1}, {"hutt_z95", 1}, {"hutt_thug", 2}});
        c.starts.push_back(hutt);

        d.addCampaign(std::move(c));
    }
}

}  // namespace content
}  // namespace gc
