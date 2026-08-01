#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "sim/GameState.h"

namespace gc {

// ---------------------------------------------------------------------------
// Report construction (shared with the tactical battler)
// ---------------------------------------------------------------------------
SummaryEntry& SideSummary::entryFor(Id defId) {
    for (SummaryEntry& e : entries) {
        if (e.defId == defId) return e;
    }
    SummaryEntry e;
    e.defId = defId;
    entries.push_back(e);
    return entries.back();
}

namespace {

void fillSide(const GameState& gs, SideSummary& side, Faction faction, const std::vector<Id>& unitIds,
              const std::unordered_map<Id, const UnitOutcome*>& byId) {
    side.faction = faction;
    for (Id id : unitIds) {
        const UnitInstance& u = gs.unit(id);
        SummaryEntry& e = side.entryFor(u.defId);
        ++e.committed;
        ++side.committed;
        auto it = byId.find(id);
        bool destroyed = (it != byId.end()) ? it->second->destroyed : !u.alive;
        bool retreated = (it != byId.end()) ? it->second->retreated : false;
        if (destroyed) {
            ++e.lost;
            ++side.lost;
            side.creditsLost += u.def().cost;
        } else {
            ++e.survived;
            ++side.survived;
            if (retreated) {
                ++e.retreated;
                ++side.retreated;
            }
        }
    }
    std::sort(side.entries.begin(), side.entries.end(),
              [](const SummaryEntry& a, const SummaryEntry& b) { return a.defId < b.defId; });
}

}  // namespace

BattleReport buildReport(const GameState& gs, const BattleSetup& setup,
                         const std::vector<UnitOutcome>& outcomes, Faction victor,
                         int structuresDestroyed, bool autoResolved) {
    std::unordered_map<Id, const UnitOutcome*> byId;
    for (const UnitOutcome& o : outcomes) byId[o.instanceId] = &o;

    BattleReport r;
    r.domain = setup.domain;
    r.planet = setup.planet;
    r.planetName = gs.planet(setup.planet).def().name;
    r.attacker = setup.attacker;
    r.defender = setup.defender;
    r.victor = victor;
    r.autoResolved = autoResolved;
    r.structuresDestroyed = structuresDestroyed;
    r.day = gs.date().day;
    fillSide(gs, r.attackerSide, setup.attacker, setup.attackerUnits, byId);
    fillSide(gs, r.defenderSide, setup.defender, setup.defenderUnits, byId);
    return r;
}

// ---------------------------------------------------------------------------
// Autoresolve
// ---------------------------------------------------------------------------
namespace autoresolve {

namespace {

struct Combatant {
    Id instanceId = kInvalid;  ///< kInvalid for carried squadrons and structures.
    Id structureId = kInvalid;
    Id defId = kInvalid;
    float hp = 0.0f;
    float maxHp = 0.0f;
    float dmgAntiCapital = 0.0f;
    float dmgAntiFighter = 0.0f;
    bool squadron = false;
    bool structure = false;
    int parent = -1;  ///< Index of the carrier that launched this squadron.

    bool alive() const { return hp > 0.0f; }
};

struct Side {
    Faction faction = Faction::Neutral;
    std::vector<Combatant> units;
    float bonus = 0.0f;
    float initialStrength = 0.0f;
    bool broken = false;
};

float combatantStrength(const Combatant& c) {
    return (c.maxHp * 0.02f) + (c.dmgAntiCapital + c.dmgAntiFighter) * 1.0f;
}

void addUnit(const GameState& gs, Side& side, Id instanceId, Domain domain) {
    const UnitInstance& inst = gs.unit(instanceId);
    const UnitDef& d = inst.def();
    Combatant c;
    c.instanceId = instanceId;
    c.defId = d.id;
    c.maxHp = d.hull + d.shield;
    c.hp = c.maxHp * inst.health;
    c.dmgAntiCapital = d.damageAntiCapital * d.accuracy;
    c.dmgAntiFighter = d.damageAntiFighter * d.accuracy;
    c.squadron = d.isSquadron();
    side.bonus += d.heroCombatBonus;
    int parentIndex = static_cast<int>(side.units.size());
    side.units.push_back(c);

    // Carriers launch their wings for the duration of the battle.
    if (domain == Domain::Space) {
        for (const CarriedWing& w : d.wings) {
            if (w.unitId == kInvalid) continue;
            const UnitDef& wd = db().unit(w.unitId);
            for (int i = 0; i < w.count; ++i) {
                Combatant s;
                s.defId = wd.id;
                s.maxHp = wd.hull + wd.shield;
                s.hp = s.maxHp;
                s.dmgAntiCapital = wd.damageAntiCapital * wd.accuracy;
                s.dmgAntiFighter = wd.damageAntiFighter * wd.accuracy;
                s.squadron = true;
                s.parent = parentIndex;
                side.units.push_back(s);
            }
        }
    }
}

void addStructure(const GameState& gs, Side& side, Id buildingId) {
    const BuildingInstance& b = gs.buildingInst(buildingId);
    const BuildingDef& bd = b.def();
    Combatant c;
    c.structureId = buildingId;
    c.defId = bd.id;
    c.maxHp = bd.defenceHp + bd.shieldStrength;
    c.hp = c.maxHp * b.hp;
    c.dmgAntiCapital = bd.defenceDamage;
    c.dmgAntiFighter = bd.defenceDamage * 0.6f;
    c.structure = true;
    side.units.push_back(c);
}

float sideStrength(const Side& s) {
    float total = 0.0f;
    for (const Combatant& c : s.units) {
        if (c.alive()) total += combatantStrength(c) * (c.hp / std::max(1.0f, c.maxHp));
    }
    return total;
}

/// Spread `damage` over the given targets, killing the weakest first.
void applyDamage(Side& target, float damage, bool againstSquadrons) {
    if (damage <= 0.0f) return;
    std::vector<int> targets;
    for (size_t i = 0; i < target.units.size(); ++i) {
        const Combatant& c = target.units[i];
        if (!c.alive()) continue;
        if (c.squadron == againstSquadrons) targets.push_back(static_cast<int>(i));
    }
    float efficiency = 1.0f;
    if (targets.empty()) {
        // Wrong weapon for the job, but something is better than nothing.
        for (size_t i = 0; i < target.units.size(); ++i) {
            if (target.units[i].alive()) targets.push_back(static_cast<int>(i));
        }
        efficiency = againstSquadrons ? 0.35f : 0.30f;
    }
    if (targets.empty()) return;

    float remaining = damage * efficiency;
    // Two passes: spread evenly, then dump any leftovers on survivors.
    for (int pass = 0; pass < 3 && remaining > 0.01f; ++pass) {
        std::vector<int> alive;
        for (int i : targets) {
            if (target.units[static_cast<size_t>(i)].alive()) alive.push_back(i);
        }
        if (alive.empty()) return;
        float share = remaining / static_cast<float>(alive.size());
        remaining = 0.0f;
        for (int i : alive) {
            Combatant& c = target.units[static_cast<size_t>(i)];
            float applied = std::min(share, c.hp);
            c.hp -= applied;
            remaining += share - applied;
            if (c.hp <= 0.0f) c.hp = 0.0f;  // squadrons die with their carrier below
        }
    }
}

void killOrphanedSquadrons(Side& s) {
    for (size_t i = 0; i < s.units.size(); ++i) {
        Combatant& c = s.units[i];
        if (c.parent >= 0 && !s.units[static_cast<size_t>(c.parent)].alive()) c.hp = 0.0f;
    }
}

}  // namespace

float forceStrength(const GameState& gs, const std::vector<Id>& unitIds, Domain domain) {
    Side s;
    for (Id id : unitIds) {
        if (!gs.unit(id).alive) continue;
        addUnit(gs, s, id, domain);
    }
    return sideStrength(s) * (1.0f + s.bonus);
}

BattleResolution resolve(const GameState& gs, const BattleSetup& setup, Rng& rng) {
    Side atk;
    atk.faction = setup.attacker;
    Side def;
    def.faction = setup.defender;

    for (Id id : setup.attackerUnits) {
        if (gs.unit(id).alive) addUnit(gs, atk, id, setup.domain);
    }
    for (Id id : setup.defenderUnits) {
        if (gs.unit(id).alive) addUnit(gs, def, id, setup.domain);
    }
    for (Id id : setup.defenderStructures) {
        if (gs.buildingInst(id).alive) addStructure(gs, def, id);
    }

    atk.bonus += setup.attackerBonus;
    def.bonus += setup.defenderBonus;
    // The AI difficulty handicap applies to whichever side is not the player.
    float aiMult = difficultyAiCombatMult(gs.setup().difficulty);
    if (atk.faction != gs.playerFaction()) atk.bonus += (aiMult - 1.0f);
    if (def.faction != gs.playerFaction()) def.bonus += (aiMult - 1.0f);

    atk.initialStrength = sideStrength(atk);
    def.initialStrength = sideStrength(def);

    Faction victor = Faction::Neutral;
    const int kMaxRounds = 40;
    for (int round = 0; round < kMaxRounds; ++round) {
        float atkAC = 0.0f, atkAF = 0.0f, defAC = 0.0f, defAF = 0.0f;
        for (const Combatant& c : atk.units) {
            if (!c.alive()) continue;
            float eff = 0.55f + 0.45f * (c.hp / std::max(1.0f, c.maxHp));
            atkAC += c.dmgAntiCapital * eff;
            atkAF += c.dmgAntiFighter * eff;
        }
        for (const Combatant& c : def.units) {
            if (!c.alive()) continue;
            float eff = 0.55f + 0.45f * (c.hp / std::max(1.0f, c.maxHp));
            defAC += c.dmgAntiCapital * eff;
            defAF += c.dmgAntiFighter * eff;
        }
        float atkRoll = rng.range(0.85f, 1.15f) * (1.0f + atk.bonus);
        float defRoll = rng.range(0.85f, 1.15f) * (1.0f + def.bonus);

        applyDamage(def, atkAC * atkRoll, false);
        applyDamage(def, atkAF * atkRoll, true);
        applyDamage(atk, defAC * defRoll, false);
        applyDamage(atk, defAF * defRoll, true);
        killOrphanedSquadrons(atk);
        killOrphanedSquadrons(def);

        float as = sideStrength(atk);
        float ds = sideStrength(def);
        if (as <= 0.0f && ds <= 0.0f) {
            victor = setup.defender;  // mutual annihilation favours the holder
            break;
        }
        if (as <= 0.0f) {
            victor = setup.defender;
            break;
        }
        if (ds <= 0.0f) {
            victor = setup.attacker;
            break;
        }
        // Morale: a badly mauled and outgunned side withdraws.
        if (as < 0.32f * atk.initialStrength && ds > as * 1.4f) {
            atk.broken = true;
            victor = setup.defender;
            break;
        }
        if (ds < 0.32f * def.initialStrength && as > ds * 1.4f) {
            def.broken = true;
            victor = setup.attacker;
            break;
        }
    }
    if (victor == Faction::Neutral) {
        float as = sideStrength(atk);
        float ds = sideStrength(def);
        victor = (as > ds) ? setup.attacker : setup.defender;
        (victor == setup.attacker ? def : atk).broken = true;
    }

    BattleResolution res;
    res.victor = victor;

    auto emit = [&](Side& side, bool lost) {
        for (const Combatant& c : side.units) {
            if (c.instanceId == kInvalid) continue;
            UnitOutcome o;
            o.instanceId = c.instanceId;
            if (!c.alive()) {
                o.destroyed = true;
            } else if (lost) {
                // Broken formations pull out; stragglers are overrun.
                bool escapes = side.broken ? rng.chance(0.80f) : rng.chance(0.45f);
                o.destroyed = !escapes;
                o.retreated = escapes;
                o.health = std::max(0.1f, c.hp / std::max(1.0f, c.maxHp));
            } else {
                o.health = std::max(0.1f, c.hp / std::max(1.0f, c.maxHp));
            }
            res.outcomes.push_back(o);
        }
    };
    emit(atk, victor != setup.attacker);
    emit(def, victor != setup.defender);

    // Structures: destroyed outright when their side loses, or when shot away.
    int structuresDestroyed = 0;
    for (const Combatant& c : def.units) {
        if (c.structureId == kInvalid) continue;
        if (!c.alive() || victor != setup.defender) {
            res.destroyedStructures.push_back(c.structureId);
            ++structuresDestroyed;
        }
    }

    res.report = buildReport(gs, setup, res.outcomes, victor, structuresDestroyed, true);
    return res;
}

}  // namespace autoresolve
}  // namespace gc
