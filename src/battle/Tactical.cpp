#include "battle/Tactical.h"

#include <algorithm>
#include <cmath>

#include "sim/GameState.h"

namespace gc {
namespace tactical {

const UnitDef* TUnit::def() const {
    if (structure) return nullptr;
    return defId == kInvalid ? nullptr : &db().unit(defId);
}

namespace {

float radiusFor(UnitClass c) {
    switch (c) {
        case UnitClass::Capital: return 34.0f;
        case UnitClass::Cruiser: return 26.0f;
        case UnitClass::Frigate: return 20.0f;
        case UnitClass::Corvette: return 14.0f;
        case UnitClass::Fighter:
        case UnitClass::Bomber: return 8.0f;
        case UnitClass::Vehicle: return 16.0f;
        case UnitClass::Artillery: return 18.0f;
        case UnitClass::AirSupport: return 12.0f;
        default: return 11.0f;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void Battle::init(const GameState& gs, const BattleSetup& setup, uint64_t seed) {
    setup_ = setup;
    units_.clear();
    shots_.clear();
    rng_.seed(seed ? seed : 7777);
    elapsed_ = 0.0f;
    finished_ = false;
    victor_ = Faction::Neutral;
    attackerRetreating_ = false;
    defenderRetreating_ = false;
    fieldSize_ = setup.domain == Domain::Space ? Vec2(1400.0f, 900.0f) : Vec2(1200.0f, 820.0f);

    spawnSide(gs, setup.attackerUnits, true, setup.attacker);
    spawnSide(gs, setup.defenderUnits, false, setup.defender);
    spawnStructures(gs, setup.defenderStructures, setup.defender);

    // Carriers deploy their squadrons immediately.
    const size_t initial = units_.size();
    for (size_t i = 0; i < initial; ++i) {
        if (setup.domain == Domain::Space && !units_[i].structure) launchWings(gs, static_cast<int>(i));
    }
    for (size_t i = 0; i < units_.size(); ++i) units_[i].index = static_cast<Id>(i);
}

Vec2 Battle::spawnPoint(bool attackerSide, int slot, int total) const {
    float x = attackerSide ? fieldSize_.x * 0.22f : fieldSize_.x * 0.78f;
    int perColumn = std::max(1, (total + 2) / 3);
    int column = slot / perColumn;
    int row = slot % perColumn;
    float spacing = fieldSize_.y / static_cast<float>(perColumn + 1);
    float y = spacing * static_cast<float>(row + 1);
    x += (attackerSide ? -1.0f : 1.0f) * static_cast<float>(column) * 60.0f;
    return Vec2(x, y);
}

void Battle::spawnSide(const GameState& gs, const std::vector<Id>& unitIds, bool attackerSide,
                       Faction owner) {
    int total = static_cast<int>(unitIds.size());
    int slot = 0;
    for (Id id : unitIds) {
        const UnitInstance& inst = gs.unit(id);
        if (!inst.alive) continue;
        const UnitDef& d = inst.def();
        TUnit u;
        u.instanceId = id;
        u.defId = d.id;
        u.owner = owner;
        u.attackerSide = attackerSide;
        u.maxHull = d.hull;
        u.hull = d.hull * inst.health;
        u.maxShield = d.totalShield();
        u.shield = u.maxShield;
        u.radius = radiusFor(d.unitClass);
        u.squadron = d.isSquadron();
        u.pos = spawnPoint(attackerSide, slot, std::max(1, total));
        u.pos.y += rng_.range(-18.0f, 18.0f);
        units_.push_back(u);
        ++slot;
    }
}

void Battle::spawnStructures(const GameState& gs, const std::vector<Id>& structureIds, Faction owner) {
    int slot = 0;
    int total = static_cast<int>(structureIds.size());
    for (Id id : structureIds) {
        const BuildingInstance& b = gs.buildingInst(id);
        if (!b.alive) continue;
        const BuildingDef& bd = b.def();
        TUnit u;
        u.structureId = id;
        u.defId = bd.id;
        u.owner = owner;
        u.attackerSide = false;
        u.structure = true;
        u.maxHull = std::max(1.0f, bd.defenceHp);
        u.hull = u.maxHull * b.hp;
        u.maxShield = bd.shieldStrength;
        u.shield = bd.shieldStrength;
        u.radius = 24.0f;
        float spacing = fieldSize_.y / static_cast<float>(total + 1);
        u.pos = Vec2(fieldSize_.x * 0.93f, spacing * static_cast<float>(slot + 1));
        units_.push_back(u);
        ++slot;
    }
}

void Battle::launchWings(const GameState& gs, int carrierIndex) {
    const TUnit carrier = units_[static_cast<size_t>(carrierIndex)];
    const UnitDef* cd = carrier.def();
    if (cd == nullptr) return;
    for (const CarriedWing& w : cd->wings) {
        if (w.unitId == kInvalid) continue;
        const UnitDef& wd = db().unit(w.unitId);
        for (int i = 0; i < w.count; ++i) {
            TUnit u;
            u.defId = wd.id;
            u.owner = carrier.owner;
            u.attackerSide = carrier.attackerSide;
            u.maxHull = wd.hull;
            u.hull = wd.hull;
            u.maxShield = wd.totalShield();
            u.shield = u.maxShield;
            u.radius = radiusFor(wd.unitClass);
            u.squadron = true;
            u.parent = carrierIndex;
            u.pos = carrier.pos + Vec2(rng_.range(-45.0f, 45.0f), rng_.range(-45.0f, 45.0f));
            units_.push_back(u);
        }
    }
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------
void Battle::acquireTargets() {
    for (TUnit& u : units_) {
        if (!u.alive || u.escaped) continue;
        if (u.forcedTarget != kInvalid) {
            const TUnit& t = units_[static_cast<size_t>(u.forcedTarget)];
            if (t.alive && !t.escaped) {
                u.target = u.forcedTarget;
                continue;
            }
            u.forcedTarget = kInvalid;
        }
        if (u.target != kInvalid) {
            const TUnit& t = units_[static_cast<size_t>(u.target)];
            if (t.alive && !t.escaped) continue;
        }
        // Nearest enemy, with a preference for what we are good at killing.
        float best = 1e18f;
        Id bestIdx = kInvalid;
        const UnitDef* d = u.def();
        bool antiFighter = d != nullptr && d->antiFighter() > d->antiCapital();
        for (const TUnit& other : units_) {
            if (!other.alive || other.escaped) continue;
            if (other.attackerSide == u.attackerSide && other.owner == u.owner) continue;
            if (other.owner == u.owner) continue;
            float dsq = distanceSq(u.pos, other.pos);
            float preference = 1.0f;
            if (antiFighter && !other.squadron) preference = 2.4f;
            if (!antiFighter && other.squadron) preference = 2.0f;
            float score = dsq * preference;
            if (score < best) {
                best = score;
                bestIdx = other.index;
            }
        }
        u.target = bestIdx;
    }
}

void Battle::applyDamage(TUnit& target, float damage) {
    if (damage <= 0.0f || !target.alive) return;
    target.shieldDelay = 4.0f;
    if (target.shield > 0.0f) {
        float absorbed = std::min(target.shield, damage);
        target.shield -= absorbed;
        damage -= absorbed;
    }
    target.hull -= damage;
    if (target.hull <= 0.0f) {
        target.hull = 0.0f;
        target.alive = false;
        // Squadrons go down with the ship.
        for (TUnit& other : units_) {
            if (other.parent >= 0 && other.parent == static_cast<int>(target.index) && other.alive) {
                other.alive = false;
                other.hull = 0.0f;
            }
        }
    }
}

void Battle::fire(TUnit& u, TUnit& target, float dt) {
    const UnitDef* d = u.def();
    float dmg = 0.0f;
    float reload = 1.0f;
    if (d != nullptr) {
        dmg = target.squadron ? d->antiFighter() : d->antiCapital();
        reload = target.squadron ? 0.8f : 1.2f;
    } else if (u.structure) {
        const BuildingDef& bd = db().building(u.defId);
        dmg = target.squadron ? bd.defenceDamage * 0.6f : bd.defenceDamage;
        reload = 1.2f;
    }
    if (dmg <= 0.0f) return;
    u.cooldown = reload;
    applyDamage(target, dmg * reload * rng_.range(0.85f, 1.15f));
    u.firedTimer = 0.15f;
    u.firedAt = target.pos;
    Shot s;
    s.from = u.pos;
    s.to = target.pos;
    s.life = 0.18f;
    s.heavy = !target.squadron && dmg > 30.0f;
    s.owner = u.owner;
    shots_.push_back(s);
}

void Battle::stepUnit(TUnit& u, float dt) {
    if (!u.alive || u.escaped) return;
    const UnitDef* d = u.def();

    if (u.shieldDelay > 0.0f) {
        u.shieldDelay -= dt;
    } else if (u.shield < u.maxShield && d != nullptr) {
        u.shield = std::min(u.maxShield, u.shield + d->shieldRegen * dt);
    }
    if (u.firedTimer > 0.0f) u.firedTimer -= dt;
    if (u.cooldown > 0.0f) u.cooldown -= dt;
    if (u.structure) return;

    float speed = d != nullptr ? d->totalSpeed() : 30.0f;
    float range = d != nullptr ? d->range : 150.0f;

    if (u.retreating) {
        // Run for the edge of the battlefield we came in through.
        float edge = u.attackerSide ? 0.0f : fieldSize_.x;
        Vec2 dir = Vec2(edge - u.pos.x, 0.0f).normalized();
        u.pos += dir * (speed * 1.4f * dt);
        if ((u.attackerSide && u.pos.x <= 20.0f) || (!u.attackerSide && u.pos.x >= fieldSize_.x - 20.0f)) {
            u.escaped = true;
        }
        return;
    }

    Vec2 destination = u.pos;
    bool hasDestination = false;
    if (u.hasMoveOrder) {
        destination = u.moveTarget;
        hasDestination = true;
        if (distance(u.pos, u.moveTarget) < 30.0f) u.hasMoveOrder = false;
    } else if (u.target != kInvalid) {
        TUnit& t = units_[static_cast<size_t>(u.target)];
        float dist = distance(u.pos, t.pos);
        float desired = range * 0.75f;
        if (dist > desired) {
            destination = t.pos;
            hasDestination = true;
        }
    }

    if (hasDestination) {
        Vec2 dir = (destination - u.pos).normalized();
        u.pos += dir * (speed * dt);
    }

    // Keep inside the battlefield.
    u.pos.x = std::max(10.0f, std::min(fieldSize_.x - 10.0f, u.pos.x));
    u.pos.y = std::max(10.0f, std::min(fieldSize_.y - 10.0f, u.pos.y));

    if (u.target != kInvalid && u.cooldown <= 0.0f) {
        TUnit& t = units_[static_cast<size_t>(u.target)];
        if (t.alive && !t.escaped && distance(u.pos, t.pos) <= range) fire(u, t, dt);
    }
}

void Battle::update(float dt) {
    if (finished_) return;
    dt = std::min(dt, 0.1f);
    elapsed_ += dt;

    acquireTargets();
    for (TUnit& u : units_) stepUnit(u, dt);

    for (Shot& s : shots_) s.life -= dt;
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.life <= 0.0f; }),
                 shots_.end());

    checkEnd();
}

void Battle::checkEnd() {
    int attackers = 0, defenders = 0;
    for (const TUnit& u : units_) {
        if (!u.alive || u.escaped) continue;
        if (u.attackerSide) {
            ++attackers;
        } else {
            ++defenders;
        }
    }
    if (attackers > 0 && defenders > 0) {
        // A battle where nobody can win (e.g. only fighters left against a
        // shielded station) is called after ten minutes.
        if (elapsed_ < 600.0f) return;
        finished_ = true;
        victor_ = sideHealth(true) > sideHealth(false) ? setup_.attacker : setup_.defender;
        return;
    }
    finished_ = true;
    if (attackers == 0 && defenders == 0) {
        victor_ = setup_.defender;
    } else {
        victor_ = attackers > 0 ? setup_.attacker : setup_.defender;
    }
}

float Battle::sideHealth(bool attackerSide) const {
    float total = 0.0f;
    for (const TUnit& u : units_) {
        if (!u.alive || u.escaped || u.attackerSide != attackerSide) continue;
        total += u.hull + u.shield;
    }
    return total;
}

int Battle::sideCount(bool attackerSide) const {
    int n = 0;
    for (const TUnit& u : units_) {
        if (u.alive && !u.escaped && u.attackerSide == attackerSide && u.instanceId != kInvalid) ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------
void Battle::orderMove(const std::vector<Id>& unitIndices, Vec2 target) {
    for (Id i : unitIndices) {
        if (i < 0 || i >= static_cast<Id>(units_.size())) continue;
        TUnit& u = units_[static_cast<size_t>(i)];
        if (!u.alive || u.structure || u.retreating) continue;
        u.moveTarget = target;
        u.hasMoveOrder = true;
        u.forcedTarget = kInvalid;
    }
}

void Battle::orderAttack(const std::vector<Id>& unitIndices, Id targetIndex) {
    if (targetIndex < 0 || targetIndex >= static_cast<Id>(units_.size())) return;
    for (Id i : unitIndices) {
        if (i < 0 || i >= static_cast<Id>(units_.size())) continue;
        TUnit& u = units_[static_cast<size_t>(i)];
        if (!u.alive || u.structure || u.retreating) continue;
        u.forcedTarget = targetIndex;
        u.target = targetIndex;
        u.hasMoveOrder = false;
    }
}

void Battle::beginRetreat(Faction f) {
    bool attackerSide = (f == setup_.attacker);
    if (attackerSide) {
        attackerRetreating_ = true;
    } else {
        defenderRetreating_ = true;
    }
    for (TUnit& u : units_) {
        if (!u.alive || u.escaped || u.structure) continue;
        if (u.owner != f) continue;
        u.retreating = true;
        u.hasMoveOrder = false;
    }
}

bool Battle::isRetreating(Faction f) const {
    return f == setup_.attacker ? attackerRetreating_ : defenderRetreating_;
}

// ---------------------------------------------------------------------------
// Consequences
// ---------------------------------------------------------------------------
BattleResolution Battle::resolution(const GameState& gs) const {
    BattleResolution res;
    res.victor = victor_ == Faction::Neutral ? setup_.defender : victor_;

    int structuresDestroyed = 0;
    for (const TUnit& u : units_) {
        if (u.structureId != kInvalid) {
            if (!u.alive || res.victor != setup_.defender) {
                res.destroyedStructures.push_back(u.structureId);
                ++structuresDestroyed;
            }
            continue;
        }
        if (u.instanceId == kInvalid) continue;  // launched squadron

        UnitOutcome o;
        o.instanceId = u.instanceId;
        if (u.escaped) {
            o.retreated = true;
            o.health = std::max(0.1f, u.healthFraction());
        } else if (!u.alive) {
            o.destroyed = true;
        } else {
            bool lost = (u.owner != res.victor);
            if (lost) {
                // Still on the field when the battle ended and on the losing
                // side: overrun.
                o.destroyed = true;
            } else {
                o.health = std::max(0.1f, u.healthFraction());
            }
        }
        res.outcomes.push_back(o);
    }

    res.report = buildReport(gs, setup_, res.outcomes, res.victor, structuresDestroyed, false);
    return res;
}

}  // namespace tactical
}  // namespace gc
