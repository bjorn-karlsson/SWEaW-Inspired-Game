#include "data/UnitMods.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace gc {
namespace unitmods {

const char* kDefaultPath = "unitmods.txt";

namespace {

std::string rest(std::istringstream& in) {
    std::string s;
    std::getline(in, s);
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    return s;
}

/// Fresh unit used as the starting point for a key the database has never
/// seen. The designer fills in the rest.
UnitDef blankUnit(const std::string& key) {
    UnitDef u;
    u.key = key;
    u.name = key;
    u.faction = Faction::Republic;
    u.unitClass = UnitClass::Corvette;
    u.custom = true;
    return u;
}

}  // namespace

std::string serialise(const UnitDef& u) {
    std::ostringstream o;
    o << "unit " << u.key << "\n";
    o << "  name " << u.name << "\n";
    o << "  faction " << static_cast<int>(u.faction) << "\n";
    o << "  class " << static_cast<int>(u.unitClass) << "\n";
    if (!u.role.empty()) o << "  role " << u.role << "\n";
    if (!u.manufacturer.empty()) o << "  maker " << u.manufacturer << "\n";
    if (!u.description.empty()) o << "  desc " << u.description << "\n";
    o << "  cost " << u.cost << " " << u.upkeep << " " << u.buildDays << " " << u.popCost << " "
      << u.requiredTier << "\n";
    o << "  tech " << (u.requiredTech == kInvalid ? std::string("-") : db().tech(u.requiredTech).key)
      << "\n";
    o << "  stats " << u.hull << " " << u.shield << " " << u.shieldRegen << " " << u.damageAntiCapital
      << " " << u.damageAntiFighter << " " << u.range << " " << u.speed << " " << u.accuracy << "\n";
    o << "  hero " << (u.isHero ? 1 : 0) << " " << u.heroCombatBonus << " " << u.heroIncomeBonus << " "
      << u.respawnDays << "\n";
    o << "  look " << static_cast<int>(u.look.shape) << " " << u.look.length << " " << u.look.beam << " "
      << u.look.engines << " " << (u.look.useFactionColour ? 1 : 0) << " " << u.look.primary[0] << " "
      << u.look.primary[1] << " " << u.look.primary[2] << " " << u.look.secondary[0] << " "
      << u.look.secondary[1] << " " << u.look.secondary[2] << " " << u.look.accent[0] << " "
      << u.look.accent[1] << " " << u.look.accent[2] << "\n";
    for (const CarriedWing& w : u.wings) {
        o << "  wing " << w.unitKey << " " << w.count << "\n";
    }
    for (const HullPart& part : u.look.parts) {
        o << "  part " << static_cast<int>(part.shape) << " " << static_cast<int>(part.tint) << " "
          << part.x << " " << part.y << " " << part.w << " " << part.h << " "
          << (part.mirrored ? 1 : 0) << "\n";
    }
    for (const Hardpoint& h : u.hardpoints) {
        // "mount" carries the whole turret; the older "hardpoint" line, which
        // stopped at the mount's position, is still read for old files.
        o << "  mount " << static_cast<int>(h.type) << " " << h.damage << " " << h.range << " "
          << h.health << " " << h.offsetX << " " << h.offsetY << " "
          << static_cast<int>(h.projectile) << " " << h.barrels << " " << h.reload << " "
          << h.projectileSpeed << " " << h.tracking << " " << (h.useOwnColour ? 1 : 0) << " "
          << h.colour[0] << " " << h.colour[1] << " " << h.colour[2] << " " << h.name << "\n";
    }
    o << "end\n";
    return o.str();
}

int load(Database& d, const std::string& path) {
    std::ifstream file(path);
    if (!file.good()) return -1;

    int touched = 0;
    std::string line;
    UnitDef* current = nullptr;
    bool clearedWings = false;
    bool clearedHardpoints = false;
    bool clearedParts = false;

    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string token;
        if (!(in >> token)) continue;
        if (token.empty() || token[0] == '#') continue;

        if (token == "unit") {
            std::string key;
            in >> key;
            Id id = d.unitId(key);
            if (id == kInvalid) id = d.createUnit(key, blankUnit(key));
            current = &d.unitMutable(id);
            current->custom = true;
            clearedWings = false;
            clearedHardpoints = false;
            clearedParts = false;
            ++touched;
            continue;
        }
        if (token == "end") {
            current = nullptr;
            continue;
        }
        if (current == nullptr) continue;

        if (token == "name") {
            current->name = rest(in);
        } else if (token == "role") {
            current->role = rest(in);
        } else if (token == "maker") {
            current->manufacturer = rest(in);
        } else if (token == "desc") {
            current->description = rest(in);
        } else if (token == "faction") {
            int v = 1;
            in >> v;
            current->faction = factionFromIndex(v);
        } else if (token == "class") {
            int v = 0;
            in >> v;
            current->unitClass = static_cast<UnitClass>(v);
        } else if (token == "cost") {
            in >> current->cost >> current->upkeep >> current->buildDays >> current->popCost >>
                current->requiredTier;
        } else if (token == "tech") {
            std::string key;
            in >> key;
            current->requiredTech = (key == "-") ? kInvalid : d.techId(key);
        } else if (token == "stats") {
            in >> current->hull >> current->shield >> current->shieldRegen >>
                current->damageAntiCapital >> current->damageAntiFighter >> current->range >>
                current->speed >> current->accuracy;
        } else if (token == "hero") {
            int isHero = 0;
            in >> isHero >> current->heroCombatBonus >> current->heroIncomeBonus >>
                current->respawnDays;
            current->isHero = isHero != 0;
        } else if (token == "look") {
            int shape = 0;
            int useFaction = 1;
            in >> shape >> current->look.length >> current->look.beam >> current->look.engines >>
                useFaction >> current->look.primary[0] >> current->look.primary[1] >>
                current->look.primary[2] >> current->look.secondary[0] >> current->look.secondary[1] >>
                current->look.secondary[2] >> current->look.accent[0] >> current->look.accent[1] >>
                current->look.accent[2];
            current->look.shape = static_cast<HullShape>(shape);
            current->look.useFactionColour = useFaction != 0;
        } else if (token == "wing") {
            if (!clearedWings) {
                current->wings.clear();
                clearedWings = true;
            }
            CarriedWing w;
            in >> w.unitKey >> w.count;
            w.unitId = d.unitId(w.unitKey);
            current->wings.push_back(w);
        } else if (token == "part") {
            if (!clearedParts) {
                current->look.parts.clear();
                clearedParts = true;
            }
            HullPart part;
            int shape = 0;
            int tint = 0;
            int mirrored = 0;
            in >> shape >> tint >> part.x >> part.y >> part.w >> part.h >> mirrored;
            part.shape = static_cast<PartShape>(shape);
            part.tint = static_cast<PartTint>(tint);
            part.mirrored = mirrored != 0;
            current->look.parts.push_back(part);
        } else if (token == "mount") {
            if (!clearedHardpoints) {
                current->hardpoints.clear();
                clearedHardpoints = true;
            }
            Hardpoint h;
            int type = 0;
            int proj = 0;
            int own = 0;
            in >> type >> h.damage >> h.range >> h.health >> h.offsetX >> h.offsetY >> proj >>
                h.barrels >> h.reload >> h.projectileSpeed >> h.tracking >> own >> h.colour[0] >>
                h.colour[1] >> h.colour[2];
            h.type = static_cast<HardpointType>(type);
            h.projectile = static_cast<ProjectileKind>(proj);
            h.useOwnColour = own != 0;
            std::string name = rest(in);
            if (!name.empty()) h.name = name;
            current->hardpoints.push_back(h);
        } else if (token == "hardpoint") {
            if (!clearedHardpoints) {
                current->hardpoints.clear();
                clearedHardpoints = true;
            }
            Hardpoint h;
            int type = 0;
            in >> type >> h.damage >> h.range >> h.health >> h.offsetX >> h.offsetY;
            h.type = static_cast<HardpointType>(type);
            std::string name = rest(in);
            if (!name.empty()) h.name = name;
            current->hardpoints.push_back(h);
        }
    }
    return touched;
}

int save(const Database& d, const std::string& path) {
    std::ofstream file(path);
    if (!file.good()) return -1;
    file << "# Galactic Conquest unit definitions.\n";
    file << "# Written by the in-game designer; edit by hand at your own risk.\n\n";
    int written = 0;
    for (const UnitDef& u : d.units()) {
        if (!u.custom) continue;
        file << serialise(u) << "\n";
        ++written;
    }
    return written;
}

}  // namespace unitmods
}  // namespace gc
