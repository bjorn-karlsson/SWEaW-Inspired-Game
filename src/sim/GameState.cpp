#include "sim/GameState.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <queue>
#include <set>

#include "sim/AI.h"

namespace gc {

float speedMultiplier(GameSpeed s) {
    switch (s) {
        case GameSpeed::Paused: return 0.0f;
        case GameSpeed::Normal: return 1.0f;
        case GameSpeed::Fast: return 2.5f;
        case GameSpeed::Fastest: return 5.0f;
        default: return 1.0f;
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void GameState::start(const GameSetup& setup) {
    setup_ = setup;
    rng_.seed(setup.seed);
    date_ = GalacticDate{};
    dayAccumulator_ = 0.0f;
    speed_ = GameSpeed::Normal;
    outcome_ = GameOutcome::InProgress;

    planets_.clear();
    lanes_.clear();
    adjacency_.clear();
    units_.clear();
    buildings_.clear();
    fleets_.clear();
    factions_.clear();
    pendingBattles_.clear();
    battleLog_.clear();
    events_.clear();
    ais_.clear();

    factions_.resize(kFactionCount);
    for (int i = 0; i < kFactionCount; ++i) {
        factions_[static_cast<size_t>(i)].id = factionFromIndex(i);
        factions_[static_cast<size_t>(i)].techKnown.assign(db().techs().size(), 0);
        factions_[static_cast<size_t>(i)].credits = 0;
    }
    faction(setup.playerFaction).isPlayer = true;

    const CampaignDef& c = db().campaign(setup.campaign);
    buildGalaxy(c);
    for (const FactionStart& s : c.starts) applyFactionStart(s);
    seedNeutralGarrisons();

    for (Faction f : playableFactions()) {
        if (f == setup.playerFaction) continue;
        ais_.push_back(std::make_shared<AiController>(f, setup.difficulty));
    }

    log("Campaign begins: " + c.name);
    log("You command the " + std::string(factionName(setup.playerFaction)) + ".",
        setup.playerFaction);
}

void GameState::buildGalaxy(const CampaignDef& c) {
    planetDefToIndex_.assign(db().planets().size(), kInvalid);

    std::vector<Id> included;
    if (c.planetKeys.empty()) {
        for (const PlanetDef& p : db().planets()) included.push_back(p.id);
    } else {
        for (const std::string& key : c.planetKeys) {
            Id id = db().planetId(key);
            if (id != kInvalid) included.push_back(id);
        }
    }

    for (Id defId : included) {
        PlanetState ps;
        ps.defId = defId;
        ps.owner = Faction::Neutral;
        planetDefToIndex_[static_cast<size_t>(defId)] = static_cast<Id>(planets_.size());
        planets_.push_back(ps);
    }

    adjacency_.assign(planets_.size(), {});
    for (const LaneDef& l : db().lanes()) {
        Id a = planetDefToIndex_[static_cast<size_t>(l.a)];
        Id b = planetDefToIndex_[static_cast<size_t>(l.b)];
        if (a == kInvalid || b == kInvalid) continue;
        LaneDef lane;
        lane.a = a;
        lane.b = b;
        lane.hyperlane = l.hyperlane;
        lanes_.push_back(lane);
        adjacency_[static_cast<size_t>(a)].push_back(b);
        adjacency_[static_cast<size_t>(b)].push_back(a);
    }
}

void GameState::applyFactionStart(const FactionStart& s) {
    FactionState& fs = faction(s.faction);
    fs.credits = s.credits;

    for (const std::string& key : s.planets) {
        Id defId = db().planetId(key);
        if (defId == kInvalid) continue;
        Id idx = planetDefToIndex_[static_cast<size_t>(defId)];
        if (idx == kInvalid) continue;
        planets_[static_cast<size_t>(idx)].owner = s.faction;
    }

    for (const StartingBuilding& b : s.buildings) {
        Id defId = db().planetId(b.planetKey);
        if (defId == kInvalid) continue;
        Id idx = planetDefToIndex_[static_cast<size_t>(defId)];
        Id bdef = db().buildingId(b.buildingKey);
        if (idx == kInvalid || bdef == kInvalid) continue;
        if (planets_[static_cast<size_t>(idx)].owner != s.faction) continue;
        const BuildingDef& bd = db().building(bdef);
        if (usedBuildSlots(idx, bd.domain) >= buildSlotCapacity(idx, bd.domain)) continue;
        spawnBuilding(bdef, s.faction, idx);
    }

    for (const StartingForce& f : s.forces) {
        Id defId = db().planetId(f.planetKey);
        if (defId == kInvalid) continue;
        Id idx = planetDefToIndex_[static_cast<size_t>(defId)];
        Id udef = db().unitId(f.unitKey);
        if (idx == kInvalid || udef == kInvalid) continue;
        const UnitDef& ud = db().unit(udef);
        for (int i = 0; i < f.count; ++i) {
            spawnUnit(udef, s.faction, idx, ud.domain() == Domain::Ground);
        }
    }
}

void GameState::seedNeutralGarrisons() {
    Id militia = db().unitId("neu_militia");
    Id militiaTank = db().unitId("neu_militia_tank");
    Id corvette = db().unitId("neu_corvette");
    Id fighter = db().unitId("neu_fighter");
    Id frigate = db().unitId("neu_frigate");

    for (size_t i = 0; i < planets_.size(); ++i) {
        PlanetState& p = planets_[i];
        if (p.owner != Faction::Neutral) continue;
        const PlanetDef& pd = p.def();
        Id idx = static_cast<Id>(i);

        int spaceStrength = 1 + pd.spaceUnitSlots / 3;
        for (int n = 0; n < spaceStrength; ++n) {
            Id defId = (n == 0 && pd.spaceUnitSlots >= 6) ? frigate : (n % 2 == 0 ? corvette : fighter);
            spawnUnit(defId, Faction::Neutral, idx, false);
        }
        if (!pd.spaceOnly) {
            int groundStrength = 1 + pd.groundUnitSlots / 3;
            for (int n = 0; n < groundStrength; ++n) {
                Id defId = (n == 1) ? militiaTank : militia;
                spawnUnit(defId, Faction::Neutral, idx, true);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Object lifecycle
// ---------------------------------------------------------------------------
Id GameState::spawnUnit(Id defId, Faction owner, Id planet, bool landed) {
    UnitInstance u;
    u.id = static_cast<Id>(units_.size());
    u.defId = defId;
    u.owner = owner;
    u.health = 1.0f;
    u.landed = landed && isGroundClass(db().unit(defId).unitClass);
    units_.push_back(u);
    addUnitToPlanet(u.id, planet);
    return u.id;
}

Id GameState::spawnBuilding(Id defId, Faction owner, Id planet) {
    BuildingInstance b;
    b.id = static_cast<Id>(buildings_.size());
    b.defId = defId;
    b.owner = owner;
    b.planet = planet;
    buildings_.push_back(b);
    planets_[static_cast<size_t>(planet)].buildings.push_back(b.id);
    return b.id;
}

void GameState::addUnitToPlanet(Id unitId, Id planet) {
    UnitInstance& u = unit(unitId);
    u.planet = planet;
    u.fleet = kInvalid;
    planets_[static_cast<size_t>(planet)].units.push_back(unitId);
}

void GameState::removeUnitFromPlanet(Id unitId) {
    UnitInstance& u = unit(unitId);
    if (u.planet == kInvalid) return;
    std::vector<Id>& v = planets_[static_cast<size_t>(u.planet)].units;
    v.erase(std::remove(v.begin(), v.end(), unitId), v.end());
    u.planet = kInvalid;
}

void GameState::destroyUnit(Id unitId) {
    UnitInstance& u = unit(unitId);
    if (!u.alive) return;
    removeUnitFromPlanet(unitId);
    if (u.fleet != kInvalid) {
        Fleet& f = fleets_[static_cast<size_t>(u.fleet)];
        f.units.erase(std::remove(f.units.begin(), f.units.end(), unitId), f.units.end());
        u.fleet = kInvalid;
    }
    u.alive = false;
    const UnitDef& d = u.def();
    if (d.isHero && d.respawnDays > 0) {
        faction(u.owner).heroRespawnTimer[u.defId] = d.respawnDays;
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
const std::vector<Id>& GameState::neighbours(Id planet) const {
    static const std::vector<Id> kEmpty;
    if (planet < 0 || planet >= static_cast<Id>(adjacency_.size())) return kEmpty;
    return adjacency_[static_cast<size_t>(planet)];
}

std::vector<Id> GameState::unitsAt(Id planet, Faction owner, Domain domain, bool landedOnly,
                                   bool orbitOnly) const {
    std::vector<Id> out;
    if (planet == kInvalid) return out;
    for (Id id : planets_[static_cast<size_t>(planet)].units) {
        const UnitInstance& u = unit(id);
        if (!u.alive || u.owner != owner) continue;
        const UnitDef& d = u.def();
        if (d.domain() != domain) continue;
        if (domain == Domain::Ground) {
            if (landedOnly && !u.landed) continue;
            if (orbitOnly && u.landed) continue;
        }
        out.push_back(id);
    }
    return out;
}

std::vector<Id> GameState::allUnitsAt(Id planet, Faction owner) const {
    std::vector<Id> out;
    if (planet == kInvalid) return out;
    for (Id id : planets_[static_cast<size_t>(planet)].units) {
        const UnitInstance& u = unit(id);
        if (u.alive && u.owner == owner) out.push_back(id);
    }
    return out;
}

int GameState::usedUnitSlots(Id planet, Faction owner, Domain domain) const {
    int used = 0;
    for (Id id : planets_[static_cast<size_t>(planet)].units) {
        const UnitInstance& u = unit(id);
        if (!u.alive || u.owner != owner) continue;
        const UnitDef& d = u.def();
        if (d.domain() != domain) continue;
        used += d.popCost;
    }
    // Queued units reserve their slots too.
    for (const BuildOrder& o : planets_[static_cast<size_t>(planet)].queue) {
        if (o.kind != BuildKind::Unit) continue;
        const UnitDef& d = db().unit(o.defId);
        if (d.domain() == domain) used += d.popCost;
    }
    return used;
}

int GameState::unitSlotCapacity(Id planet, Domain domain) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    const PlanetDef& pd = p.def();
    int cap = domain == Domain::Space ? pd.spaceUnitSlots : pd.groundUnitSlots;
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (!b.alive || b.owner != p.owner) continue;
        const BuildingDef& bd = b.def();
        if (bd.domain == domain) cap += bd.unitSlotBonus;
    }
    return cap;
}

int GameState::usedBuildSlots(Id planet, Domain domain) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    int used = 0;
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (b.alive && b.def().domain == domain) ++used;
    }
    for (const BuildOrder& o : p.queue) {
        if (o.kind == BuildKind::Building && db().building(o.defId).domain == domain) ++used;
    }
    return used;
}

int GameState::buildSlotCapacity(Id planet, Domain domain) const {
    const PlanetDef& pd = planets_[static_cast<size_t>(planet)].def();
    return domain == Domain::Space ? pd.spaceBuildSlots : pd.groundBuildSlots;
}

int GameState::bestProductionTier(Id planet, Faction owner, Domain domain) const {
    int best = 0;
    for (Id bid : planets_[static_cast<size_t>(planet)].buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (!b.alive || b.owner != owner) continue;
        const BuildingDef& bd = b.def();
        if (bd.domain == domain) best = std::max(best, bd.productionTier);
    }
    return best;
}

bool GameState::hasProductionTier(Id planet, Faction owner, Domain domain, int tier) const {
    return bestProductionTier(planet, owner, domain) >= tier;
}

bool GameState::techKnown(Faction f, Id techId) const {
    if (techId == kInvalid) return true;
    const FactionState& fs = faction(f);
    if (techId < 0 || techId >= static_cast<Id>(fs.techKnown.size())) return false;
    return fs.techKnown[static_cast<size_t>(techId)] != 0;
}

bool GameState::canResearch(Faction f, Id techId) const {
    const TechDef& t = db().tech(techId);
    if (t.faction != f) return false;
    if (techKnown(f, techId)) return false;
    for (const ResearchOrder& r : faction(f).research) {
        if (r.techId == techId) return false;
    }
    for (Id pre : t.prerequisites) {
        if (!techKnown(f, pre)) return false;
    }
    return true;
}

std::vector<Faction> GameState::factionsPresent(Id planet) const {
    std::set<int> present;
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    for (Id id : p.units) {
        const UnitInstance& u = unit(id);
        if (u.alive) present.insert(fidx(u.owner));
    }
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (b.alive && b.def().defenceHp > 0.0f) present.insert(fidx(b.owner));
    }
    std::vector<Faction> out;
    for (int i : present) out.push_back(factionFromIndex(i));
    return out;
}

bool GameState::isContested(Id planet) const { return factionsPresent(planet).size() > 1; }

bool GameState::orbitClearFor(Id planet, Faction f) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    for (Id id : p.units) {
        const UnitInstance& u = unit(id);
        if (!u.alive || u.owner == f) continue;
        if (u.isSpace()) return false;
    }
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (!b.alive || b.owner == f) continue;
        const BuildingDef& bd = b.def();
        if (bd.domain == Domain::Space && bd.defenceHp > 0.0f) return false;
    }
    return true;
}

bool GameState::spaceControlledBy(Id planet, Faction f) const {
    bool has = !unitsAt(planet, f, Domain::Space).empty();
    return has && orbitClearFor(planet, f);
}

// ---------------------------------------------------------------------------
// Costs
// ---------------------------------------------------------------------------
int GameState::unitCost(Id planet, Id unitDefId) const {
    const UnitDef& u = db().unit(unitDefId);
    TraitMods m = planets_[static_cast<size_t>(planet)].def().mods();
    float cost = static_cast<float>(u.cost);
    switch (u.unitClass) {
        case UnitClass::Capital:
        case UnitClass::Cruiser: cost *= m.capitalCostMult; break;
        case UnitClass::Infantry: cost *= m.infantryCostMult; break;
        case UnitClass::Vehicle:
        case UnitClass::Artillery: cost *= m.groundVehicleCostMult; break;
        default: break;
    }
    return static_cast<int>(cost + 0.5f);
}

float GameState::unitBuildDays(Id planet, Id unitDefId) const {
    const UnitDef& u = db().unit(unitDefId);
    TraitMods m = planets_[static_cast<size_t>(planet)].def().mods();
    float days = static_cast<float>(u.buildDays);
    switch (u.unitClass) {
        case UnitClass::Capital:
        case UnitClass::Cruiser: days *= m.capitalTimeMult; break;
        case UnitClass::Infantry: days *= m.infantryTimeMult; break;
        case UnitClass::Vehicle:
        case UnitClass::Artillery: days *= m.groundVehicleTimeMult; break;
        default: break;
    }
    return std::max(1.0f, days);
}

int GameState::buildingCost(Id planet, Id buildingDefId) const {
    const BuildingDef& b = db().building(buildingDefId);
    TraitMods m = planets_[static_cast<size_t>(planet)].def().mods();
    float cost = static_cast<float>(b.cost);
    if (b.defenceHp > 0.0f || b.shieldStrength > 0.0f) cost *= m.structureCostMult;
    return static_cast<int>(cost + 0.5f);
}

float GameState::buildingBuildDays(Id planet, Id buildingDefId) const {
    return static_cast<float>(db().building(buildingDefId).buildDays);
}

int GameState::techCost(Faction f, Id techId) const {
    const TechDef& t = db().tech(techId);
    float best = 1.0f;
    for (size_t i = 0; i < planets_.size(); ++i) {
        if (planets_[i].owner != f) continue;
        best = std::min(best, planets_[i].def().mods().researchCostMult);
    }
    return static_cast<int>(static_cast<float>(t.cost) * best + 0.5f);
}

float GameState::techDays(Faction f, Id techId) const {
    const TechDef& t = db().tech(techId);
    float speedup = 0.0f;
    for (const BuildingInstance& b : buildings_) {
        if (!b.alive || b.owner != f) continue;
        speedup = std::max(speedup, b.def().researchSpeed);
    }
    return std::max(1.0f, static_cast<float>(t.researchDays) * (1.0f - speedup));
}

// ---------------------------------------------------------------------------
// Build availability
// ---------------------------------------------------------------------------
OrderResult GameState::canQueueUnit(Id planet, Id unitDefId, Faction f) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    const UnitDef& u = db().unit(unitDefId);
    if (p.owner != f) return OrderResult::fail("You do not control this world");
    if (u.faction != f) return OrderResult::fail("Not available to your faction");
    if (u.domain() == Domain::Ground && p.def().spaceOnly)
        return OrderResult::fail("No surface on this world");
    if (!techKnown(f, u.requiredTech))
        return OrderResult::fail("Requires " + db().tech(u.requiredTech).name);
    if (!hasProductionTier(planet, f, u.domain(), u.requiredTier)) {
        return OrderResult::fail(std::string(u.domain() == Domain::Space ? "Requires orbital station "
                                                                        : "Requires production facility ") +
                                 "tier " + std::to_string(u.requiredTier));
    }
    if (usedUnitSlots(planet, f, u.domain()) + u.popCost > unitSlotCapacity(planet, u.domain()))
        return OrderResult::fail("No free unit slots");
    if (u.isHero) {
        for (const UnitInstance& inst : units_) {
            if (inst.alive && inst.defId == unitDefId) return OrderResult::fail("Already in the field");
        }
        auto it = faction(f).heroRespawnTimer.find(unitDefId);
        if (it != faction(f).heroRespawnTimer.end() && it->second > 0)
            return OrderResult::fail("Unavailable for " + std::to_string(it->second) + " days");
        for (const BuildOrder& o : p.queue) {
            if (o.kind == BuildKind::Unit && o.defId == unitDefId)
                return OrderResult::fail("Already being recruited");
        }
    }
    if (faction(f).credits < unitCost(planet, unitDefId)) return OrderResult::fail("Not enough credits");
    return OrderResult::success();
}

OrderResult GameState::canQueueBuilding(Id planet, Id buildingDefId, Faction f) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    const BuildingDef& b = db().building(buildingDefId);
    if (p.owner != f) return OrderResult::fail("You do not control this world");
    if (b.faction != Faction::Neutral && b.faction != f)
        return OrderResult::fail("Not available to your faction");
    if (b.domain == Domain::Ground && p.def().spaceOnly)
        return OrderResult::fail("No surface on this world");
    if (!techKnown(f, b.requiredTech))
        return OrderResult::fail("Requires " + db().tech(b.requiredTech).name);
    if (b.requiredTrait != Trait::Count && !p.def().hasTrait(b.requiredTrait))
        return OrderResult::fail(std::string("Requires ") + traitName(b.requiredTrait));
    if (usedBuildSlots(planet, b.domain) >= buildSlotCapacity(planet, b.domain))
        return OrderResult::fail("No free building slots");
    if (b.uniquePerPlanet) {
        for (Id bid : p.buildings) {
            const BuildingInstance& inst = buildingInst(bid);
            if (inst.alive && inst.defId == buildingDefId) return OrderResult::fail("Already built here");
        }
        for (const BuildOrder& o : p.queue) {
            if (o.kind == BuildKind::Building && o.defId == buildingDefId)
                return OrderResult::fail("Already in the queue");
        }
    }
    if (faction(f).credits < buildingCost(planet, buildingDefId))
        return OrderResult::fail("Not enough credits");
    return OrderResult::success();
}

std::vector<Id> GameState::buildableUnits(Id planet, Faction f) const {
    std::vector<Id> out;
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (p.owner != f) return out;
    int spaceTier = bestProductionTier(planet, f, Domain::Space);
    int groundTier = bestProductionTier(planet, f, Domain::Ground);
    for (const UnitDef& u : db().units()) {
        if (u.faction != f) continue;
        if (!techKnown(f, u.requiredTech)) continue;
        int tier = u.domain() == Domain::Space ? spaceTier : groundTier;
        if (tier < u.requiredTier) continue;
        if (u.domain() == Domain::Ground && p.def().spaceOnly) continue;
        out.push_back(u.id);
    }
    return out;
}

std::vector<Id> GameState::buildableBuildings(Id planet, Faction f) const {
    std::vector<Id> out;
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (p.owner != f) return out;
    for (const BuildingDef& b : db().buildings()) {
        if (b.faction != Faction::Neutral && b.faction != f) continue;
        if (!techKnown(f, b.requiredTech)) continue;
        if (b.requiredTrait != Trait::Count && !p.def().hasTrait(b.requiredTrait)) continue;
        if (b.domain == Domain::Ground && p.def().spaceOnly) continue;
        out.push_back(b.id);
    }
    return out;
}

std::vector<Id> GameState::researchableTechs(Faction f) const {
    std::vector<Id> out;
    for (const TechDef& t : db().techs()) {
        if (canResearch(f, t.id)) out.push_back(t.id);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Economy
// ---------------------------------------------------------------------------
int GameState::planetIncome(Id planet) const {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (p.owner == Faction::Neutral) return 0;
    const PlanetDef& pd = p.def();
    TraitMods m = pd.mods();
    float base = static_cast<float>(pd.baseIncome);
    float mult = m.incomeMult;
    int flat = m.incomeFlat;
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (!b.alive || b.owner != p.owner) continue;
        const BuildingDef& bd = b.def();
        flat += bd.incomeFlat;
        mult += bd.incomeMult;
    }
    // A world under active siege produces nothing.
    if (isContested(planet)) return 0;
    return static_cast<int>(base * mult + 0.5f) + flat;
}

int GameState::factionIncome(Faction f) const {
    int total = 0;
    for (size_t i = 0; i < planets_.size(); ++i) {
        if (planets_[i].owner == f) total += planetIncome(static_cast<Id>(i));
    }
    for (const UnitInstance& u : units_) {
        if (u.alive && u.owner == f) total += u.def().heroIncomeBonus;
    }
    if (f != playerFaction()) {
        total = static_cast<int>(static_cast<float>(total) * difficultyAiIncomeMult(setup_.difficulty));
    }
    return total;
}

int GameState::planetsOwned(Faction f) const {
    int n = 0;
    for (const PlanetState& p : planets_) {
        if (p.owner == f) ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------
bool GameState::laneIsHyperlane(Id a, Id b) const {
    for (const LaneDef& l : lanes_) {
        if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) return l.hyperlane;
    }
    return false;
}

float GameState::laneTravelDays(Id a, Id b) const {
    const PlanetDef& pa = planets_[static_cast<size_t>(a)].def();
    const PlanetDef& pb = planets_[static_cast<size_t>(b)].def();
    float dist = distance(pa.pos, pb.pos);
    bool hyper = laneIsHyperlane(a, b);
    float speed = hyper ? 60.0f : 24.0f;  // map units per day
    return std::max(0.6f, dist / speed);
}

std::vector<Id> GameState::findPath(Id from, Id to) const {
    std::vector<Id> path;
    if (from == to || from == kInvalid || to == kInvalid) return path;
    const size_t n = planets_.size();
    std::vector<float> dist(n, 1e9f);
    std::vector<Id> prev(n, kInvalid);
    using Node = std::pair<float, Id>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    dist[static_cast<size_t>(from)] = 0.0f;
    pq.push({0.0f, from});
    while (!pq.empty()) {
        auto [d, cur] = pq.top();
        pq.pop();
        if (cur == to) break;
        if (d > dist[static_cast<size_t>(cur)] + 1e-4f) continue;
        for (Id nb : neighbours(cur)) {
            float nd = d + laneTravelDays(cur, nb);
            if (nd < dist[static_cast<size_t>(nb)]) {
                dist[static_cast<size_t>(nb)] = nd;
                prev[static_cast<size_t>(nb)] = cur;
                pq.push({nd, nb});
            }
        }
    }
    if (dist[static_cast<size_t>(to)] >= 1e9f) return path;
    for (Id cur = to; cur != kInvalid && cur != from; cur = prev[static_cast<size_t>(cur)]) {
        path.push_back(cur);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

Id GameState::nearestFriendlyPlanet(Id from, Faction f) const {
    std::vector<int> dist(planets_.size(), -1);
    std::queue<Id> q;
    dist[static_cast<size_t>(from)] = 0;
    q.push(from);
    while (!q.empty()) {
        Id cur = q.front();
        q.pop();
        if (cur != from && planets_[static_cast<size_t>(cur)].owner == f && orbitClearFor(cur, f)) {
            return cur;
        }
        for (Id nb : neighbours(cur)) {
            if (dist[static_cast<size_t>(nb)] < 0) {
                dist[static_cast<size_t>(nb)] = dist[static_cast<size_t>(cur)] + 1;
                q.push(nb);
            }
        }
    }
    return kInvalid;
}

// ---------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------
OrderResult GameState::queueUnit(Id planet, Id unitDefId, Faction f) {
    OrderResult r = canQueueUnit(planet, unitDefId, f);
    if (!r.ok) return r;
    BuildOrder o;
    o.kind = BuildKind::Unit;
    o.defId = unitDefId;
    o.totalDays = unitBuildDays(planet, unitDefId);
    o.daysRemaining = o.totalDays;
    o.cost = unitCost(planet, unitDefId);
    faction(f).credits -= o.cost;
    planets_[static_cast<size_t>(planet)].queue.push_back(o);
    return OrderResult::success(db().unit(unitDefId).name + " queued");
}

OrderResult GameState::queueBuilding(Id planet, Id buildingDefId, Faction f) {
    OrderResult r = canQueueBuilding(planet, buildingDefId, f);
    if (!r.ok) return r;
    BuildOrder o;
    o.kind = BuildKind::Building;
    o.defId = buildingDefId;
    o.totalDays = buildingBuildDays(planet, buildingDefId);
    o.daysRemaining = o.totalDays;
    o.cost = buildingCost(planet, buildingDefId);
    faction(f).credits -= o.cost;
    planets_[static_cast<size_t>(planet)].queue.push_back(o);
    return OrderResult::success(db().building(buildingDefId).name + " queued");
}

OrderResult GameState::cancelBuildOrder(Id planet, int index, Faction f) {
    PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (p.owner != f) return OrderResult::fail("Not your world");
    if (index < 0 || index >= static_cast<int>(p.queue.size())) return OrderResult::fail("No such order");
    const BuildOrder& o = p.queue[static_cast<size_t>(index)];
    // Refund the unspent portion.
    float frac = o.totalDays > 0.0f ? o.daysRemaining / o.totalDays : 1.0f;
    faction(f).credits += static_cast<int>(static_cast<float>(o.cost) * frac);
    p.queue.erase(p.queue.begin() + index);
    return OrderResult::success("Order cancelled");
}

OrderResult GameState::startResearch(Faction f, Id techId) {
    if (!canResearch(f, techId)) return OrderResult::fail("Cannot research this yet");
    int cost = techCost(f, techId);
    if (faction(f).credits < cost) return OrderResult::fail("Not enough credits");
    faction(f).credits -= cost;
    ResearchOrder r;
    r.techId = techId;
    r.totalDays = techDays(f, techId);
    r.daysRemaining = r.totalDays;
    faction(f).research.push_back(r);
    return OrderResult::success("Researching " + db().tech(techId).name);
}

OrderResult GameState::moveUnits(const std::vector<Id>& unitIds, Id destination) {
    if (unitIds.empty()) return OrderResult::fail("No units selected");
    Faction owner = unit(unitIds.front()).owner;
    Id origin = unit(unitIds.front()).planet;
    if (origin == kInvalid) return OrderResult::fail("Units are already in transit");
    for (Id id : unitIds) {
        const UnitInstance& u = unit(id);
        if (!u.alive) return OrderResult::fail("Unit destroyed");
        if (u.owner != owner) return OrderResult::fail("Mixed ownership");
        if (u.planet != origin) return OrderResult::fail("Units are not in the same system");
    }
    if (destination == origin) return OrderResult::fail("Already there");
    if (!orbitClearFor(origin, owner))
        return OrderResult::fail("Enemy forces hold orbit - win the space battle first");

    // A world you already hold can only be stacked so deep: production is
    // capped at the planet's slots, and a garrison may not exceed twice that.
    if (planets_[static_cast<size_t>(destination)].owner == owner) {
        int incoming[2] = {0, 0};
        for (Id id : unitIds) {
            const UnitDef& d = unit(id).def();
            incoming[d.domain() == Domain::Space ? 0 : 1] += d.popCost;
        }
        for (int i = 0; i < 2; ++i) {
            Domain dom = i == 0 ? Domain::Space : Domain::Ground;
            if (incoming[i] == 0) continue;
            int limit = unitSlotCapacity(destination, dom) * 2;
            if (usedUnitSlots(destination, owner, dom) + incoming[i] > limit) {
                return OrderResult::fail(std::string(domainName(dom)) +
                                         " garrison at the destination is full");
            }
        }
    }

    std::vector<Id> path = findPath(origin, destination);
    if (path.empty()) return OrderResult::fail("No hyperspace route");

    Fleet fl;
    fl.id = static_cast<Id>(fleets_.size());
    fl.owner = owner;
    fl.from = origin;
    fl.to = path.front();
    fl.finalDestination = destination;
    fl.path.assign(path.begin() + 1, path.end());
    fl.legDays = laneTravelDays(origin, fl.to);
    fl.legProgress = 0.0f;
    for (Id id : unitIds) {
        removeUnitFromPlanet(id);
        unit(id).fleet = fl.id;
        unit(id).landed = false;
        fl.units.push_back(id);
    }
    fleets_.push_back(fl);
    return OrderResult::success("Fleet en route to " + planets_[static_cast<size_t>(destination)].def().name);
}

OrderResult GameState::invade(Id planet, const std::vector<Id>& unitIds, Faction f) {
    const PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (p.def().spaceOnly) return OrderResult::fail("This system has no surface");
    if (!orbitClearFor(planet, f))
        return OrderResult::fail("Orbital defences must be destroyed before landing");

    std::vector<Id> landing;
    for (Id id : unitIds) {
        const UnitInstance& u = unit(id);
        if (!u.alive || u.owner != f || u.planet != planet) continue;
        if (u.def().domain() != Domain::Ground || u.landed) continue;
        landing.push_back(id);
    }
    if (landing.empty()) return OrderResult::fail("No ground forces in orbit");

    for (Id id : landing) unit(id).landed = true;

    if (p.owner == f) return OrderResult::success("Ground forces deployed");

    // Anything to fight?
    Faction defender = p.owner;
    std::vector<Id> defenders = unitsAt(planet, defender, Domain::Ground, true);
    std::vector<Id> structures;
    for (Id bid : p.buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (b.alive && b.owner == defender && b.def().domain == Domain::Ground && b.def().defenceHp > 0.0f)
            structures.push_back(bid);
    }
    if (defenders.empty() && structures.empty()) {
        capturePlanet(planet, f);
        return OrderResult::success("World captured without resistance");
    }

    PendingBattle pb;
    pb.setup.domain = Domain::Ground;
    pb.setup.planet = planet;
    pb.setup.attacker = f;
    pb.setup.defender = defender;
    pb.setup.attackerUnits = landing;
    pb.setup.defenderUnits = defenders;
    pb.setup.defenderStructures = structures;
    pb.setup.defenderBonus = p.def().mods().defenceBonus;
    pb.setup.playerInvolved = (f == playerFaction() || defender == playerFaction());
    pb.needsPlayerDecision = pb.setup.playerInvolved;
    pendingBattles_.push_back(pb);
    if (!pb.needsPlayerDecision) resolveAutomaticBattles();
    return OrderResult::success("Landing under fire");
}

OrderResult GameState::retreatFrom(Id planet, Faction f) {
    std::vector<Id> mine = allUnitsAt(planet, f);
    if (mine.empty()) return OrderResult::fail("No forces here");
    Id target = nearestFriendlyPlanet(planet, f);
    if (target == kInvalid) return OrderResult::fail("Nowhere to retreat to");
    return moveUnits(mine, target);
}

OrderResult GameState::withdraw(Id planet, Faction f) {
    std::vector<Id> mine = allUnitsAt(planet, f);
    if (mine.empty()) return OrderResult::fail("No forces here");
    Id target = nearestFriendlyPlanet(planet, f);
    retreatUnits(mine, planet);
    if (target == kInvalid) {
        log("Forces at " + planets_[static_cast<size_t>(planet)].def().name +
                " had nowhere to run and were lost",
            f);
        return OrderResult::success("No line of retreat - forces lost");
    }
    log(std::string(factionShortName(f)) + " withdraws from " +
            planets_[static_cast<size_t>(planet)].def().name,
        f);
    return OrderResult::success("Withdrawing to " + planets_[static_cast<size_t>(target)].def().name);
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
void GameState::togglePause() {
    speed_ = (speed_ == GameSpeed::Paused) ? GameSpeed::Normal : GameSpeed::Paused;
}

void GameState::update(float dtSeconds) {
    if (outcome_ != GameOutcome::InProgress) return;
    if (paused()) return;
    if (hasPendingPlayerBattle()) return;  // the galaxy waits for your decision

    float days = dtSeconds * speedMultiplier(speed_) / kSecondsPerDay;
    if (days <= 0.0f) return;

    tickFleets(days);
    if (hasPendingPlayerBattle()) return;

    dayAccumulator_ += days;
    int guard = 0;
    while (dayAccumulator_ >= 1.0f && guard++ < 40) {
        dayAccumulator_ -= 1.0f;
        advanceDay();
        if (outcome_ != GameOutcome::InProgress || hasPendingPlayerBattle()) break;
    }
}

void GameState::advanceDay() {
    date_.day += 1;
    for (PlanetState& p : planets_) {
        if (p.daysSinceCombat < 9999) ++p.daysSinceCombat;
    }

    tickProduction();
    tickResearch();
    tickHeroRespawns();

    for (auto& ai : ais_) ai->onDay(*this);

    if (date_.dayOfWeek() == 0) payWeeklyIncome();

    detectBattles();
    resolveAutomaticBattles();
    handleStrandedTransports();
    refreshFactionAlive();
    checkVictory();
}

void GameState::tickProduction() {
    for (size_t i = 0; i < planets_.size(); ++i) {
        PlanetState& p = planets_[i];
        if (p.queue.empty()) continue;
        if (isContested(static_cast<Id>(i))) continue;  // no production while besieged

        BuildOrder& o = p.queue.front();
        o.daysRemaining -= 1.0f;
        if (o.daysRemaining > 0.0f) continue;

        if (o.kind == BuildKind::Unit) {
            const UnitDef& ud = db().unit(o.defId);
            spawnUnit(o.defId, p.owner, static_cast<Id>(i), ud.domain() == Domain::Ground);
            if (p.owner == playerFaction()) {
                log(ud.name + " ready at " + p.def().name, p.owner);
            }
        } else {
            spawnBuilding(o.defId, p.owner, static_cast<Id>(i));
            if (p.owner == playerFaction()) {
                log(db().building(o.defId).name + " completed at " + p.def().name, p.owner);
            }
        }
        p.queue.erase(p.queue.begin());
    }
}

void GameState::tickResearch() {
    for (FactionState& fs : factions_) {
        if (fs.research.empty()) continue;
        ResearchOrder& r = fs.research.front();
        r.daysRemaining -= 1.0f;
        if (r.daysRemaining > 0.0f) continue;
        fs.techKnown[static_cast<size_t>(r.techId)] = 1;
        const TechDef& t = db().tech(r.techId);
        if (fs.id == playerFaction()) log("Research complete: " + t.name + " (" + t.unlocksText + ")", fs.id);
        fs.research.erase(fs.research.begin());
    }
}

void GameState::tickHeroRespawns() {
    for (FactionState& fs : factions_) {
        for (auto& kv : fs.heroRespawnTimer) {
            if (kv.second > 0) --kv.second;
        }
    }
}

void GameState::payWeeklyIncome() {
    for (FactionState& fs : factions_) {
        if (fs.id == Faction::Neutral) continue;
        int income = factionIncome(fs.id);
        fs.lastIncome = income;
        fs.credits += income;
    }
    if (date_.week() > 0) {
        log("Week " + std::to_string(date_.week() + 1) + " begins. Treasury: " +
                std::to_string(faction(playerFaction()).credits) + " credits (+" +
                std::to_string(faction(playerFaction()).lastIncome) + ")",
            playerFaction());
    }
}

void GameState::tickFleets(float days) {
    for (Fleet& f : fleets_) {
        if (!f.alive) continue;
        f.legProgress += days;
        while (f.alive && f.legProgress >= f.legDays) {
            float carry = f.legProgress - f.legDays;
            Id arrivedAt = f.to;
            if (f.path.empty()) {
                // Final destination reached.
                for (Id id : f.units) {
                    UnitInstance& u = unit(id);
                    if (!u.alive) continue;
                    u.fleet = kInvalid;
                    u.landed = false;
                    addUnitToPlanet(id, arrivedAt);
                }
                f.units.clear();
                f.alive = false;
                planets_[static_cast<size_t>(arrivedAt)].lastAggressor = f.owner;
            } else {
                f.from = arrivedAt;
                f.to = f.path.front();
                f.path.erase(f.path.begin());
                f.legDays = laneTravelDays(f.from, f.to);
                f.legProgress = carry;
                if (f.legProgress < f.legDays) break;
            }
        }
    }
    fleets_.erase(std::remove_if(fleets_.begin(), fleets_.end(),
                                 [](const Fleet& f) { return !f.alive && f.units.empty(); }),
                  fleets_.end());
    for (size_t i = 0; i < fleets_.size(); ++i) {
        fleets_[i].id = static_cast<Id>(i);
        for (Id u : fleets_[i].units) unit(u).fleet = static_cast<Id>(i);
    }

    detectBattles();
    resolveAutomaticBattles();
    for (size_t i = 0; i < planets_.size(); ++i) tryAutoCapture(static_cast<Id>(i));
}

// ---------------------------------------------------------------------------
// Battles
// ---------------------------------------------------------------------------
void GameState::detectBattles() {
    for (size_t i = 0; i < planets_.size(); ++i) {
        Id pid = static_cast<Id>(i);
        PlanetState& p = planets_[i];

        bool alreadyPending = false;
        for (const PendingBattle& pb : pendingBattles_) {
            if (pb.setup.planet == pid) alreadyPending = true;
        }
        if (alreadyPending) continue;

        // Which factions have space forces (or armed orbital structures) here?
        std::vector<Faction> spaceSides;
        for (int fi = 0; fi < kFactionCount; ++fi) {
            Faction f = factionFromIndex(fi);
            bool has = !unitsAt(pid, f, Domain::Space).empty();
            if (!has) {
                for (Id bid : p.buildings) {
                    const BuildingInstance& b = buildingInst(bid);
                    if (b.alive && b.owner == f && b.def().domain == Domain::Space &&
                        b.def().defenceHp > 0.0f) {
                        has = true;
                        break;
                    }
                }
            }
            if (has) spaceSides.push_back(f);
        }
        if (spaceSides.size() < 2) continue;

        // Attacker: the side that is not the owner (or the newest arrival).
        Faction defender = p.owner;
        if (std::find(spaceSides.begin(), spaceSides.end(), defender) == spaceSides.end()) {
            defender = spaceSides.front();
        }
        Faction attacker = Faction::Neutral;
        if (p.lastAggressor != Faction::Neutral && p.lastAggressor != defender &&
            std::find(spaceSides.begin(), spaceSides.end(), p.lastAggressor) != spaceSides.end()) {
            attacker = p.lastAggressor;
        } else {
            for (Faction f : spaceSides) {
                if (f != defender) {
                    attacker = f;
                    break;
                }
            }
        }
        if (attacker == Faction::Neutral && defender == Faction::Neutral) continue;
        if (attacker == defender) continue;

        PendingBattle pb;
        pb.setup.domain = Domain::Space;
        pb.setup.planet = pid;
        pb.setup.attacker = attacker;
        pb.setup.defender = defender;
        pb.setup.attackerUnits = unitsAt(pid, attacker, Domain::Space);
        pb.setup.defenderUnits = unitsAt(pid, defender, Domain::Space);
        for (Id bid : p.buildings) {
            const BuildingInstance& b = buildingInst(bid);
            if (b.alive && b.owner == defender && b.def().domain == Domain::Space &&
                b.def().defenceHp > 0.0f) {
                pb.setup.defenderStructures.push_back(bid);
            }
        }
        if (pb.setup.attackerUnits.empty()) continue;
        if (pb.setup.defenderUnits.empty() && pb.setup.defenderStructures.empty()) continue;
        pb.setup.defenderBonus = p.def().mods().defenceBonus * 0.5f;
        pb.setup.playerInvolved = (attacker == playerFaction() || defender == playerFaction());
        pb.needsPlayerDecision = pb.setup.playerInvolved;
        pendingBattles_.push_back(pb);
        p.daysSinceCombat = 0;
    }
}

void GameState::handleStrandedTransports() {
    for (size_t i = 0; i < planets_.size(); ++i) {
        Id pid = static_cast<Id>(i);
        for (int fi = 1; fi < kFactionCount; ++fi) {
            Faction f = factionFromIndex(fi);
            std::vector<Id> inOrbit = unitsAt(pid, f, Domain::Ground, false, true);
            if (inOrbit.empty()) continue;
            if (!unitsAt(pid, f, Domain::Space).empty()) continue;  // they have an escort
            if (orbitClearFor(pid, f)) continue;                    // the orbit is safe
            Id refuge = nearestFriendlyPlanet(pid, f);
            retreatUnits(inOrbit, pid);
            log(std::string(factionShortName(f)) + " transports over " + planets_[i].def().name +
                    (refuge == kInvalid ? " were destroyed with no escort" : " fled without escort"),
                f);
        }
    }
}

void GameState::resolveAutomaticBattles() {
    for (size_t i = 0; i < pendingBattles_.size();) {
        if (pendingBattles_[i].needsPlayerDecision) {
            ++i;
            continue;
        }
        BattleSetup setup = pendingBattles_[i].setup;
        pendingBattles_.erase(pendingBattles_.begin() + static_cast<long>(i));
        BattleResolution res = autoresolve::resolve(*this, setup, rng_);
        applyBattleResolution(setup, res);
    }
}

bool GameState::hasPendingPlayerBattle() const {
    for (const PendingBattle& pb : pendingBattles_) {
        if (pb.needsPlayerDecision) return true;
    }
    return false;
}

const PendingBattle* GameState::pendingPlayerBattle() const {
    for (const PendingBattle& pb : pendingBattles_) {
        if (pb.needsPlayerDecision) return &pb;
    }
    return nullptr;
}

BattleReport GameState::autoResolvePendingBattle() {
    BattleReport empty;
    for (size_t i = 0; i < pendingBattles_.size(); ++i) {
        if (!pendingBattles_[i].needsPlayerDecision) continue;
        BattleSetup setup = pendingBattles_[i].setup;
        pendingBattles_.erase(pendingBattles_.begin() + static_cast<long>(i));
        BattleResolution res = autoresolve::resolve(*this, setup, rng_);
        applyBattleResolution(setup, res);
        return res.report;
    }
    return empty;
}

bool GameState::takePendingBattle(BattleSetup& out) {
    for (size_t i = 0; i < pendingBattles_.size(); ++i) {
        if (!pendingBattles_[i].needsPlayerDecision) continue;
        out = pendingBattles_[i].setup;
        pendingBattles_.erase(pendingBattles_.begin() + static_cast<long>(i));
        return true;
    }
    return false;
}

void GameState::applyBattleResolution(const BattleSetup& setup, const BattleResolution& res) {
    std::vector<Id> retreating;
    for (const UnitOutcome& o : res.outcomes) {
        if (o.instanceId == kInvalid) continue;
        UnitInstance& u = unit(o.instanceId);
        if (!u.alive) continue;
        if (o.destroyed) {
            destroyUnit(o.instanceId);
        } else {
            u.health = std::max(0.05f, o.health);
            if (o.retreated) retreating.push_back(o.instanceId);
        }
    }
    for (Id bid : res.destroyedStructures) {
        BuildingInstance& b = buildingInst(bid);
        if (!b.alive) continue;
        b.alive = false;
        PlanetState& p = planets_[static_cast<size_t>(b.planet)];
        p.buildings.erase(std::remove(p.buildings.begin(), p.buildings.end(), bid), p.buildings.end());
    }

    Faction loser = (res.victor == setup.attacker) ? setup.defender : setup.attacker;
    Id pid = setup.planet;
    PlanetState& p = planets_[static_cast<size_t>(pid)];
    p.daysSinceCombat = 0;

    if (setup.domain == Domain::Space) {
        // Ground forces still in orbit share the fate of their fleet.
        std::vector<Id> cargo = unitsAt(pid, loser, Domain::Ground, false, true);
        for (Id id : cargo) {
            if (!retreating.empty()) {
                retreating.push_back(id);
            } else {
                destroyUnit(id);
            }
        }
    } else {
        // Losing ground troops fall back to orbit if their side holds it.
        bool holdsOrbit = spaceControlledBy(pid, loser);
        std::vector<Id> stillHere = unitsAt(pid, loser, Domain::Ground, true);
        for (Id id : stillHere) {
            if (holdsOrbit) {
                unit(id).landed = false;
            } else {
                destroyUnit(id);
            }
        }
        if (res.victor == setup.attacker) {
            bool hasBoots = !unitsAt(pid, setup.attacker, Domain::Ground, true).empty();
            if (hasBoots) capturePlanet(pid, setup.attacker);
        }
    }

    retreatUnits(retreating, pid);

    battleLog_.push_back(res.report);
    if (battleLog_.size() > 64) battleLog_.erase(battleLog_.begin());

    const std::string& pname = p.def().name;
    log(std::string(domainName(setup.domain)) + " battle over " + pname + ": " +
            factionShortName(res.victor) + " victorious (" +
            std::to_string(res.report.attackerSide.lost + res.report.defenderSide.lost) + " units lost)",
        res.victor);

    tryAutoCapture(pid);
}

void GameState::retreatUnits(const std::vector<Id>& unitIds, Id fromPlanet) {
    if (unitIds.empty()) return;
    Faction owner = unit(unitIds.front()).owner;
    Id target = nearestFriendlyPlanet(fromPlanet, owner);
    if (target == kInvalid) {
        for (Id id : unitIds) destroyUnit(id);
        return;
    }
    std::vector<Id> alive;
    for (Id id : unitIds) {
        if (unit(id).alive) alive.push_back(id);
    }
    if (alive.empty()) return;
    // Bypass the usual "orbit must be clear" check: this is a withdrawal.
    std::vector<Id> path = findPath(fromPlanet, target);
    if (path.empty()) {
        for (Id id : alive) destroyUnit(id);
        return;
    }
    Fleet fl;
    fl.id = static_cast<Id>(fleets_.size());
    fl.owner = owner;
    fl.from = fromPlanet;
    fl.to = path.front();
    fl.finalDestination = target;
    fl.path.assign(path.begin() + 1, path.end());
    fl.legDays = laneTravelDays(fromPlanet, fl.to);
    for (Id id : alive) {
        removeUnitFromPlanet(id);
        unit(id).fleet = fl.id;
        unit(id).landed = false;
        fl.units.push_back(id);
    }
    fleets_.push_back(fl);
}

void GameState::tryAutoCapture(Id planet) {
    PlanetState& p = planets_[static_cast<size_t>(planet)];
    if (!p.def().spaceOnly) return;
    for (int fi = 1; fi < kFactionCount; ++fi) {
        Faction f = factionFromIndex(fi);
        if (p.owner == f) continue;
        if (!unitsAt(planet, f, Domain::Space).empty() && orbitClearFor(planet, f)) {
            capturePlanet(planet, f);
            return;
        }
    }
}

void GameState::capturePlanet(Id planet, Faction newOwner) {
    PlanetState& p = planets_[static_cast<size_t>(planet)];
    Faction old = p.owner;
    if (old == newOwner) return;
    p.owner = newOwner;
    p.queue.clear();

    // Faction-specific installations are wrecked; neutral infrastructure is
    // captured intact.
    std::vector<Id> keep;
    for (Id bid : p.buildings) {
        BuildingInstance& b = buildingInst(bid);
        if (!b.alive) continue;
        const BuildingDef& bd = b.def();
        if (bd.faction == Faction::Neutral && bd.productionTier == 0) {
            b.owner = newOwner;
            b.hp = 1.0f;
            keep.push_back(bid);
        } else {
            b.alive = false;
        }
    }
    p.buildings = keep;

    log(std::string(factionShortName(newOwner)) + " captures " + p.def().name +
            (old == Faction::Neutral ? "" : " from " + std::string(factionShortName(old))),
        newOwner);
}

// ---------------------------------------------------------------------------
// Victory
// ---------------------------------------------------------------------------
void GameState::refreshFactionAlive() {
    for (FactionState& fs : factions_) {
        if (fs.id == Faction::Neutral || fs.defeated) continue;
        if (planetsOwned(fs.id) > 0) continue;
        bool anyUnits = false;
        for (const UnitInstance& u : units_) {
            if (u.alive && u.owner == fs.id) {
                anyUnits = true;
                break;
            }
        }
        if (!anyUnits) {
            fs.defeated = true;
            log(std::string(factionName(fs.id)) + " has been eliminated!", fs.id);
        }
    }
}

void GameState::checkVictory() {
    Faction me = playerFaction();
    int total = planetCount();
    if (planetsOwned(me) >= total) {
        outcome_ = GameOutcome::Victory;
        log("The galaxy is yours. Total victory!", me);
        speed_ = GameSpeed::Paused;
        return;
    }
    if (faction(me).defeated) {
        outcome_ = GameOutcome::Defeat;
        log("Your faction has been destroyed.", me);
        speed_ = GameSpeed::Paused;
    }
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------
void GameState::log(const std::string& text, Faction f) {
    GameEvent e;
    e.day = date_.day;
    e.text = text;
    e.faction = f;
    events_.push_back(e);
    while (events_.size() > 200) events_.pop_front();
}

float GameState::forceStrengthAt(Id planet, Faction f, Domain domain) const {
    std::vector<Id> ids = unitsAt(planet, f, domain, domain == Domain::Ground);
    float s = autoresolve::forceStrength(*this, ids, domain);
    for (Id bid : planets_[static_cast<size_t>(planet)].buildings) {
        const BuildingInstance& b = buildingInst(bid);
        if (!b.alive || b.owner != f) continue;
        const BuildingDef& bd = b.def();
        if (bd.domain != domain) continue;
        s += (bd.defenceHp * 0.02f + bd.defenceDamage * 1.5f) * b.hp;
    }
    return s;
}

}  // namespace gc
