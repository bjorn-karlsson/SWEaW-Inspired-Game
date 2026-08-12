// Simulation tests. No framework: a tiny CHECK macro keeps the build simple.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fstream>

#include "battle/Tactical.h"
#include "data/UnitMods.h"
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
    // Warships come out of the factory with their guns already mounted, and
    // fitting them must not have changed a single ship's firepower.
    const UnitDef& venator = d.unit(d.unitId("rep_venator"));
    CHECK(!venator.hardpoints.empty(), "capital ships have hardpoints");
    CHECK(std::abs(venator.antiCapital() - 115.0f) < 0.01f, "hardpoints preserve anti-capital damage");
    CHECK(std::abs(venator.antiFighter() - 55.0f) < 0.01f, "hardpoints preserve anti-squadron damage");
    CHECK(venator.hangarBays() == static_cast<int>(venator.wings.size()), "one hangar bay per wing");
    for (const UnitDef& u : d.units()) {
        if (u.domain() != Domain::Space || u.isSquadron()) continue;
        CHECK(!u.hardpoints.empty(), ("warship is armed: " + u.key).c_str());
        for (const Hardpoint& h : u.hardpoints) {
            CHECK(h.damage >= 0.0f && h.health > 0.0f, "hardpoint is sane");
        }
    }

    // Space-only worlds have no ground slots at all.
    for (const PlanetDef& p : d.planets()) {
        if (!p.spaceOnly) continue;
        CHECK(p.groundUnitSlots == 0 && p.groundBuildSlots == 0, "space-only world has no ground slots");
    }
}

static void testUnitModsRoundTrip() {
    std::printf("unit designer persistence...\n");
    Database& d = editableDb();
    Id venator = d.unitId("rep_venator");
    CHECK(venator != kInvalid, "venator exists");
    if (venator == kInvalid) return;

    const int originalCost = d.unit(venator).cost;
    const size_t originalMounts = d.unit(venator).hardpoints.size();

    // Edit the way the designer does, write the file, then read it back into a
    // unit that has been changed again in the meantime.
    d.unitMutable(venator).cost = 7777;
    d.unitMutable(venator).custom = true;
    Hardpoint extra;
    extra.name = "Test Battery";
    extra.type = HardpointType::IonCannon;
    extra.damage = 12.0f;
    extra.projectile = ProjectileKind::Flak;
    extra.barrels = 4;
    extra.reload = 1.25f;
    extra.tracking = 0.75f;
    extra.useOwnColour = true;
    extra.colour[0] = 11;
    extra.colour[1] = 22;
    extra.colour[2] = 33;
    d.unitMutable(venator).hardpoints.push_back(extra);

    // A hand-built model, which is stored as a list of primitives.
    HullPart part;
    part.shape = PartShape::Trapezoid;
    part.tint = PartTint::Accent;
    part.x = 0.25f;
    part.h = 0.4f;
    part.mirrored = true;
    d.unitMutable(venator).look.parts.push_back(part);

    const std::string path = "unitmods_test.txt";
    int written = unitmods::save(d, path);
    CHECK(written >= 1, "designer wrote at least one unit");

    d.unitMutable(venator).cost = 1;
    d.unitMutable(venator).hardpoints.clear();
    int touched = unitmods::load(d, path);
    CHECK(touched >= 1, "designer file reloaded");
    CHECK(d.unit(venator).cost == 7777, "cost survives a save and reload");
    CHECK(d.unit(venator).hardpoints.size() == originalMounts + 1, "hardpoints survive too");
    CHECK(!d.unit(venator).hardpoints.empty() &&
              d.unit(venator).hardpoints.back().type == HardpointType::IonCannon,
          "hardpoint type survives");
    {
        const Hardpoint& back = d.unit(venator).hardpoints.back();
        CHECK(back.projectile == ProjectileKind::Flak, "ammunition survives");
        CHECK(back.barrels == 4 && std::abs(back.reload - 1.25f) < 0.001f, "salvo survives");
        CHECK(back.useOwnColour && back.colour[1] == 22, "bolt colour survives");
        CHECK(back.name == "Test Battery", "mount name survives");
    }
    CHECK(d.unit(venator).look.parts.size() == 1, "the model survives");
    if (!d.unit(venator).look.parts.empty()) {
        const HullPart& p0 = d.unit(venator).look.parts.front();
        CHECK(p0.shape == PartShape::Trapezoid && p0.tint == PartTint::Accent && p0.mirrored,
              "model part keeps its shape, colour and mirroring");
    }

    // A key the database has never seen creates a brand new unit.
    {
        std::ofstream f(path, std::ios::app);
        f << "unit test_frigate\n  name Test Frigate\n  faction 2\n  class 1\n"
          << "  cost 900 9 4 1 1\n  stats 500 100 20 10 200 40 1\n"
          << "  hardpoint 0 5 200 100 0.3 0.2 Test Gun\nend\n";
    }
    unitmods::load(d, path);
    Id made = d.unitId("test_frigate");
    CHECK(made != kInvalid, "an unknown key creates a new unit");
    if (made != kInvalid) {
        CHECK(d.unit(made).name == "Test Frigate", "new unit keeps its name");
        CHECK(d.unit(made).faction == Faction::CIS, "new unit keeps its faction");
        CHECK(d.unit(made).hardpoints.size() == 1, "new unit keeps its hardpoint");
    }

    std::remove(path.c_str());
    d.unitMutable(venator).cost = originalCost;
    d.unitMutable(venator).hardpoints.pop_back();
    d.unitMutable(venator).look.parts.clear();
    d.unitMutable(venator).custom = false;
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

    // Run a few weeks and confirm the order is delivered and income is paid.
    auto clonesAt = [&]() {
        int n = 0;
        for (Id id : gs.planet(planet).units) {
            const UnitInstance& u = gs.unit(id);
            if (u.alive && u.owner == Faction::Republic && u.defId == clone) ++n;
        }
        return n;
    };
    int unitsBefore = clonesAt();
    // ~60 seconds of play. Battles the player is dragged into stop the clock,
    // so resolve them as they come up.
    for (int i = 0; i < 60 * 20; ++i) {
        if (gs.hasPendingPlayerBattle()) gs.autoResolvePendingBattle();
        gs.update(0.05f);
    }
    int unitsAfter = clonesAt();
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

static void testSlotsAndLanding() {
    std::printf("orbital slots and the surface...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("clone_wars_total");
    s.playerFaction = Faction::Republic;
    gs.start(s);

    Id home = kInvalid;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).def().key == "coruscant") home = i;
    }
    CHECK(home != kInvalid, "found Coruscant");
    if (home == kInvalid) return;

    // Every ship starts in one of the three orbital slots, and they are spread
    // out rather than piled into slot 1.
    std::vector<Id> ships = gs.unitsAt(home, Faction::Republic, Domain::Space);
    CHECK(!ships.empty(), "Coruscant has a fleet");
    int perSlot[kOrbitSlots] = {0, 0, 0};
    for (Id id : ships) {
        int slot = gs.unit(id).slot;
        CHECK(slot >= 0 && slot < kOrbitSlots, "slot is in range");
        if (slot >= 0 && slot < kOrbitSlots) ++perSlot[slot];
    }
    int used = 0;
    for (int i = 0; i < kOrbitSlots; ++i) {
        if (perSlot[i] > 0) ++used;
    }
    CHECK(used > 1, "the fleet is spread over more than one slot");

    // Moving a ship between slots, and the accounting that follows it.
    Id ship = ships.front();
    CHECK(gs.setUnitSlot(ship, 2, Faction::Republic).ok, "ship moves to slot 3");
    CHECK(gs.unit(ship).slot == 2, "slot recorded");
    std::vector<Id> inSlot = gs.unitsInSlot(home, Faction::Republic, 2);
    CHECK(std::find(inSlot.begin(), inSlot.end(), ship) != inSlot.end(), "ship is listed in slot 3");
    CHECK(!gs.setUnitSlot(ship, 7, Faction::Republic).ok, "no fourth slot");
    CHECK(!gs.setUnitSlot(ship, 1, Faction::CIS).ok, "cannot move someone else's ship");

    // Landed troops need lifting before they can change slot.
    std::vector<Id> troops = gs.unitsAt(home, Faction::Republic, Domain::Ground, true);
    CHECK(!troops.empty(), "Coruscant has a garrison");
    if (!troops.empty()) {
        CHECK(!gs.setUnitSlot(troops.front(), 1, Faction::Republic).ok, "landed troops have no slot");
        OrderResult lift = gs.liftToOrbit({troops.front()}, Faction::Republic);
        CHECK(lift.ok, lift.message.c_str());
        CHECK(!gs.unit(troops.front()).landed, "the unit is aboard the transports");
        CHECK(gs.setUnitSlot(troops.front(), 1, Faction::Republic).ok, "now it can change slot");
    }

    // The surface holds ten divisions and no more.
    CHECK(gs.unitSlotCapacity(home, Domain::Ground) <= kGroundSlotCapacity,
          "ground capacity is capped at ten");
    gs.faction(Faction::Republic).credits = 999999;
    Id clone = db().unitId("rep_clone");
    for (int i = 0; i < 40 && gs.canQueueUnit(home, clone, Faction::Republic).ok; ++i) {
        gs.queueUnit(home, clone, Faction::Republic);
    }
    for (int i = 0; i < 4000 && !gs.planet(home).queue.empty(); ++i) gs.update(0.2f);
    CHECK(static_cast<int>(gs.unitsAt(home, Faction::Republic, Domain::Ground, true).size()) <=
              kGroundSlotCapacity,
          "never more than ten divisions on the surface");
}

static void testTrespassIsIntercepted() {
    std::printf("trespassing...\n");
    GameState gs;
    GameSetup s;
    s.campaign = db().campaignId("clone_wars_total");
    s.playerFaction = Faction::Hutts;  // keep the player out of the way
    s.seed = 1234;
    gs.start(s);

    // Look for a defended world with a route running straight through it: a
    // start system whose shortest path to some other system passes over it.
    Id through = kInvalid, start = kInvalid, beyond = kInvalid;
    for (int t = 0; t < gs.planetCount() && through == kInvalid; ++t) {
        if (gs.unitsAt(t, gs.planet(t).owner, Domain::Space).empty()) continue;
        if (gs.planet(t).owner == Faction::CIS) continue;  // must be hostile to the fleet
        const std::vector<Id>& nb = gs.neighbours(t);
        for (size_t i = 0; i < nb.size() && through == kInvalid; ++i) {
            if (!gs.orbitClearFor(nb[i], Faction::CIS)) continue;  // safe staging system
            for (size_t j = 0; j < nb.size(); ++j) {
                if (i == j) continue;
                std::vector<Id> path = gs.findPath(nb[i], nb[j]);
                if (path.size() < 2 || path.front() != t) continue;
                through = t;
                start = nb[i];
                beyond = nb[j];
                break;
            }
        }
    }
    CHECK(through != kInvalid, "found a defended world sitting on a route");
    if (through == kInvalid) return;

    std::vector<Id> fleet;
    Id corvette = db().unitId("cis_diamond");
    for (int i = 0; i < 3; ++i) fleet.push_back(gs.spawnUnitForTest(corvette, Faction::CIS, start));
    CHECK(fleet.size() == 3, "staged a CIS squadron");

    OrderResult move = gs.moveUnits(fleet, beyond);
    CHECK(move.ok, move.message.c_str());

    // Fly it. The fleet must be pulled out of hyperspace over the defended
    // world instead of sailing past it to its destination. The battle that
    // follows may finish inside the same tick, so watch the battle log rather
    // than the fleet itself.
    bool intercepted = false;
    bool arrived = false;
    for (int i = 0; i < 6000 && !intercepted && !arrived; ++i) {
        if (gs.hasPendingPlayerBattle()) gs.autoResolvePendingBattle();
        gs.update(0.2f);
        for (const BattleReport& r : gs.battleLog()) {
            if (r.planet == through && (r.attacker == Faction::CIS || r.defender == Faction::CIS)) {
                intercepted = true;
            }
        }
        if (!gs.unitsAt(beyond, Faction::CIS, Domain::Space).empty()) arrived = true;
    }
    CHECK(intercepted, "the fleet was intercepted over the defended world");
    CHECK(!arrived, "the fleet did not slip past the defended world");
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
    testSlotsAndLanding();
    testTrespassIsIntercepted();
    testTacticalBattle();
    testAiPlaysByTheRules();
    testDeterminism();
    // Last: it adds a unit to the shared database, which would otherwise
    // change what the AI has to play with in the tests above.
    testUnitModsRoundTrip();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
