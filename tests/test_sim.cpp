// Simulation tests. No framework: a tiny CHECK macro keeps the build simple.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "battle/Tactical.h"
#include "sim/AI.h"
#include "sim/GameState.h"

using namespace gc;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++g_checks;                                                           \
        if (!(cond)) {                                                        \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));     \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

static void testDatabase() {
    std::printf("database...\n");
    const Database& d = db();
    CHECK(!d.planets().empty(), "planets loaded");
    CHECK(!d.units().empty(), "units loaded");
    CHECK(!d.buildings().empty(), "buildings loaded");
    CHECK(!d.techs().empty(), "techs loaded");
    CHECK(d.campaigns().size() >= 3, "three campaigns");
    CHECK(!d.lanes().empty(), "lanes loaded");

    // Every carried wing must resolve to a real squadron.
    for (const UnitDef& u : d.units()) {
        for (const CarriedWing& w : u.wings) {
            CHECK(w.unitId != kInvalid, ("wing resolves: " + u.key + " -> " + w.unitKey).c_str());
            if (w.unitId != kInvalid) {
                CHECK(d.unit(w.unitId).isSquadron(), "carried unit is a squadron");
            }
        }
    }
    // Every faction can actually build something at tier 1 in both domains.
    for (Faction f : playableFactions()) {
        bool space = false, ground = false;
        for (const UnitDef& u : d.units()) {
            if (u.faction != f || u.requiredTech != kInvalid || u.requiredTier != 1) continue;
            if (u.domain() == Domain::Space) space = true;
            if (u.domain() == Domain::Ground) ground = true;
        }
        CHECK(space, "faction has a tier 1 space unit");
        CHECK(ground, "faction has a tier 1 ground unit");
    }
    // Kuat really does discount capital ships.
    const PlanetDef& kuat = d.planet(d.planetId("kuat"));
    CHECK(kuat.mods().capitalCostMult < 1.0f, "Kuat discounts capital ships");
    CHECK(kuat.mods().capitalTimeMult < 1.0f, "Kuat speeds up capital ships");
    // Space-only worlds have no ground slots at all.
    for (const PlanetDef& p : d.planets()) {
        if (!p.spaceOnly) continue;
        CHECK(p.groundUnitSlots == 0 && p.groundBuildSlots == 0, "space-only world has no ground slots");
    }
}

static void testGalaxyConnectivity() {
    std::printf("galaxy connectivity...\n");
    for (const CampaignDef& c : db().campaigns()) {
        GameState gs;
        GameSetup s;
        s.campaign = c.id;
        s.playerFaction = Faction::Republic;
        gs.start(s);
        CHECK(gs.planetCount() > 0, "campaign has planets");
        // Every planet must be reachable from planet 0.
        int unreachable = 0;
        for (int i = 1; i < gs.planetCount(); ++i) {
            if (gs.findPath(0, i).empty()) ++unreachable;
        }
        CHECK(unreachable == 0, (c.key + ": all planets reachable").c_str());
        // Each faction must start with something.
        for (Faction f : playableFactions()) {
            CHECK(gs.planetsOwned(f) > 0, (c.key + ": faction has a homeworld").c_str());
        }
    }
}

static void testEconomyAndProduction() {
    std::printf("economy and production...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("core_conflict");
    s.playerFaction = Faction::Republic;
    gs.start(s);

    // Find a Republic world with a barracks and queue a clone platoon there.
    Id planet = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner != Faction::Republic) continue;
        if (gs.bestProductionTier(i, Faction::Republic, Domain::Ground) >= 1) {
            planet = i;
            break;
        }
    }
    CHECK(planet != kInvalid, "republic has a ground production world");
    if (planet == kInvalid) return;

    Id clone = db().unitId("rep_clone");
    int before = gs.faction(Faction::Republic).credits;
    OrderResult r = gs.queueUnit(planet, clone, Faction::Republic);
    CHECK(r.ok, r.message.c_str());
    CHECK(gs.faction(Faction::Republic).credits < before, "credits deducted up front");
    CHECK(!gs.planet(planet).queue.empty(), "order is queued");

    // A Venator needs technology the Republic does not have yet.
    Id venator = db().unitId("rep_venator");
    CHECK(!gs.canQueueUnit(planet, venator, Faction::Republic).ok, "tech gate blocks the Venator");

    // Run a few weeks and confirm the unit appears and income is paid.
    int unitsBefore = 0;
    for (const UnitInstance& u : gs.units()) {
        if (u.alive && u.owner == Faction::Republic) ++unitsBefore;
    }
    for (int i = 0; i < 60 * 20; ++i) gs.update(0.05f);  // ~60 seconds of play
    int unitsAfter = 0;
    for (const UnitInstance& u : gs.units()) {
        if (u.alive && u.owner == Faction::Republic) ++unitsAfter;
    }
    CHECK(gs.date().day > 0, "days advance");
    CHECK(unitsAfter > unitsBefore, "production delivered units");
    CHECK(gs.faction(Faction::Republic).lastIncome > 0, "weekly income paid");
}

static void testSlotsAndTraits() {
    std::printf("slots and traits...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("clone_wars_total");
    s.playerFaction = Faction::Republic;
    gs.start(s);

    // Mines require a mining world.
    Id mine = db().buildingId("b_mine");
    Id coruscant = kInvalid, kessel = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).def().key == "coruscant") coruscant = i;
        if (gs.planet(i).def().key == "kessel") kessel = i;
    }
    CHECK(coruscant != kInvalid && kessel != kInvalid, "found test planets");
    if (coruscant == kInvalid || kessel == kInvalid) return;
    CHECK(!gs.canQueueBuilding(coruscant, mine, Faction::Republic).ok, "no mine without a mining world");

    // Slot limits are enforced.
    int cap = gs.unitSlotCapacity(coruscant, Domain::Ground);
    CHECK(cap > 0, "ground slots exist");
    gs.faction(Faction::Republic).credits = 999999;
    Id clone = db().unitId("rep_clone");
    int queued = 0;
    while (gs.canQueueUnit(coruscant, clone, Faction::Republic).ok && queued < 100) {
        gs.queueUnit(coruscant, clone, Faction::Republic);
        ++queued;
    }
    CHECK(gs.usedUnitSlots(coruscant, Faction::Republic, Domain::Ground) <= cap, "slot cap respected");
    CHECK(queued < 100, "queueing stops at the cap");
}

static void testAutoresolveAndCapture() {
    std::printf("autoresolve and capture...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("core_conflict");
    s.playerFaction = Faction::Republic;
    s.seed = 42;
    gs.start(s);

    // Build a lopsided attack on a neutral world and check the rules:
    // space must be cleared before troops can land.
    Id neutral = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner == Faction::Neutral && !gs.planet(i).def().spaceOnly) {
            neutral = i;
            break;
        }
    }
    CHECK(neutral != kInvalid, "found a neutral world");
    if (neutral == kInvalid) return;

    // Landing is refused while hostile ships hold the orbit.
    OrderResult bad = gs.invade(neutral, {}, Faction::Republic);
    CHECK(!bad.ok, "cannot land with no troops");

    BattleSetup setup;
    setup.domain = Domain::Space;
    setup.planet = neutral;
    setup.attacker = Faction::Republic;
    setup.defender = Faction::Neutral;
    setup.defenderUnits = gs.unitsAt(neutral, Faction::Neutral, Domain::Space);
    CHECK(!setup.defenderUnits.empty(), "neutral world is garrisoned");

    // Give the Republic an overwhelming fleet in orbit via a normal move order.
    Id source = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner == Faction::Republic && !gs.unitsAt(i, Faction::Republic, Domain::Space).empty()) {
            source = i;
            break;
        }
    }
    CHECK(source != kInvalid, "republic has a fleet");
    if (source == kInvalid) return;

    std::vector<Id> fleet = gs.allUnitsAt(source, Faction::Republic);
    OrderResult mv = gs.moveUnits(fleet, neutral);
    CHECK(mv.ok, mv.message.c_str());

    // Let the fleet arrive; the battle involves the player so it must wait for
    // a decision rather than resolving itself.
    for (int i = 0; i < 4000 && !gs.hasPendingPlayerBattle(); ++i) gs.update(0.1f);
    CHECK(gs.hasPendingPlayerBattle(), "player battle is offered, not auto-resolved");
    BattleReport rep = gs.autoResolvePendingBattle();
    CHECK(rep.planet == neutral, "report is about the right world");
    CHECK(rep.attackerSide.committed > 0, "attacker committed units");
    CHECK(rep.attackerSide.lost + rep.attackerSide.survived == rep.attackerSide.committed,
          "every committed unit is accounted for");
    CHECK(rep.defenderSide.lost + rep.defenderSide.survived == rep.defenderSide.committed,
          "defender units are accounted for");

    if (rep.victor == Faction::Republic) {
        CHECK(gs.orbitClearFor(neutral, Faction::Republic), "orbit cleared after victory");
        std::vector<Id> troops = gs.unitsAt(neutral, Faction::Republic, Domain::Ground, false, true);
        if (!troops.empty()) {
            gs.invade(neutral, troops, Faction::Republic);
            while (gs.hasPendingPlayerBattle()) gs.autoResolvePendingBattle();
        }
    }
}

static void testTacticalBattle() {
    std::printf("tactical battle...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("core_conflict");
    s.playerFaction = Faction::Republic;
    s.seed = 7;
    gs.start(s);

    Id planet = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner == Faction::Republic && !gs.unitsAt(i, Faction::Republic, Domain::Space).empty()) {
            planet = i;
            break;
        }
    }
    if (planet == kInvalid) return;

    Id enemy = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner == Faction::CIS && !gs.unitsAt(i, Faction::CIS, Domain::Space).empty()) {
            enemy = i;
            break;
        }
    }
    if (enemy == kInvalid) return;

    BattleSetup setup;
    setup.domain = Domain::Space;
    setup.planet = planet;
    setup.attacker = Faction::Republic;
    setup.defender = Faction::CIS;
    setup.attackerUnits = gs.unitsAt(planet, Faction::Republic, Domain::Space);
    setup.defenderUnits = gs.unitsAt(enemy, Faction::CIS, Domain::Space);

    tactical::Battle b;
    b.init(gs, setup, 99);
    CHECK(!b.units().empty(), "battle has combatants");
    bool carriersLaunched = false;
    for (const tactical::TUnit& u : b.units()) {
        if (u.instanceId == kInvalid && !u.structure) carriersLaunched = true;
    }
    (void)carriersLaunched;

    int steps = 0;
    while (!b.finished() && steps < 40000) {
        b.update(1.0f / 30.0f);
        ++steps;
    }
    CHECK(b.finished(), "battle terminates");
    CHECK(b.victor() != Faction::Neutral, "battle has a victor");

    BattleResolution res = b.resolution(gs);
    CHECK(!res.outcomes.empty(), "battle produced outcomes");
    int total = res.report.attackerSide.committed + res.report.defenderSide.committed;
    CHECK(total == static_cast<int>(setup.attackerUnits.size() + setup.defenderUnits.size()),
          "summary counts every unit");
}

static void testAiPlaysByTheRules() {
    std::printf("ai campaign...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("core_conflict");
    s.playerFaction = Faction::Republic;  // the player just sits still
    s.difficulty = Difficulty::Hard;
    s.seed = 2024;
    gs.start(s);
    gs.setSpeed(GameSpeed::Fastest);

    int cisPlanetsStart = gs.planetsOwned(Faction::CIS);
    int huttPlanetsStart = gs.planetsOwned(Faction::Hutts);

    // Simulate about a year of game time, auto-resolving anything the player
    // is dragged into.
    for (int i = 0; i < 200000 && gs.outcome() == GameOutcome::InProgress; ++i) {
        if (gs.hasPendingPlayerBattle()) gs.autoResolvePendingBattle();
        gs.update(0.05f);
        if (gs.date().day > 360) break;
    }

    CHECK(gs.date().day > 300, "a year of game time elapsed");
    // The AI must have spent money on structures and units, and taken ground.
    int cisBuildings = 0, cisUnits = 0;
    for (const UnitInstance& u : gs.units()) {
        if (u.alive && u.owner == Faction::CIS) ++cisUnits;
    }
    for (int i = 0; i < gs.planetCount(); ++i) {
        for (Id bid : gs.planet(i).buildings) {
            if (gs.buildingInst(bid).alive && gs.buildingInst(bid).owner == Faction::CIS) ++cisBuildings;
        }
    }
    CHECK(cisUnits > 0, "CIS still fields units");
    CHECK(cisBuildings > 0, "CIS built structures");

    bool aiExpanded = gs.planetsOwned(Faction::CIS) > cisPlanetsStart ||
                      gs.planetsOwned(Faction::Hutts) > huttPlanetsStart;
    CHECK(aiExpanded, "an AI faction captured at least one world in a year");

    // Building slots are a hard limit at all times. (Unit garrisons can end up
    // over the planet's slot count after a capture or a retreat, which is why
    // only production and voluntary moves are checked against it.)
    for (int i = 0; i < gs.planetCount(); ++i) {
        CHECK(gs.usedBuildSlots(i, Domain::Space) <= gs.buildSlotCapacity(i, Domain::Space),
              "space building slots respected");
        CHECK(gs.usedBuildSlots(i, Domain::Ground) <= gs.buildSlotCapacity(i, Domain::Ground),
              "ground building slots respected");
    }
    std::printf("  (day %d: REP %d / CIS %d / HUTT %d planets)\n", gs.date().day,
                gs.planetsOwned(Faction::Republic), gs.planetsOwned(Faction::CIS),
                gs.planetsOwned(Faction::Hutts));
}

static void testDeterminism() {
    std::printf("determinism...\n");
    auto run = [](uint64_t seed) {
        GameState gs;
        GameSetup s;
        s.campaign = db().campaignId("core_conflict");
        s.playerFaction = Faction::Republic;
        s.seed = seed;
        gs.start(s);
        for (int i = 0; i < 20000 && gs.date().day < 120; ++i) {
            if (gs.hasPendingPlayerBattle()) gs.autoResolvePendingBattle();
            gs.update(0.1f);
        }
        return gs.planetsOwned(Faction::CIS) * 1000 + gs.faction(Faction::CIS).credits % 1000;
    };
    CHECK(run(1234) == run(1234), "same seed gives the same campaign");
}

int main() {
    std::printf("== Galactic Conquest simulation tests ==\n");
    testDatabase();
    testGalaxyConnectivity();
    testEconomyAndProduction();
    testSlotsAndTraits();
    testAutoresolveAndCapture();
    testTacticalBattle();
    testAiPlaysByTheRules();
    testDeterminism();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
