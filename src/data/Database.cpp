#include "data/Database.h"

#include <algorithm>

namespace gc {

// ---------------------------------------------------------------------------
// Traits
// ---------------------------------------------------------------------------
const char* traitName(Trait t) {
    switch (t) {
        case Trait::CoreWorld: return "Core World";
        case Trait::TradeHub: return "Trade Hub";
        case Trait::Shipyards: return "Orbital Shipyards";
        case Trait::MiningWorld: return "Mining World";
        case Trait::ForgeWorld: return "Forge World";
        case Trait::FortressWorld: return "Fortress World";
        case Trait::AgriWorld: return "Agri World";
        case Trait::CloneFacility: return "Cloning Facility";
        case Trait::DroidFoundry: return "Droid Foundry";
        case Trait::CriminalHub: return "Criminal Hub";
        case Trait::ResearchCentre: return "Research Centre";
        case Trait::DeepSpace: return "Deep Space Station";
        default: return "?";
    }
}

const char* traitDescription(Trait t) {
    switch (t) {
        case Trait::CoreWorld: return "+40% planetary income";
        case Trait::TradeHub: return "+25% planetary income";
        case Trait::Shipyards: return "Capital ships -20% cost, -30% build time";
        case Trait::MiningWorld: return "Mining complexes may be built here";
        case Trait::ForgeWorld: return "Vehicles -20% cost, -20% build time";
        case Trait::FortressWorld: return "Defences -25% cost, defenders +20% strength";
        case Trait::AgriWorld: return "+80 credits per week";
        case Trait::CloneFacility: return "Infantry -25% cost, -30% build time";
        case Trait::DroidFoundry: return "Droid units -25% cost, -30% build time";
        case Trait::CriminalHub: return "+35% planetary income";
        case Trait::ResearchCentre: return "Technology -20% research cost";
        case Trait::DeepSpace: return "No surface economy";
        default: return "";
    }
}

TraitMods PlanetDef::mods() const {
    TraitMods m;
    for (Trait t : traits) {
        switch (t) {
            case Trait::CoreWorld: m.incomeMult += 0.40f; break;
            case Trait::TradeHub: m.incomeMult += 0.25f; break;
            case Trait::Shipyards:
                m.capitalCostMult *= 0.80f;
                m.capitalTimeMult *= 0.70f;
                break;
            case Trait::MiningWorld: m.allowsMines = true; break;
            case Trait::ForgeWorld:
                m.groundVehicleCostMult *= 0.80f;
                m.groundVehicleTimeMult *= 0.80f;
                break;
            case Trait::FortressWorld:
                m.structureCostMult *= 0.75f;
                m.defenceBonus += 0.20f;
                break;
            case Trait::AgriWorld: m.incomeFlat += 80; break;
            case Trait::CloneFacility:
                m.infantryCostMult *= 0.75f;
                m.infantryTimeMult *= 0.70f;
                break;
            case Trait::DroidFoundry:
                m.infantryCostMult *= 0.75f;
                m.infantryTimeMult *= 0.70f;
                m.groundVehicleCostMult *= 0.85f;
                break;
            case Trait::CriminalHub: m.incomeMult += 0.35f; break;
            case Trait::ResearchCentre: m.researchCostMult *= 0.80f; break;
            case Trait::DeepSpace: break;
            default: break;
        }
    }
    return m;
}

bool PlanetDef::hasTrait(Trait t) const {
    return std::find(traits.begin(), traits.end(), t) != traits.end();
}

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------
const Database& Database::get() {
    static Database instance;
    return instance;
}

Database::Database() { build(); }

void Database::build() {
    content::registerTechs(*this);
    content::registerUnits(*this);
    content::registerBuildings(*this);
    content::registerGalaxy(*this);
    content::registerCampaigns(*this);
    resolveReferences();
}

void Database::resolveReferences() {
    for (UnitDef& u : units_) {
        for (CarriedWing& w : u.wings) {
            w.unitId = unitId(w.unitKey);
        }
    }
}

Id Database::addUnit(UnitDef d) {
    d.id = static_cast<Id>(units_.size());
    unitByKey_[d.key] = d.id;
    units_.push_back(std::move(d));
    return units_.back().id;
}

Id Database::addBuilding(BuildingDef d) {
    d.id = static_cast<Id>(buildings_.size());
    buildingByKey_[d.key] = d.id;
    buildings_.push_back(std::move(d));
    return buildings_.back().id;
}

Id Database::addTech(TechDef d) {
    d.id = static_cast<Id>(techs_.size());
    techByKey_[d.key] = d.id;
    techs_.push_back(std::move(d));
    return techs_.back().id;
}

Id Database::addPlanet(PlanetDef d) {
    d.id = static_cast<Id>(planets_.size());
    planetByKey_[d.key] = d.id;
    planets_.push_back(std::move(d));
    return planets_.back().id;
}

void Database::addLane(const std::string& a, const std::string& b, bool hyperlane) {
    LaneDef l;
    l.a = planetId(a);
    l.b = planetId(b);
    l.hyperlane = hyperlane;
    if (l.a != kInvalid && l.b != kInvalid && l.a != l.b) lanes_.push_back(l);
}

Id Database::addCampaign(CampaignDef d) {
    d.id = static_cast<Id>(campaigns_.size());
    campaignByKey_[d.key] = d.id;
    campaigns_.push_back(std::move(d));
    return campaigns_.back().id;
}

static Id lookup(const std::unordered_map<std::string, Id>& m, const std::string& key) {
    auto it = m.find(key);
    return it == m.end() ? kInvalid : it->second;
}

Id Database::unitId(const std::string& key) const { return lookup(unitByKey_, key); }
Id Database::buildingId(const std::string& key) const { return lookup(buildingByKey_, key); }
Id Database::techId(const std::string& key) const { return lookup(techByKey_, key); }
Id Database::planetId(const std::string& key) const { return lookup(planetByKey_, key); }
Id Database::campaignId(const std::string& key) const { return lookup(campaignByKey_, key); }

const UnitDef* Database::findUnit(const std::string& key) const {
    Id id = unitId(key);
    return id == kInvalid ? nullptr : &units_[static_cast<size_t>(id)];
}

const BuildingDef* Database::findBuilding(const std::string& key) const {
    Id id = buildingId(key);
    return id == kInvalid ? nullptr : &buildings_[static_cast<size_t>(id)];
}

std::vector<Id> Database::factionUnits(Faction f) const {
    std::vector<Id> out;
    for (const UnitDef& u : units_) {
        if (u.faction == f) out.push_back(u.id);
    }
    return out;
}

std::vector<Id> Database::factionBuildings(Faction f) const {
    std::vector<Id> out;
    for (const BuildingDef& b : buildings_) {
        if (b.faction == f || b.faction == Faction::Neutral) out.push_back(b.id);
    }
    return out;
}

std::vector<Id> Database::factionTechs(Faction f) const {
    std::vector<Id> out;
    for (const TechDef& t : techs_) {
        if (t.faction == f) out.push_back(t.id);
    }
    return out;
}

}  // namespace gc
