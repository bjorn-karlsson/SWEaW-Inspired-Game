#include "sim/AI.h"

#include <algorithm>
#include <cmath>

#include "sim/GameState.h"

namespace gc {

namespace {

/// How badly the AI wants each building, highest first. Everything else is
/// judged on its own merits below.
int buildingPriority(const GameState& gs, Id planet, const BuildingDef& b, bool frontier) {
    int score = 0;
    if (b.productionTier > 0) {
        // Production first: without factories nothing else matters.
        score += 100 - b.productionTier * 5;
        if (b.domain == Domain::Space && gs.planet(planet).def().hasTrait(Trait::Shipyards)) score += 25;
    }
    score += b.incomeFlat / 4;
    score += static_cast<int>(b.incomeMult * 100.0f);
    if (b.defenceHp > 0.0f || b.shieldStrength > 0.0f) score += frontier ? 45 : 5;
    score += b.unitSlotBonus * 8;
    score += static_cast<int>(b.researchSpeed * 40.0f);
    return score;
}

}  // namespace

AiController::AiController(Faction faction, Difficulty difficulty)
    : faction_(faction), difficulty_(difficulty) {
    aggression_ = difficultyAiAggression(difficulty);
}

int AiController::reserve(const GameState& gs) const {
    // Keep roughly one week of income in the bank.
    return std::max(500, gs.faction(faction_).lastIncome / 2);
}

bool AiController::isFrontier(const GameState& gs, Id planet) const {
    for (Id nb : gs.neighbours(planet)) {
        if (gs.planet(nb).owner != faction_) return true;
    }
    return false;
}

std::vector<Id> AiController::frontierPlanets(const GameState& gs) const {
    std::vector<Id> out;
    for (int i = 0; i < gs.planetCount(); ++i) {
        if (gs.planet(i).owner == faction_ && isFrontier(gs, i)) out.push_back(i);
    }
    return out;
}

void AiController::onDay(GameState& gs) {
    if (gs.faction(faction_).defeated) return;

    economyPhase(gs);
    researchPhase(gs);
    militaryPhase(gs);
    defencePhase(gs);
    invasionPhase(gs);
    offensePhase(gs);
}

// ---------------------------------------------------------------------------
// Economy: build production and income structures.
// ---------------------------------------------------------------------------
void AiController::economyPhase(GameState& gs) {
    for (int i = 0; i < gs.planetCount(); ++i) {
        PlanetState& p = gs.planet(i);
        if (p.owner != faction_) continue;
        if (p.queue.size() >= 2) continue;
        if (gs.isContested(i)) continue;

        bool frontier = isFrontier(gs, i);
        Id best = kInvalid;
        int bestScore = -1;
        for (Id bid : gs.buildableBuildings(i, faction_)) {
            const BuildingDef& bd = db().building(bid);
            if (!gs.canQueueBuilding(i, bid, faction_).ok) continue;
            if (gs.faction(faction_).credits - gs.buildingCost(i, bid) < reserve(gs)) continue;
            int score = buildingPriority(gs, i, bd, frontier);
            if (score > bestScore) {
                bestScore = score;
                best = bid;
            }
        }
        // Only spend on structures that are actually worth it.
        if (best != kInvalid && bestScore >= 20) gs.queueBuilding(i, best, faction_);
    }
}

// ---------------------------------------------------------------------------
// Research
// ---------------------------------------------------------------------------
void AiController::researchPhase(GameState& gs) {
    FactionState& fs = gs.faction(faction_);
    if (!fs.research.empty()) return;
    std::vector<Id> options = gs.researchableTechs(faction_);
    if (options.empty()) return;

    Id best = kInvalid;
    int bestCost = 0;
    for (Id t : options) {
        const TechDef& td = db().tech(t);
        int cost = gs.techCost(faction_, t);
        // Prefer cheap, low tier technologies first: they unlock the rest.
        int score = 1000 - cost / 10 - td.tier * 40;
        if (best == kInvalid || score > bestCost) {
            bestCost = score;
            best = t;
        }
    }
    if (best == kInvalid) return;
    int cost = gs.techCost(faction_, best);
    if (fs.credits - cost < reserve(gs)) return;
    gs.startResearch(faction_, best);
}

// ---------------------------------------------------------------------------
// Military production
// ---------------------------------------------------------------------------
void AiController::militaryPhase(GameState& gs) {
    FactionState& fs = gs.faction(faction_);

    for (int i = 0; i < gs.planetCount(); ++i) {
        PlanetState& p = gs.planet(i);
        if (p.owner != faction_) continue;
        if (p.queue.size() >= 3) continue;
        if (gs.isContested(i)) continue;

        for (int pass = 0; pass < 2; ++pass) {
            Domain domain = pass == 0 ? Domain::Space : Domain::Ground;
            if (gs.bestProductionTier(i, faction_, domain) <= 0) continue;
            if (gs.usedUnitSlots(i, faction_, domain) >= gs.unitSlotCapacity(i, domain)) continue;

            // Pick the most capable unit we can afford, with a bias towards
            // filling out cheap escorts and infantry early on.
            Id best = kInvalid;
            float bestValue = 0.0f;
            for (Id uid : gs.buildableUnits(i, faction_)) {
                const UnitDef& ud = db().unit(uid);
                if (ud.domain() != domain) continue;
                if (ud.isHero) continue;  // heroes handled separately
                if (!gs.canQueueUnit(i, uid, faction_).ok) continue;
                int cost = gs.unitCost(i, uid);
                if (fs.credits - cost < reserve(gs)) continue;
                float power = ud.hull + ud.shield + (ud.damageAntiCapital + ud.damageAntiFighter) * 18.0f;
                for (const CarriedWing& w : ud.wings) {
                    if (w.unitId == kInvalid) continue;
                    const UnitDef& wd = db().unit(w.unitId);
                    power += static_cast<float>(w.count) *
                             (wd.hull + (wd.damageAntiCapital + wd.damageAntiFighter) * 14.0f);
                }
                float value = power / static_cast<float>(std::max(1, cost));
                // Slight preference for bigger hulls so fleets do not become
                // an endless swarm of corvettes. Population runs from 1 to 21,
                // so it is damped to keep the bonus a nudge, not a mandate.
                value *= 1.0f + 0.02f * static_cast<float>(ud.popCost);
                if (value > bestValue) {
                    bestValue = value;
                    best = uid;
                }
            }
            if (best != kInvalid) gs.queueUnit(i, best, faction_);
        }

        // Recruit a hero when we can comfortably afford one.
        if (fs.credits > reserve(gs) * 4) {
            for (Id uid : gs.buildableUnits(i, faction_)) {
                const UnitDef& ud = db().unit(uid);
                if (!ud.isHero) continue;
                if (!gs.canQueueUnit(i, uid, faction_).ok) continue;
                if (fs.credits - gs.unitCost(i, uid) < reserve(gs) * 2) continue;
                gs.queueUnit(i, uid, faction_);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Defence: keep the frontier garrisoned, pull reserves forward.
// ---------------------------------------------------------------------------
void AiController::defencePhase(GameState& gs) {
    std::vector<Id> frontier = frontierPlanets(gs);
    if (frontier.empty()) return;

    for (int i = 0; i < gs.planetCount(); ++i) {
        const PlanetState& p = gs.planet(i);
        if (p.owner != faction_) continue;
        if (isFrontier(gs, i)) continue;
        if (i == stagingPlanet_) continue;
        if (!gs.orbitClearFor(i, faction_)) continue;

        // Interior world: send everything but a token garrison to the nearest
        // threatened frontier world.
        std::vector<Id> space = gs.unitsAt(i, faction_, Domain::Space);
        std::vector<Id> ground = gs.unitsAt(i, faction_, Domain::Ground);
        if (space.size() + ground.size() <= 2) continue;

        Id destination = kInvalid;
        float worst = 1e9f;
        for (Id f : frontier) {
            float mine = gs.forceStrengthAt(f, faction_, Domain::Space) +
                         gs.forceStrengthAt(f, faction_, Domain::Ground);
            std::vector<Id> path = gs.findPath(i, f);
            if (path.empty()) continue;
            float score = mine + static_cast<float>(path.size()) * 40.0f;
            if (score < worst) {
                worst = score;
                destination = f;
            }
        }
        if (destination == kInvalid) continue;

        std::vector<Id> moving;
        for (size_t k = 1; k < space.size(); ++k) moving.push_back(space[k]);
        for (size_t k = 1; k < ground.size(); ++k) moving.push_back(ground[k]);
        if (!moving.empty()) gs.moveUnits(moving, destination);
    }
}

// ---------------------------------------------------------------------------
// Invasion: if we hold orbit over an enemy world and have troops, land them.
// ---------------------------------------------------------------------------
void AiController::invasionPhase(GameState& gs) {
    for (int i = 0; i < gs.planetCount(); ++i) {
        const PlanetState& p = gs.planet(i);
        if (p.owner == faction_) continue;
        if (!gs.orbitClearFor(i, faction_)) continue;
        if (p.def().spaceOnly) continue;

        std::vector<Id> inOrbit = gs.unitsAt(i, faction_, Domain::Ground, false, true);
        if (inOrbit.empty()) continue;

        float mine = autoresolve::forceStrength(gs, inOrbit, Domain::Ground);
        float theirs = gs.forceStrengthAt(i, p.owner, Domain::Ground);
        if (mine < theirs * (1.35f - aggression_ * 0.35f) && theirs > 0.0f) continue;
        gs.invade(i, inOrbit, faction_);
    }
}

// ---------------------------------------------------------------------------
// Offence: pick a target, mass forces, attack.
// ---------------------------------------------------------------------------
float AiController::targetValue(const GameState& gs, Id planet) const {
    const PlanetState& p = gs.planet(planet);
    const PlanetDef& pd = p.def();
    float value = static_cast<float>(pd.baseIncome);
    if (p.owner == Faction::Neutral) value *= 1.35f;  // soft targets first
    for (Trait t : pd.traits) {
        if (t == Trait::Shipyards || t == Trait::MiningWorld || t == Trait::CoreWorld) value += 80.0f;
    }
    float defence = gs.forceStrengthAt(planet, p.owner, Domain::Space) +
                    gs.forceStrengthAt(planet, p.owner, Domain::Ground) * 0.5f;
    return value / (60.0f + defence);
}

void AiController::offensePhase(GameState& gs) {
    // Re-plan when the current plan is finished or invalid.
    if (targetPlanet_ != kInvalid && gs.planet(targetPlanet_).owner == faction_) {
        targetPlanet_ = kInvalid;
        stagingPlanet_ = kInvalid;
        daysStaging_ = 0;
    }
    if (stagingPlanet_ != kInvalid && gs.planet(stagingPlanet_).owner != faction_) {
        stagingPlanet_ = kInvalid;
        targetPlanet_ = kInvalid;
    }

    if (targetPlanet_ == kInvalid) {
        float best = 0.0f;
        for (int i = 0; i < gs.planetCount(); ++i) {
            if (gs.planet(i).owner != faction_) continue;
            for (Id nb : gs.neighbours(i)) {
                if (gs.planet(nb).owner == faction_) continue;
                float v = targetValue(gs, nb);
                if (v > best) {
                    best = v;
                    targetPlanet_ = nb;
                    stagingPlanet_ = i;
                }
            }
        }
        daysStaging_ = 0;
        if (targetPlanet_ == kInvalid) return;
    }
    if (stagingPlanet_ == kInvalid || targetPlanet_ == kInvalid) return;
    ++daysStaging_;

    // Gather nearby forces at the staging world.
    if (daysStaging_ % 3 == 0) {
        for (int i = 0; i < gs.planetCount(); ++i) {
            if (i == stagingPlanet_) continue;
            const PlanetState& p = gs.planet(i);
            if (p.owner != faction_) continue;
            if (isFrontier(gs, i) && gs.neighbours(i).size() > 0) {
                // Frontier worlds keep a garrison, but spare capital ships move up.
                std::vector<Id> space = gs.unitsAt(i, faction_, Domain::Space);
                if (space.size() <= 2) continue;
                std::vector<Id> moving(space.begin() + 2, space.end());
                gs.moveUnits(moving, stagingPlanet_);
            }
        }
    }

    std::vector<Id> space = gs.unitsAt(stagingPlanet_, faction_, Domain::Space);
    std::vector<Id> ground = gs.unitsAt(stagingPlanet_, faction_, Domain::Ground);
    if (space.empty()) return;

    float mySpace = autoresolve::forceStrength(gs, space, Domain::Space);
    const PlanetState& tp = gs.planet(targetPlanet_);
    float theirSpace = gs.forceStrengthAt(targetPlanet_, tp.owner, Domain::Space);
    float myGround = autoresolve::forceStrength(gs, ground, Domain::Ground);
    float theirGround = gs.forceStrengthAt(targetPlanet_, tp.owner, Domain::Ground);

    float required = 1.6f - aggression_ * 0.5f;
    bool spaceReady = mySpace > std::max(60.0f, theirSpace * required);
    bool groundReady = tp.def().spaceOnly || myGround > std::max(30.0f, theirGround * required);
    bool patienceRunOut = daysStaging_ > 60;

    if ((spaceReady && groundReady) || (spaceReady && patienceRunOut)) {
        std::vector<Id> strike = space;
        // Leave a small home guard behind.
        if (strike.size() > 3) strike.erase(strike.begin(), strike.begin() + 1);
        for (Id g : ground) strike.push_back(g);
        if (gs.moveUnits(strike, targetPlanet_).ok) {
            lastOffensiveDay_ = gs.date().day;
            daysStaging_ = 0;
            targetPlanet_ = kInvalid;  // pick a fresh objective next time
        }
    } else if (daysStaging_ > 90) {
        // This objective is not working out; look elsewhere.
        targetPlanet_ = kInvalid;
        stagingPlanet_ = kInvalid;
        daysStaging_ = 0;
    }
}

}  // namespace gc
