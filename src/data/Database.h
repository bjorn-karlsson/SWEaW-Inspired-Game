#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "data/Defs.h"

namespace gc {

/// Static content database: every unit, building, technology, planet, lane and
/// campaign in the game. Built once at start-up and immutable afterwards.
class Database {
public:
    static const Database& get();

    // --- units ---
    const std::vector<UnitDef>& units() const { return units_; }
    const UnitDef& unit(Id id) const { return units_[static_cast<size_t>(id)]; }
    Id unitId(const std::string& key) const;
    const UnitDef* findUnit(const std::string& key) const;
    /// All units of a faction (heroes included) in database order.
    std::vector<Id> factionUnits(Faction f) const;

    // --- buildings ---
    const std::vector<BuildingDef>& buildings() const { return buildings_; }
    const BuildingDef& building(Id id) const { return buildings_[static_cast<size_t>(id)]; }
    Id buildingId(const std::string& key) const;
    const BuildingDef* findBuilding(const std::string& key) const;
    /// Buildings a faction may construct (its own plus neutral ones).
    std::vector<Id> factionBuildings(Faction f) const;

    // --- technology ---
    const std::vector<TechDef>& techs() const { return techs_; }
    const TechDef& tech(Id id) const { return techs_[static_cast<size_t>(id)]; }
    Id techId(const std::string& key) const;
    std::vector<Id> factionTechs(Faction f) const;

    // --- galaxy ---
    const std::vector<PlanetDef>& planets() const { return planets_; }
    const PlanetDef& planet(Id id) const { return planets_[static_cast<size_t>(id)]; }
    Id planetId(const std::string& key) const;
    const std::vector<LaneDef>& lanes() const { return lanes_; }

    // --- campaigns ---
    const std::vector<CampaignDef>& campaigns() const { return campaigns_; }
    const CampaignDef& campaign(Id id) const { return campaigns_[static_cast<size_t>(id)]; }
    Id campaignId(const std::string& key) const;

    // --- construction helpers, used by the content files only ---
    Id addUnit(UnitDef d);
    Id addBuilding(BuildingDef d);
    Id addTech(TechDef d);
    Id addPlanet(PlanetDef d);
    void addLane(const std::string& a, const std::string& b, bool hyperlane);
    Id addCampaign(CampaignDef d);
    /// Attaches role and manufacturer flavour to an already registered unit.
    void setUnitFlavour(const std::string& key, const std::string& role, const std::string& maker);
    /// Editable access, used by the in-game designer and the mod loader.
    UnitDef& unitMutable(Id id) { return units_[static_cast<size_t>(id)]; }
    /// Adds a unit at runtime (the designer's "new unit").
    Id createUnit(const std::string& key, const UnitDef& from);

private:
    Database();
    friend Database& editableDb();
    friend Database& mutableInstance();
    void build();
    void resolveReferences();

    std::vector<UnitDef> units_;
    std::vector<BuildingDef> buildings_;
    std::vector<TechDef> techs_;
    std::vector<PlanetDef> planets_;
    std::vector<LaneDef> lanes_;
    std::vector<CampaignDef> campaigns_;

    std::unordered_map<std::string, Id> unitByKey_;
    std::unordered_map<std::string, Id> buildingByKey_;
    std::unordered_map<std::string, Id> techByKey_;
    std::unordered_map<std::string, Id> planetByKey_;
    std::unordered_map<std::string, Id> campaignByKey_;
};

/// Convenience accessor.
inline const Database& db() { return Database::get(); }
/// The same database, editable. Only the designer and the mod loader use it.
Database& editableDb();
Database& mutableInstance();

namespace content {
// Implemented in the individual content files.
void registerTechs(Database& d);
void registerUnits(Database& d);
void registerBuildings(Database& d);
void registerGalaxy(Database& d);
void registerCampaigns(Database& d);
}  // namespace content

}  // namespace gc
