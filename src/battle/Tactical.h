#pragma once

#include <string>
#include <vector>

#include "core/Rng.h"
#include "core/Vec2.h"
#include "data/Database.h"
#include "sim/Combat.h"

namespace gc {

class GameState;

namespace tactical {

/// One combatant on the tactical map.
struct TUnit {
    Id index = kInvalid;        ///< Index into Battle::units().
    Id instanceId = kInvalid;   ///< Campaign unit, kInvalid for launched squadrons.
    Id structureId = kInvalid;  ///< Campaign building, for defence structures.
    Id defId = kInvalid;
    Faction owner = Faction::Neutral;
    bool attackerSide = false;

    Vec2 pos;
    Vec2 vel;
    float radius = 10.0f;

    float hull = 100.0f;
    float maxHull = 100.0f;
    float shield = 0.0f;
    float maxShield = 0.0f;
    float shieldDelay = 0.0f;

    float cooldown = 0.0f;
    Id target = kInvalid;
    Vec2 moveTarget;
    bool hasMoveOrder = false;
    Id forcedTarget = kInvalid;

    bool alive = true;
    bool retreating = false;
    bool escaped = false;
    bool structure = false;
    bool squadron = false;
    int parent = -1;  ///< Carrier index; squadrons die with their carrier.

    /// Muzzle flash bookkeeping for the renderer.
    float firedTimer = 0.0f;
    Vec2 firedAt;

    const UnitDef* def() const;
    float healthFraction() const { return maxHull <= 0.0f ? 0.0f : hull / maxHull; }
};

/// A short-lived weapon trace, purely visual.
struct Shot {
    Vec2 from;
    Vec2 to;
    float life = 0.0f;
    bool heavy = false;
    Faction owner = Faction::Neutral;
};

/// Real-time tactical battle: space or ground. Runs happily without any
/// renderer attached, which is what makes it testable.
class Battle {
public:
    void init(const GameState& gs, const BattleSetup& setup, uint64_t seed);
    void update(float dt);

    bool finished() const { return finished_; }
    Faction victor() const { return victor_; }
    float elapsed() const { return elapsed_; }

    const std::vector<TUnit>& units() const { return units_; }
    const std::vector<Shot>& shots() const { return shots_; }
    const BattleSetup& setup() const { return setup_; }
    Vec2 fieldSize() const { return fieldSize_; }
    Domain domain() const { return setup_.domain; }

    /// Sum of remaining hull for one side, for the strength bars.
    float sideHealth(bool attackerSide) const;
    int sideCount(bool attackerSide) const;

    // --- player commands ---
    void orderMove(const std::vector<Id>& unitIndices, Vec2 target);
    void orderAttack(const std::vector<Id>& unitIndices, Id targetIndex);
    /// Sound the retreat: surviving units run for the edge of the map.
    void beginRetreat(Faction f);
    bool isRetreating(Faction f) const;

    /// Convert the battle into campaign consequences.
    BattleResolution resolution(const GameState& gs) const;

private:
    void spawnSide(const GameState& gs, const std::vector<Id>& unitIds, bool attackerSide, Faction owner);
    void spawnStructures(const GameState& gs, const std::vector<Id>& structureIds, Faction owner);
    void launchWings(const GameState& gs, int carrierIndex);
    void acquireTargets();
    void stepUnit(TUnit& u, float dt);
    void fire(TUnit& u, TUnit& target, float dt);
    void applyDamage(TUnit& target, float damage);
    void checkEnd();
    Vec2 spawnPoint(bool attackerSide, int slot, int total) const;

    BattleSetup setup_;
    std::vector<TUnit> units_;
    std::vector<Shot> shots_;
    Vec2 fieldSize_{1800.0f, 1100.0f};
    Rng rng_;
    float elapsed_ = 0.0f;
    bool finished_ = false;
    Faction victor_ = Faction::Neutral;
    bool attackerRetreating_ = false;
    bool defenderRetreating_ = false;
};

}  // namespace tactical
}  // namespace gc
