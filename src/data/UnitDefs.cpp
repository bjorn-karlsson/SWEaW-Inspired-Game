#include "data/Database.h"

namespace gc {
namespace content {

namespace {

/// Small fluent helper so the roster below reads like a data table.
struct U {
    UnitDef d;
    Database* db;

    U(Database& database, Faction f, UnitClass c, const char* key, const char* name) : db(&database) {
        d.key = key;
        d.name = name;
        d.faction = f;
        d.unitClass = c;
    }

    U& cost(int credits, int days, int pop) {
        d.cost = credits;
        d.buildDays = days;
        d.popCost = pop;
        return *this;
    }
    U& stats(float hull, float shield, float antiCapital, float antiFighter) {
        d.hull = hull;
        d.shield = shield;
        d.damageAntiCapital = antiCapital;
        d.damageAntiFighter = antiFighter;
        return *this;
    }
    U& tac(float range, float speed) {
        d.range = range;
        d.speed = speed;
        return *this;
    }
    U& tier(int t) {
        d.requiredTier = t;
        return *this;
    }
    U& tech(const char* key) {
        d.requiredTech = db->techId(key);
        return *this;
    }
    U& wing(const char* unitKey, int count) {
        CarriedWing w;
        w.unitKey = unitKey;
        w.count = count;
        d.wings.push_back(w);
        return *this;
    }
    U& hero(float combatBonus, int incomeBonus, int respawnDays) {
        d.isHero = true;
        d.heroCombatBonus = combatBonus;
        d.heroIncomeBonus = incomeBonus;
        d.respawnDays = respawnDays;
        return *this;
    }
    U& accuracy(float a) {
        d.accuracy = a;
        return *this;
    }
    U& desc(const char* s) {
        d.description = s;
        return *this;
    }
    Id add() { return db->addUnit(d); }
};

}  // namespace

void registerUnits(Database& d) {
    using UC = UnitClass;

    // =======================================================================
    // GALACTIC REPUBLIC
    // =======================================================================
    const Faction R = Faction::Republic;

    // --- starfighters ---
    U(d, R, UC::Fighter, "rep_v19", "V-19 Torrent Squadron")
        .cost(260, 1, 1).stats(70, 0, 4, 16).tac(110, 92).tier(1)
        .desc("Mainline Republic interceptor squadron. Cheap, fast, expendable.").add();
    U(d, R, UC::Fighter, "rep_arc170", "ARC-170 Squadron")
        .cost(430, 2, 1).stats(130, 25, 13, 18).tac(130, 76).tier(1).tech("rep_venator_program")
        .desc("Heavy three-seat fighter able to trade blows with warships.").add();
    U(d, R, UC::Fighter, "rep_delta7", "Delta-7 Aethersprite Squadron")
        .cost(390, 2, 1).stats(85, 35, 6, 25).tac(115, 105).tier(1).tech("rep_jedi_taskforce")
        .desc("Jedi-piloted interceptors; lethal against enemy squadrons.").add();
    U(d, R, UC::Bomber, "rep_ywing", "BTL-B Y-wing Squadron")
        .cost(410, 2, 1).stats(95, 0, 27, 3).tac(95, 70).tier(1).tech("rep_torpedo_doctrine")
        .desc("Proton torpedo bombers. Devastating against capital ships.").add();

    // --- warships ---
    U(d, R, UC::Corvette, "rep_consular", "Consular-class Cruiser")
        .cost(620, 3, 1).stats(280, 120, 14, 18).tac(180, 52).tier(1)
        .desc("Diplomatic cruiser pressed into escort duty.").add();
    U(d, R, UC::Frigate, "rep_pelta", "Pelta-class Frigate")
        .cost(1000, 4, 2).stats(560, 240, 20, 28).tac(210, 42).tier(2)
        .desc("Fleet support frigate with heavy point defence batteries.").add();
    U(d, R, UC::Frigate, "rep_arquitens", "Arquitens-class Light Cruiser")
        .cost(1250, 5, 2).stats(640, 280, 35, 22).tac(235, 40).tier(2)
        .desc("Compact line cruiser with an oversized main battery.").add();
    U(d, R, UC::Cruiser, "rep_acclamator", "Acclamator-class Assault Ship")
        .cost(2300, 8, 3).stats(1500, 620, 52, 30).tac(265, 32).tier(2).tech("rep_mobilisation")
        .wing("rep_v19", 2)
        .desc("Assault transport and carrier; the backbone of the early war.").add();
    U(d, R, UC::Capital, "rep_venator", "Venator-class Star Destroyer")
        .cost(4800, 12, 5).stats(3400, 1600, 115, 55).tac(320, 24).tier(3).tech("rep_venator_program")
        .wing("rep_v19", 2).wing("rep_arc170", 1).wing("rep_ywing", 2)
        .desc("Star Destroyer and fleet carrier. Launches five squadrons.").add();
    U(d, R, UC::Capital, "rep_victory", "Victory-class Star Destroyer")
        .cost(5600, 14, 5).stats(4200, 1900, 152, 40).tac(330, 22).tier(3).tech("rep_victory_program")
        .wing("rep_v19", 1)
        .desc("Dedicated line battleship built for the Outer Rim Sieges.").add();

    // --- ground forces ---
    U(d, R, UC::Infantry, "rep_clone", "Clone Trooper Platoon")
        .cost(220, 2, 1).stats(180, 0, 6, 16).tac(130, 27).tier(1)
        .desc("Kaminoan-trained troopers, the Republic's standard line unit.").add();
    U(d, R, UC::Infantry, "rep_clone_heavy", "Clone Heavy Weapons Squad")
        .cost(340, 3, 1).stats(230, 0, 24, 12).tac(150, 24).tier(1).tech("rep_clone_doctrine")
        .desc("Rocket launcher teams that shred armour columns.").add();
    U(d, R, UC::Infantry, "rep_arc_trooper", "ARC Trooper Squad")
        .cost(520, 4, 1).stats(310, 40, 20, 30).tac(145, 30).tier(2).tech("rep_clone_doctrine")
        .desc("Advanced Recon Commandos: elite, independent, expensive.").add();
    U(d, R, UC::Vehicle, "rep_atrt", "AT-RT Scout Walker")
        .cost(420, 3, 1).stats(340, 60, 17, 20).tac(165, 36).tier(1)
        .desc("Fast reconnaissance walker for screening advances.").add();
    U(d, R, UC::Vehicle, "rep_saber", "TX-130 Saber Tank")
        .cost(900, 4, 2).stats(780, 180, 42, 20).tac(190, 38).tier(2)
        .desc("Repulsor tank favoured by Jedi armour commanders.").add();
    U(d, R, UC::Vehicle, "rep_atte", "AT-TE Walker")
        .cost(1150, 5, 2).stats(1100, 200, 56, 24).tac(200, 22).tier(2)
        .desc("Six-legged siege walker; slow, armoured and relentless.").add();
    U(d, R, UC::Artillery, "rep_spha", "SPHA-T Artillery Walker")
        .cost(1400, 6, 2).stats(700, 0, 98, 10).tac(320, 16).tier(3).tech("rep_siege_warfare")
        .desc("Self-propelled heavy artillery. Outranges everything it faces.").add();
    U(d, R, UC::Vehicle, "rep_juggernaut", "HAVw A6 Juggernaut")
        .cost(1700, 7, 3).stats(1700, 300, 72, 42).tac(215, 28).tier(3).tech("rep_juggernaut")
        .desc("Ten-wheeled Turbo Tank that spearheads armoured columns.").add();
    U(d, R, UC::AirSupport, "rep_laat", "LAAT/i Gunship Flight")
        .cost(700, 3, 1).stats(420, 80, 31, 26).tac(150, 62).tier(2).tech("rep_mobilisation")
        .desc("Low Altitude Assault Transports providing close air support.").add();

    // --- heroes ---
    U(d, R, UC::Infantry, "rep_hero_obiwan", "Obi-Wan Kenobi")
        .cost(2200, 5, 1).stats(900, 300, 45, 70).tac(150, 34).tier(1).tech("rep_jedi_taskforce")
        .hero(0.12f, 0, 30)
        .desc("General Kenobi. Steadies any army he fights beside.").add();
    U(d, R, UC::Infantry, "rep_hero_anakin", "Anakin Skywalker")
        .cost(2400, 5, 1).stats(950, 280, 60, 80).tac(150, 40).tier(1).tech("rep_jedi_taskforce")
        .hero(0.15f, 0, 30)
        .desc("The Hero With No Fear. Reckless, brilliant, unstoppable.").add();
    U(d, R, UC::Infantry, "rep_hero_yoda", "Grand Master Yoda")
        .cost(3000, 6, 1).stats(1100, 420, 55, 95).tac(150, 36).tier(1).tech("rep_jedi_taskforce")
        .hero(0.18f, 0, 40)
        .desc("Nine centuries of the Force, condensed into one small general.").add();
    U(d, R, UC::Infantry, "rep_hero_mace", "Mace Windu")
        .cost(2600, 5, 1).stats(1000, 350, 55, 85).tac(150, 35).tier(1).tech("rep_jedi_taskforce")
        .hero(0.15f, 0, 35)
        .desc("Master of Vaapad and of the Jedi High Council.").add();
    U(d, R, UC::Infantry, "rep_hero_rex", "Captain Rex")
        .cost(1400, 4, 1).stats(600, 120, 35, 55).tac(160, 32).tier(1)
        .hero(0.08f, 0, 25)
        .desc("Veteran captain of the 501st. Clones fight harder beside him.").add();
    U(d, R, UC::Capital, "rep_hero_yularen", "Admiral Yularen (Resolute)")
        .cost(4200, 9, 4).stats(3600, 1900, 125, 70).tac(325, 26).tier(2).tech("rep_mobilisation")
        .wing("rep_v19", 2).wing("rep_ywing", 1)
        .hero(0.12f, 0, 45)
        .desc("Flag officer aboard the Resolute; a superb fleet tactician.").add();
    U(d, R, UC::Infantry, "rep_hero_bail", "Senator Bail Organa")
        .cost(1600, 4, 1).stats(320, 80, 8, 14).tac(120, 30).tier(1)
        .hero(0.02f, 260, 20)
        .desc("Alderaanian senator. His committees keep the credits flowing.").add();

    // =======================================================================
    // CONFEDERACY OF INDEPENDENT SYSTEMS
    // =======================================================================
    const Faction C = Faction::CIS;

    U(d, C, UC::Fighter, "cis_vulture", "Vulture Droid Squadron")
        .cost(200, 1, 1).stats(62, 0, 4, 14).tac(105, 95).tier(1)
        .desc("Expendable droid starfighters launched in overwhelming numbers.").add();
    U(d, C, UC::Fighter, "cis_tri", "Droid Tri-Fighter Squadron")
        .cost(380, 2, 1).stats(100, 20, 8, 24).tac(120, 100).tier(1).tech("cis_droid_fighters")
        .desc("Colicoid-built interceptors that hunt enemy squadrons.").add();
    U(d, C, UC::Bomber, "cis_hyena", "Hyena-class Bomber Squadron")
        .cost(380, 2, 1).stats(90, 0, 26, 3).tac(95, 72).tier(1).tech("cis_droid_fighters")
        .desc("Droid bombers carrying proton torpedoes and buzz droids.").add();

    U(d, C, UC::Corvette, "cis_diamond", "Diamond-class Cruiser")
        .cost(560, 3, 1).stats(260, 100, 13, 17).tac(180, 54).tier(1)
        .desc("Light Separatist escort, produced in enormous quantity.").add();
    U(d, C, UC::Frigate, "cis_munificent", "Munificent-class Star Frigate")
        .cost(1300, 5, 2).stats(700, 300, 42, 16).tac(250, 38).tier(2).tech("cis_munificent_line")
        .desc("Banking Clan frigate with long-range heavy turbolasers.").add();
    U(d, C, UC::Frigate, "cis_corona", "Corona-class Armed Frigate")
        .cost(980, 4, 2).stats(540, 220, 22, 30).tac(205, 44).tier(2)
        .desc("Fast escort frigate bristling with point defence.").add();
    U(d, C, UC::Cruiser, "cis_recusant", "Recusant-class Light Destroyer")
        .cost(2200, 8, 3).stats(1450, 450, 72, 18).tac(275, 34).tier(2)
        .desc("A spinal gun with engines welded on. Fragile but brutal.").add();
    U(d, C, UC::Capital, "cis_providence", "Providence-class Dreadnought")
        .cost(4700, 12, 5).stats(3300, 1500, 110, 45).tac(315, 25).tier(3).tech("cis_providence_program")
        .wing("cis_vulture", 3).wing("cis_hyena", 2)
        .desc("Command carrier of the Separatist navy. Launches five squadrons.").add();
    U(d, C, UC::Capital, "cis_lucrehulk", "Lucrehulk-class Battleship")
        .cost(6000, 16, 6).stats(5200, 2200, 118, 62).tac(300, 18).tier(3).tech("cis_lucrehulk_refit")
        .wing("cis_vulture", 6).wing("cis_hyena", 2)
        .desc("Converted Trade Federation core ship carrying eight squadrons.").add();

    U(d, C, UC::Infantry, "cis_b1", "B1 Battle Droid Platoon")
        .cost(140, 1, 1).stats(140, 0, 5, 12).tac(130, 26).tier(1)
        .desc("Cheap, stupid and utterly inexhaustible.").add();
    U(d, C, UC::Infantry, "cis_b2", "B2 Super Battle Droid Squad")
        .cost(320, 3, 1).stats(270, 0, 15, 21).tac(135, 24).tier(1).tech("cis_mass_production")
        .desc("Armoured droids with integrated wrist blasters.").add();
    U(d, C, UC::Infantry, "cis_droideka", "Droideka Squad")
        .cost(520, 4, 1).stats(280, 200, 18, 32).tac(140, 28).tier(2)
        .desc("Destroyer droids behind deflector shields. Hard to dislodge.").add();
    U(d, C, UC::Infantry, "cis_magna", "IG-100 Magnaguard Squad")
        .cost(560, 4, 1).stats(330, 40, 26, 22).tac(140, 32).tier(2).tech("cis_dark_acolytes")
        .desc("Grievous' bodyguards; electrostaffs that can duel a Jedi.").add();
    U(d, C, UC::Vehicle, "cis_aat", "AAT Battle Tank")
        .cost(820, 4, 2).stats(720, 140, 40, 20).tac(190, 34).tier(2)
        .desc("Armoured Assault Tank, the mainstay of Baktoid armour.").add();
    U(d, C, UC::Vehicle, "cis_hailfire", "Hailfire Droid")
        .cost(900, 4, 2).stats(520, 0, 66, 8).tac(230, 44).tier(2).tech("cis_techno_union")
        .desc("Missile racks on wheels. Murders armour, dies to infantry.").add();
    U(d, C, UC::Vehicle, "cis_octuptarra", "Octuptarra Tri-Droid")
        .cost(850, 4, 2).stats(680, 100, 24, 42).tac(185, 30).tier(2).tech("cis_techno_union")
        .desc("Three-legged anti-infantry and anti-air platform.").add();
    U(d, C, UC::Artillery, "cis_spider", "OG-9 Homing Spider Droid")
        .cost(1350, 6, 2).stats(780, 0, 92, 12).tac(310, 18).tier(3).tech("cis_techno_union")
        .desc("Walking heavy laser platform used to crack fortifications.").add();
    U(d, C, UC::Vehicle, "cis_crab", "NR-N99 Persuader Droid")
        .cost(1500, 6, 3).stats(1500, 250, 62, 36).tac(210, 26).tier(3).tech("cis_bio_droids")
        .desc("Heavy tank droid built around a single massive cannon.").add();
    U(d, C, UC::AirSupport, "cis_hyena_air", "Hyena Bomber Strike Flight")
        .cost(650, 3, 1).stats(380, 0, 36, 14).tac(150, 64).tier(2).tech("cis_droid_fighters")
        .desc("Atmospheric droid bombers running ground attack sorties.").add();

    U(d, C, UC::Infantry, "cis_hero_grievous", "General Grievous")
        .cost(2600, 5, 1).stats(1000, 250, 60, 85).tac(150, 38).tier(1).tech("cis_dark_acolytes")
        .hero(0.16f, 0, 35)
        .desc("Supreme Commander of the Droid Armies. Collects lightsabers.").add();
    U(d, C, UC::Infantry, "cis_hero_dooku", "Count Dooku")
        .cost(3000, 6, 1).stats(1050, 400, 58, 92).tac(150, 36).tier(1).tech("cis_dark_acolytes")
        .hero(0.18f, 120, 40)
        .desc("Head of State of the Confederacy and a Lord of the Sith.").add();
    U(d, C, UC::Infantry, "cis_hero_ventress", "Asajj Ventress")
        .cost(2000, 5, 1).stats(820, 240, 42, 72).tac(145, 40).tier(1).tech("cis_dark_acolytes")
        .hero(0.12f, 0, 28)
        .desc("Dathomirian assassin and Dooku's favoured blade.").add();
    U(d, C, UC::Infantry, "cis_hero_durge", "Durge")
        .cost(1700, 4, 1).stats(900, 180, 48, 50).tac(150, 34).tier(1).tech("cis_dark_acolytes")
        .hero(0.10f, 0, 26)
        .desc("Gen'Dai bounty hunter with a centuries-old grudge against clones.").add();
    U(d, C, UC::Capital, "cis_hero_trench", "Admiral Trench (Invincible)")
        .cost(4300, 9, 4).stats(3400, 1750, 130, 55).tac(330, 26).tier(2).tech("cis_munificent_line")
        .wing("cis_vulture", 3)
        .hero(0.13f, 0, 45)
        .desc("Harch admiral famed for blockades and ambushes.").add();
    U(d, C, UC::Capital, "cis_hero_malevolence", "Malevolence")
        .cost(7000, 18, 6).stats(6000, 2600, 175, 75).tac(360, 18).tier(3).tech("cis_lucrehulk_refit")
        .wing("cis_vulture", 4).wing("cis_hyena", 2)
        .hero(0.20f, 0, 0)
        .desc("Subjugator-class heavy cruiser mounting twin ion pulse cannons.").add();
    U(d, C, UC::Infantry, "cis_hero_gunray", "Viceroy Nute Gunray")
        .cost(1600, 4, 1).stats(300, 60, 6, 12).tac(120, 28).tier(1)
        .hero(0.02f, 300, 20)
        .desc("Trade Federation viceroy. Cowardly, but very good with money.").add();

    // =======================================================================
    // HUTT CARTELS
    // =======================================================================
    const Faction H = Faction::Hutts;

    U(d, H, UC::Fighter, "hutt_z95", "Z-95 Headhunter Squadron")
        .cost(220, 1, 1).stats(75, 0, 5, 15).tac(110, 88).tier(1)
        .desc("Surplus headhunters flown by cartel pilots and pirates.").add();
    U(d, H, UC::Bomber, "hutt_skipray", "Skipray Blastboat Squadron")
        .cost(360, 2, 1).stats(115, 30, 23, 9).tac(105, 74).tier(1)
        .desc("Heavily armed patrol boats used as improvised bombers.").add();
    U(d, H, UC::Corvette, "hutt_cr90", "Modified CR90 Corvette")
        .cost(640, 3, 1).stats(300, 130, 15, 21).tac(185, 56).tier(1)
        .desc("Blockade runner refitted with smuggled weapon mounts.").add();
    U(d, H, UC::Frigate, "hutt_action6", "Armed Action VI Transport")
        .cost(950, 4, 2).stats(720, 180, 22, 14).tac(200, 34).tier(2)
        .desc("Bulk freighter plated in scrap armour. Soaks up fire cheaply.").add();
    U(d, H, UC::Frigate, "hutt_dp20", "Corellian Gunship")
        .cost(1150, 5, 2).stats(560, 260, 26, 48).tac(215, 46).tier(2).tech("hutt_pirate_fleets")
        .desc("DP20 gunship. Sweeps enemy squadrons out of the sky.").add();
    U(d, H, UC::Cruiser, "hutt_kaloth", "Kaloth Battlecruiser")
        .cost(2100, 8, 3).stats(1350, 500, 60, 32).tac(265, 32).tier(2).tech("hutt_pirate_fleets")
        .wing("hutt_z95", 1)
        .desc("Ancient cartel warship, endlessly patched and re-armed.").add();
    U(d, H, UC::Cruiser, "hutt_dreadnaught", "Dreadnaught-class Heavy Cruiser")
        .cost(2600, 9, 4).stats(1900, 700, 74, 26).tac(285, 26).tier(3)
        .desc("Six-hundred-metre relic crewed by a small army of thugs.").add();
    U(d, H, UC::Capital, "hutt_providence_bm", "Black Market Providence")
        .cost(5200, 13, 5).stats(3200, 1400, 105, 44).tac(315, 24).tier(3).tech("hutt_black_market")
        .wing("hutt_z95", 2).wing("hutt_skipray", 2)
        .desc("Separatist dreadnought bought, stolen, and refitted at Nal Hutta.").add();

    U(d, H, UC::Infantry, "hutt_thug", "Cartel Enforcers")
        .cost(160, 2, 1).stats(150, 0, 5, 14).tac(125, 28).tier(1)
        .desc("Hired guns. Not disciplined, but there are always more.").add();
    U(d, H, UC::Infantry, "hutt_gamorrean", "Gamorrean Heavies")
        .cost(300, 3, 1).stats(330, 0, 10, 19).tac(115, 22).tier(1).tech("hutt_enforcers")
        .desc("Palace guards who close to axe range and stay there.").add();
    U(d, H, UC::Infantry, "hutt_nikto", "Nikto Raiders")
        .cost(280, 2, 1).stats(200, 0, 18, 12).tac(150, 30).tier(1).tech("hutt_enforcers")
        .desc("Raider teams with looted rocket launchers.").add();
    U(d, H, UC::Infantry, "hutt_merc", "Mercenary Squad")
        .cost(480, 3, 1).stats(290, 60, 23, 26).tac(150, 31).tier(2).tech("hutt_bounty_contracts")
        .desc("Professional soldiers of fortune with proper equipment.").add();
    U(d, H, UC::Vehicle, "hutt_hover", "Ubrikkian Hover Tank")
        .cost(760, 4, 2).stats(600, 100, 32, 24).tac(180, 40).tier(2)
        .desc("Luxury repulsor hull converted into a fast gun platform.").add();
    U(d, H, UC::Vehicle, "hutt_aat", "Black Market AAT")
        .cost(880, 4, 2).stats(700, 120, 38, 20).tac(190, 34).tier(2).tech("hutt_war_machines")
        .desc("Separatist tanks that fell off the back of a freighter.").add();
    U(d, H, UC::Artillery, "hutt_proton", "Mobile Proton Cannon")
        .cost(1250, 5, 2).stats(620, 0, 88, 10).tac(300, 17).tier(3).tech("hutt_war_machines")
        .desc("Siege gun towed from world to world by the highest bidder.").add();
    U(d, H, UC::AirSupport, "hutt_skiff", "Skiff Gunship Flight")
        .cost(640, 3, 1).stats(360, 40, 27, 25).tac(150, 66).tier(2)
        .desc("Armed desert skiffs making strafing runs.").add();

    U(d, H, UC::Infantry, "hutt_hero_jabba", "Jabba the Hutt")
        .cost(2000, 5, 1).stats(500, 150, 10, 20).tac(120, 18).tier(1)
        .hero(0.05f, 380, 25)
        .desc("Lord of the Desilijic clan. His ledgers fund entire fleets.").add();
    U(d, H, UC::Infantry, "hutt_hero_bane", "Cad Bane")
        .cost(2200, 5, 1).stats(760, 180, 44, 68).tac(165, 36).tier(1).tech("hutt_bounty_contracts")
        .hero(0.14f, 0, 30)
        .desc("Duros bounty hunter who has killed Jedi for the right price.").add();
    U(d, H, UC::Infantry, "hutt_hero_bossk", "Bossk")
        .cost(1800, 4, 1).stats(820, 140, 40, 55).tac(150, 32).tier(1).tech("hutt_bounty_contracts")
        .hero(0.11f, 0, 26)
        .desc("Trandoshan hunter with regenerative hide and a mortar launcher.").add();
    U(d, H, UC::Infantry, "hutt_hero_aurra", "Aurra Sing")
        .cost(1700, 4, 1).stats(700, 130, 34, 62).tac(180, 34).tier(1).tech("hutt_bounty_contracts")
        .hero(0.10f, 0, 26)
        .desc("Sniper and former Jedi padawan turned assassin.").add();
    U(d, H, UC::Capital, "hutt_hero_toth", "Captain Toth (Sabaoth Destroyer)")
        .cost(4000, 9, 4).stats(3100, 1500, 115, 52).tac(320, 28).tier(2).tech("hutt_pirate_fleets")
        .wing("hutt_z95", 2)
        .hero(0.12f, 0, 45)
        .desc("Sabaoth mercenary captain commanding a one-of-a-kind destroyer.").add();

    // =======================================================================
    // NEUTRAL / MILITIA - garrisons of unaligned worlds
    // =======================================================================
    const Faction N = Faction::Neutral;
    U(d, N, UC::Fighter, "neu_fighter", "Local Defence Squadron")
        .cost(200, 1, 1).stats(65, 0, 4, 13).tac(105, 85).tier(1)
        .desc("Planetary defence force fighters.").add();
    U(d, N, UC::Corvette, "neu_corvette", "System Patrol Corvette")
        .cost(560, 3, 1).stats(260, 90, 12, 16).tac(175, 50).tier(1)
        .desc("Customs cutter armed for anti-piracy work.").add();
    U(d, N, UC::Frigate, "neu_frigate", "Mercenary Frigate")
        .cost(1000, 4, 2).stats(540, 200, 26, 18).tac(210, 38).tier(2)
        .desc("Hired warship guarding an unaligned world.").add();
    U(d, N, UC::Infantry, "neu_militia", "Planetary Militia")
        .cost(150, 2, 1).stats(160, 0, 6, 13).tac(130, 26).tier(1)
        .desc("Local defence volunteers with surplus weapons.").add();
    U(d, N, UC::Vehicle, "neu_militia_tank", "Militia Armour")
        .cost(600, 3, 2).stats(520, 80, 28, 18).tac(180, 32).tier(2)
        .desc("A handful of old tanks kept running by the local garrison.").add();

    // =======================================================================
    // Role and manufacturer, shown on the unit info card.
    // =======================================================================
    struct Flavour {
        const char* key;
        const char* role;
        const char* maker;
    };
    static const Flavour kFlavour[] = {
        // Republic
        {"rep_v19", "Interceptor", "Slayn & Korpil"},
        {"rep_arc170", "Heavy Fighter", "Incom / Subpro"},
        {"rep_delta7", "Jedi Interceptor", "Kuat Systems Engineering"},
        {"rep_ywing", "Torpedo Bomber", "Koensayr Manufacturing"},
        {"rep_consular", "Picket", "Corellian Engineering Corporation"},
        {"rep_pelta", "Fleet Support", "Kuat Drive Yards"},
        {"rep_arquitens", "Line Cruiser", "Kuat Drive Yards"},
        {"rep_acclamator", "Assault Carrier", "Rothana Heavy Engineering"},
        {"rep_venator", "Star Destroyer / Carrier", "Kuat Drive Yards"},
        {"rep_victory", "Line Battleship", "Kuat Drive Yards"},
        {"rep_clone", "Line Infantry", "Kaminoan Cloners"},
        {"rep_clone_heavy", "Anti-Armour Infantry", "Kaminoan Cloners"},
        {"rep_arc_trooper", "Special Forces", "Kaminoan Cloners"},
        {"rep_atrt", "Scout Walker", "Kuat Drive Yards"},
        {"rep_saber", "Repulsor Tank", "Rothana Heavy Engineering"},
        {"rep_atte", "Assault Walker", "Rothana Heavy Engineering"},
        {"rep_spha", "Siege Artillery", "Rothana Heavy Engineering"},
        {"rep_juggernaut", "Heavy Assault Vehicle", "Kuat Drive Yards"},
        {"rep_laat", "Close Air Support", "Rothana Heavy Engineering"},
        {"rep_hero_obiwan", "Jedi General", "Jedi Order"},
        {"rep_hero_anakin", "Jedi General", "Jedi Order"},
        {"rep_hero_yoda", "Grand Master", "Jedi Order"},
        {"rep_hero_mace", "Jedi General", "Jedi Order"},
        {"rep_hero_rex", "Clone Captain", "Kaminoan Cloners"},
        {"rep_hero_yularen", "Fleet Command", "Kuat Drive Yards"},
        {"rep_hero_bail", "Senator", "Galactic Senate"},
        // Confederacy
        {"cis_vulture", "Droid Interceptor", "Haor Chall Engineering"},
        {"cis_tri", "Droid Interceptor", "Colicoid Creation Nest"},
        {"cis_hyena", "Droid Bomber", "Baktoid Armour Workshop"},
        {"cis_diamond", "Picket", "Haor Chall Engineering"},
        {"cis_munificent", "Long Range Frigate", "Hoersch-Kessel Drive"},
        {"cis_corona", "Escort Frigate", "Haor Chall Engineering"},
        {"cis_recusant", "Light Destroyer", "Free Dac Volunteers Engineering"},
        {"cis_providence", "Dreadnought / Carrier", "Free Dac Volunteers Engineering"},
        {"cis_lucrehulk", "Battleship / Carrier", "Hoersch-Kessel Drive"},
        {"cis_b1", "Line Infantry", "Baktoid Combat Automata"},
        {"cis_b2", "Heavy Infantry", "Baktoid Combat Automata"},
        {"cis_droideka", "Shielded Assault Droid", "Colicoid Creation Nest"},
        {"cis_magna", "Bodyguard Droid", "Holowan Mechanicals"},
        {"cis_aat", "Battle Tank", "Baktoid Armour Workshop"},
        {"cis_hailfire", "Anti-Armour Missile Platform", "Haor Chall Engineering"},
        {"cis_octuptarra", "Anti-Infantry Walker", "Techno Union"},
        {"cis_spider", "Siege Walker", "Baktoid Armour Workshop"},
        {"cis_crab", "Heavy Tank Droid", "Baktoid Armour Workshop"},
        {"cis_hyena_air", "Close Air Support", "Baktoid Armour Workshop"},
        {"cis_hero_grievous", "Supreme Commander", "Confederacy High Command"},
        {"cis_hero_dooku", "Head of State", "Confederacy High Command"},
        {"cis_hero_ventress", "Assassin", "Confederacy High Command"},
        {"cis_hero_durge", "Bounty Hunter", "Independent"},
        {"cis_hero_trench", "Fleet Command", "Hoersch-Kessel Drive"},
        {"cis_hero_malevolence", "Heavy Cruiser", "Free Dac Volunteers Engineering"},
        {"cis_hero_gunray", "Viceroy", "Trade Federation"},
        // Hutt Cartels
        {"hutt_z95", "Interceptor", "Incom / Subpro"},
        {"hutt_skipray", "Assault Gunboat", "Sienar Fleet Systems"},
        {"hutt_cr90", "Blockade Runner", "Corellian Engineering Corporation"},
        {"hutt_action6", "Armed Freighter", "Corellian Engineering Corporation"},
        {"hutt_dp20", "Anti-Fighter Gunship", "Corellian Engineering Corporation"},
        {"hutt_kaloth", "Battlecruiser", "Kaloth Shipwrights"},
        {"hutt_dreadnaught", "Heavy Cruiser", "Rendili StarDrive"},
        {"hutt_providence_bm", "Dreadnought / Carrier", "Free Dac (stolen hull)"},
        {"hutt_thug", "Militia Infantry", "Cartel Armouries"},
        {"hutt_gamorrean", "Shock Infantry", "Cartel Armouries"},
        {"hutt_nikto", "Anti-Armour Infantry", "Cartel Armouries"},
        {"hutt_merc", "Professional Infantry", "Cartel Armouries"},
        {"hutt_hover", "Repulsor Tank", "Ubrikkian Industries"},
        {"hutt_aat", "Battle Tank", "Baktoid (stolen)"},
        {"hutt_proton", "Siege Artillery", "Merr-Sonn Munitions"},
        {"hutt_skiff", "Close Air Support", "Ubrikkian Industries"},
        {"hutt_hero_jabba", "Crime Lord", "Desilijic Clan"},
        {"hutt_hero_bane", "Bounty Hunter", "Independent"},
        {"hutt_hero_bossk", "Bounty Hunter", "Independent"},
        {"hutt_hero_aurra", "Assassin", "Independent"},
        {"hutt_hero_toth", "Mercenary Flagship", "Sabaoth Squadron"},
        // Neutral
        {"neu_fighter", "Defence Squadron", "Local Industry"},
        {"neu_corvette", "Customs Cutter", "Local Industry"},
        {"neu_frigate", "Hired Warship", "Independent"},
        {"neu_militia", "Militia Infantry", "Local Industry"},
        {"neu_militia_tank", "Militia Armour", "Local Industry"},
    };
    for (const Flavour& f : kFlavour) d.setUnitFlavour(f.key, f.role, f.maker);
}

}  // namespace content
}  // namespace gc
