#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Rng.h"
#include "core/Types.h"
#include "data/Database.h"
#include "sim/Combat.h"

namespace gc {

// ---------------------------------------------------------------------------
// Live game objects
// ---------------------------------------------------------------------------
struct UnitInstance {
    Id id = kInvalid;
    Id defId = kInvalid;
    Faction owner = Faction::Neutral;
    float health = 1.0f;   ///< Fraction of full hull.
    Id planet = kInvalid;  ///< Where it sits (kInvalid while in a fleet).
    Id fleet = kInvalid;   ///< Fleet it travels with (kInvalid when garrisoned).
    bool landed = false;   ///< Ground units: deployed on the surface.
    bool alive = true;
    /// Which orbital holding slot the unit sits in (0..2). Purely an
    /// organisational device, like Empire at War's fleets: a slot has no
    /// capacity of its own. Ground units on the surface always use slot 0.
    int slot = 0;

    const UnitDef& def() const { return db().unit(defId); }
    bool isSpace() const { return isSpaceClass(db().unit(defId).unitClass); }
};

struct BuildingInstance {
    Id id = kInvalid;
    Id defId = kInvalid;
    Id planet = kInvalid;
    Faction owner = Faction::Neutral;
    float hp = 1.0f;  ///< Fraction of defenceHp remaining.
    bool alive = true;

    const BuildingDef& def() const { return db().building(defId); }
};

enum class BuildKind { Unit, Building };

struct BuildOrder {
    BuildKind kind = BuildKind::Unit;
    Id defId = kInvalid;
    float daysRemaining = 0.0f;
    float totalDays = 1.0f;
    int cost = 0;
};

struct PlanetState {
    Id defId = kInvalid;
    Faction owner = Faction::Neutral;
    std::vector<Id> units;      ///< All unit instances present (space and ground).
    std::vector<Id> buildings;  ///< Building instances present.
    std::vector<BuildOrder> queue;
    int daysSinceCombat = 999;
    /// Last faction whose fleet arrived here; used to decide who is attacking.
    Faction lastAggressor = Faction::Neutral;

    const PlanetDef& def() const { return db().planet(defId); }
};

struct ResearchOrder {
    Id techId = kInvalid;
    float daysRemaining = 0.0f;
    float totalDays = 1.0f;
};

struct FactionState {
    Faction id = Faction::Neutral;
    bool isPlayer = false;
    bool defeated = false;
    int credits = 0;
    int lastIncome = 0;
    std::vector<char> techKnown;      ///< Indexed by tech id.
    std::vector<ResearchOrder> research;
    std::unordered_map<Id, int> heroRespawnTimer;  ///< defId -> days until available again.
};

struct Fleet {
    Id id = kInvalid;
    Faction owner = Faction::Neutral;
    std::vector<Id> units;
    Id from = kInvalid;
    Id to = kInvalid;            ///< Next planet on the path.
    Id finalDestination = kInvalid;
    std::vector<Id> path;        ///< Remaining hops after `to`.
    float legDays = 1.0f;
    float legProgress = 0.0f;    ///< Days travelled on the current leg.
    bool alive = true;

    float fraction() const { return legDays <= 0.0f ? 1.0f : legProgress / legDays; }
};

/// A battle that is waiting to be resolved. Battles involving the player are
/// offered to them (autoresolve or fight it out); the rest resolve instantly.
struct PendingBattle {
    BattleSetup setup;
    bool needsPlayerDecision = false;
};

struct GameEvent {
    int day = 0;
    std::string text;
    Faction faction = Faction::Neutral;
};

// ---------------------------------------------------------------------------
// Settings and results
// ---------------------------------------------------------------------------
struct GameSetup {
    Id campaign = 0;
    Faction playerFaction = Faction::Republic;
    Difficulty difficulty = Difficulty::Normal;
    uint64_t seed = 12345;
};

enum class GameSpeed : int { Paused = 0, Normal = 1, Fast = 2, Fastest = 3 };

enum class GameOutcome { InProgress, Victory, Defeat };

/// Result of trying to issue an order; `ok == false` carries a UI message.
struct OrderResult {
    bool ok = true;
    std::string message;

    static OrderResult fail(std::string m) { return OrderResult{false, std::move(m)}; }
    static OrderResult success(std::string m = std::string()) { return OrderResult{true, std::move(m)}; }
    explicit operator bool() const { return ok; }
};

// ---------------------------------------------------------------------------
// The campaign
// ---------------------------------------------------------------------------
class GameState {
public:
    void start(const GameSetup& setup);

    // --- time ---
    /// Advance the simulation. `dtSeconds` is real time; the current speed
    /// setting converts it into game days.
    void update(float dtSeconds);
    void setSpeed(GameSpeed s) { speed_ = s; }
    GameSpeed speed() const { return speed_; }
    void togglePause();
    bool paused() const { return speed_ == GameSpeed::Paused; }
    const GalacticDate& date() const { return date_; }
    /// 0..1 progress through the current day, for smooth UI animation.
    float dayFraction() const { return dayAccumulator_; }

    // --- accessors ---
    const GameSetup& setup() const { return setup_; }
    Faction playerFaction() const { return setup_.playerFaction; }
    const std::vector<PlanetState>& planets() const { return planets_; }
    PlanetState& planet(Id i) { return planets_[static_cast<size_t>(i)]; }
    const PlanetState& planet(Id i) const { return planets_[static_cast<size_t>(i)]; }
    int planetCount() const { return static_cast<int>(planets_.size()); }
    const PlanetDef& planetDef(Id i) const { return db().planet(planets_[static_cast<size_t>(i)].defId); }

    const std::vector<LaneDef>& lanes() const { return lanes_; }
    const std::vector<Id>& neighbours(Id planet) const;

    const UnitInstance& unit(Id id) const { return units_.at(static_cast<size_t>(id)); }
    UnitInstance& unit(Id id) { return units_.at(static_cast<size_t>(id)); }
    const std::vector<UnitInstance>& units() const { return units_; }
    const BuildingInstance& buildingInst(Id id) const { return buildings_.at(static_cast<size_t>(id)); }
    BuildingInstance& buildingInst(Id id) { return buildings_.at(static_cast<size_t>(id)); }

    const std::vector<Fleet>& fleets() const { return fleets_; }
    FactionState& faction(Faction f) { return factions_[static_cast<size_t>(fidx(f))]; }
    const FactionState& faction(Faction f) const { return factions_[static_cast<size_t>(fidx(f))]; }

    const std::deque<GameEvent>& events() const { return events_; }
    const std::vector<BattleReport>& battleLog() const { return battleLog_; }
    GameOutcome outcome() const { return outcome_; }

    // --- queries used by the UI and the AI ---
    std::vector<Id> unitsAt(Id planet, Faction owner, Domain domain, bool landedOnly = false,
                            bool orbitOnly = false) const;
    std::vector<Id> allUnitsAt(Id planet, Faction owner) const;
    int usedUnitSlots(Id planet, Faction owner, Domain domain) const;
    int unitSlotCapacity(Id planet, Domain domain) const;
    int usedBuildSlots(Id planet, Domain domain) const;
    int buildSlotCapacity(Id planet, Domain domain) const;
    bool hasProductionTier(Id planet, Faction owner, Domain domain, int tier) const;
    int bestProductionTier(Id planet, Faction owner, Domain domain) const;
    bool techKnown(Faction f, Id techId) const;
    bool canResearch(Faction f, Id techId) const;

    /// Contested = two or more factions with forces/structures at the planet.
    bool isContested(Id planet) const;
    bool spaceControlledBy(Id planet, Faction f) const;
    /// True when nothing hostile to `f` remains in orbit (units or structures).
    bool orbitClearFor(Id planet, Faction f) const;
    std::vector<Faction> factionsPresent(Id planet) const;

    int unitCost(Id planet, Id unitDefId) const;
    float unitBuildDays(Id planet, Id unitDefId) const;
    int buildingCost(Id planet, Id buildingDefId) const;
    float buildingBuildDays(Id planet, Id buildingDefId) const;
    int techCost(Faction f, Id techId) const;
    float techDays(Faction f, Id techId) const;

    std::vector<Id> buildableUnits(Id planet, Faction f) const;
    std::vector<Id> buildableBuildings(Id planet, Faction f) const;
    std::vector<Id> researchableTechs(Faction f) const;
    OrderResult canQueueUnit(Id planet, Id unitDefId, Faction f) const;
    OrderResult canQueueBuilding(Id planet, Id buildingDefId, Faction f) const;

    int planetIncome(Id planet) const;
    int factionIncome(Faction f) const;
    int planetsOwned(Faction f) const;

    /// Travel time in days between two adjacent planets.
    float laneTravelDays(Id a, Id b) const;
    bool laneIsHyperlane(Id a, Id b) const;
    /// Shortest path (in days) from `from` to `to`; empty when unreachable.
    std::vector<Id> findPath(Id from, Id to) const;
    /// Nearest planet owned by `f` measured in hops; kInvalid when none.
    Id nearestFriendlyPlanet(Id from, Faction f) const;

    // --- orders ---
    OrderResult queueUnit(Id planet, Id unitDefId, Faction f);
    OrderResult queueBuilding(Id planet, Id buildingDefId, Faction f);
    OrderResult cancelBuildOrder(Id planet, int index, Faction f);
    OrderResult startResearch(Faction f, Id techId);
    /// Moves a unit between the three orbital holding slots.
    OrderResult setUnitSlot(Id unitId, int slot, Faction f);
    /// Lifts landed troops back into orbit (they need a friendly orbit).
    OrderResult liftToOrbit(const std::vector<Id>& unitIds, Faction f);
    std::vector<Id> unitsInSlot(Id planet, Faction owner, int slot) const;
    OrderResult moveUnits(const std::vector<Id>& unitIds, Id destination);
    /// Land ground units currently in orbit, starting a ground battle if the
    /// world is defended.
    OrderResult invade(Id planet, const std::vector<Id>& unitIds, Faction f);
    /// Order units at a contested planet to disengage to a friendly world.
    OrderResult retreatFrom(Id planet, Faction f);
    /// Break off a battle before it is fought: every unit of `f` at the planet
    /// runs for the nearest friendly world (or is lost if there is none).
    OrderResult withdraw(Id planet, Faction f);

    // --- battles ---
    bool hasPendingPlayerBattle() const;
    const PendingBattle* pendingPlayerBattle() const;
    /// Resolve the pending player battle statistically.
    BattleReport autoResolvePendingBattle();
    /// Take the pending battle out of the queue so the tactical battler can
    /// run it; apply the result afterwards with `applyBattleResolution`.
    bool takePendingBattle(BattleSetup& out);
    void applyBattleResolution(const BattleSetup& setup, const BattleResolution& res);

    // --- misc ---
    Rng& rng() { return rng_; }
    void log(const std::string& text, Faction f = Faction::Neutral);
    /// Total strength estimate, used for the odds readout.
    float forceStrengthAt(Id planet, Faction f, Domain domain) const;

private:
    // setup helpers
    void buildGalaxy(const CampaignDef& c);
    void applyFactionStart(const FactionStart& s);
    void seedNeutralGarrisons();

    // per-day processing
    void advanceDay();
    void tickProduction();
    void tickResearch();
    void tickFleets(float days);
    void payWeeklyIncome();
    void tickHeroRespawns();
    void resolveAutomaticBattles();
    void detectBattles();
    /// Troop transports left in a hostile orbit with no escort must run or die.
    void handleStrandedTransports();
    void checkVictory();

    Id spawnUnit(Id defId, Faction owner, Id planet, bool landed);
    int emptiestSlot(Id planet, Faction owner) const;
    Id spawnBuilding(Id defId, Faction owner, Id planet);
    void destroyUnit(Id unitId);
    void removeUnitFromPlanet(Id unitId);
    void addUnitToPlanet(Id unitId, Id planet);
    void capturePlanet(Id planet, Faction newOwner);
    void tryAutoCapture(Id planet);
    void retreatUnits(const std::vector<Id>& unitIds, Id fromPlanet);
    void refreshFactionAlive();

    GameSetup setup_;
    GameSpeed speed_ = GameSpeed::Normal;
    GalacticDate date_;
    float dayAccumulator_ = 0.0f;
    Rng rng_;
    GameOutcome outcome_ = GameOutcome::InProgress;

    std::vector<PlanetState> planets_;
    std::vector<LaneDef> lanes_;
    std::vector<std::vector<Id>> adjacency_;
    std::vector<UnitInstance> units_;
    std::vector<BuildingInstance> buildings_;
    std::vector<Fleet> fleets_;
    std::vector<FactionState> factions_;
    std::vector<PendingBattle> pendingBattles_;
    std::vector<BattleReport> battleLog_;
    std::deque<GameEvent> events_;
    std::vector<std::shared_ptr<class AiController>> ais_;

    /// Database planet id -> campaign planet index (kInvalid when the planet
    /// is not part of this campaign).
    std::vector<Id> planetDefToIndex_;
};

/// Seconds of real time per in-game day at normal speed.
constexpr float kSecondsPerDay = 6.0f;

/// Orbital holding slots per planet, and how many divisions fit on a surface.
constexpr int kOrbitSlots = 3;
constexpr int kGroundSlotCapacity = 10;

float speedMultiplier(GameSpeed s);

}  // namespace gc
