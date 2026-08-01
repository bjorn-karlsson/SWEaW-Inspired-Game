// Console client. Plays the full galactic campaign from a terminal with no
// dependencies at all - handy for testing balance, for headless machines, and
// for watching the AI play itself.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sim/GameState.h"

using namespace gc;

namespace {

std::string pad(const std::string& s, size_t n) {
    std::string out = s.substr(0, n);
    while (out.size() < n) out += ' ';
    return out;
}

void printHelp() {
    std::printf(
        "\nCOMMANDS\n"
        "  map                     list every world\n"
        "  mine                    list the worlds you control\n"
        "  info <planet>           full detail on a world\n"
        "  build <planet> <n>      queue the n-th buildable unit there\n"
        "  struct <planet> <n>     queue the n-th buildable structure there\n"
        "  tech [n]                list technologies, or research the n-th\n"
        "  move <from> <to>        send everything you have at <from> to <to>\n"
        "  land <planet>           land your troops in orbit (invasion)\n"
        "  wait [days]             let time pass (default 7)\n"
        "  auto                    auto-resolve the pending battle\n"
        "  log                     recent holonet reports\n"
        "  help / quit\n"
        "Planets can be given by name or by index.\n\n");
}

Id findPlanet(const GameState& gs, const std::string& token) {
    if (token.empty()) return kInvalid;
    if (std::all_of(token.begin(), token.end(), [](char c) { return std::isdigit(c) != 0; })) {
        int idx = std::atoi(token.c_str());
        if (idx >= 0 && idx < gs.planetCount()) return idx;
    }
    std::string lower;
    for (char c : token) lower += static_cast<char>(std::tolower(c));
    for (int i = 0; i < gs.planetCount(); ++i) {
        std::string name = gs.planet(i).def().name;
        std::string key = gs.planet(i).def().key;
        std::string nameLower;
        for (char c : name) nameLower += static_cast<char>(std::tolower(c));
        if (nameLower.rfind(lower, 0) == 0 || key.rfind(lower, 0) == 0) return i;
    }
    return kInvalid;
}

void printPlanetLine(const GameState& gs, Id i) {
    const PlanetState& p = gs.planet(i);
    int space = static_cast<int>(gs.unitsAt(i, p.owner, Domain::Space).size());
    int ground = static_cast<int>(gs.unitsAt(i, p.owner, Domain::Ground).size());
    std::printf("  %3d %s %s  income %4d  garrison %d ships / %d troops%s\n", i,
                pad(p.def().name, 20).c_str(), pad(factionShortName(p.owner), 9).c_str(),
                gs.planetIncome(i), space, ground, gs.isContested(i) ? "  [CONTESTED]" : "");
}

void printInfo(GameState& gs, Id i) {
    const PlanetState& p = gs.planet(i);
    const PlanetDef& pd = p.def();
    Faction me = gs.playerFaction();
    std::printf("\n=== %s (%s) - %s ===\n", pd.name.c_str(), pd.region.c_str(),
                factionShortName(p.owner));
    std::printf("%s\n", pd.description.c_str());
    if (pd.spaceOnly) std::printf("SPACE-ONLY SYSTEM: capture it by holding orbit.\n");
    for (Trait t : pd.traits) std::printf("  trait: %-20s %s\n", traitName(t), traitDescription(t));
    std::printf("  income %d/week   space units %d/%d   ground units %d/%d\n", gs.planetIncome(i),
                gs.usedUnitSlots(i, p.owner, Domain::Space), gs.unitSlotCapacity(i, Domain::Space),
                gs.usedUnitSlots(i, p.owner, Domain::Ground), gs.unitSlotCapacity(i, Domain::Ground));
    std::printf("  structures: space %d/%d, ground %d/%d\n", gs.usedBuildSlots(i, Domain::Space),
                gs.buildSlotCapacity(i, Domain::Space), gs.usedBuildSlots(i, Domain::Ground),
                gs.buildSlotCapacity(i, Domain::Ground));

    if (!p.buildings.empty()) {
        std::printf("  built:");
        for (Id bid : p.buildings) {
            if (gs.buildingInst(bid).alive) std::printf(" [%s]", gs.buildingInst(bid).def().name.c_str());
        }
        std::printf("\n");
    }
    std::printf("  forces:\n");
    for (Id uid : p.units) {
        const UnitInstance& u = gs.unit(uid);
        if (!u.alive) continue;
        std::printf("    %-9s %-32s %3d%% %s\n", factionShortName(u.owner), u.def().name.c_str(),
                    static_cast<int>(u.health * 100.0f),
                    u.def().domain() == Domain::Ground ? (u.landed ? "(surface)" : "(in orbit)") : "");
    }
    std::printf("  neighbours:");
    for (Id nb : gs.neighbours(i)) {
        std::printf(" %s(%.1fd%s)", gs.planet(nb).def().name.c_str(), gs.laneTravelDays(i, nb),
                    gs.laneIsHyperlane(i, nb) ? ",hyper" : "");
    }
    std::printf("\n");

    if (p.owner == me) {
        std::vector<Id> units = gs.buildableUnits(i, me);
        std::printf("  buildable units:\n");
        for (size_t n = 0; n < units.size(); ++n) {
            OrderResult can = gs.canQueueUnit(i, units[n], me);
            std::printf("    %2zu) %-34s %6d cr %4.1f d  %s\n", n, db().unit(units[n]).name.c_str(),
                        gs.unitCost(i, units[n]), static_cast<double>(gs.unitBuildDays(i, units[n])),
                        can.ok ? "" : ("- " + can.message).c_str());
        }
        std::vector<Id> structures = gs.buildableBuildings(i, me);
        std::printf("  buildable structures:\n");
        for (size_t n = 0; n < structures.size(); ++n) {
            OrderResult can = gs.canQueueBuilding(i, structures[n], me);
            std::printf("    %2zu) %-34s %6d cr %4.1f d  %s\n", n,
                        db().building(structures[n]).name.c_str(), gs.buildingCost(i, structures[n]),
                        static_cast<double>(gs.buildingBuildDays(i, structures[n])),
                        can.ok ? "" : ("- " + can.message).c_str());
        }
        if (!p.queue.empty()) {
            std::printf("  queue:");
            for (const BuildOrder& o : p.queue) {
                std::printf(" %s(%.1fd)",
                            (o.kind == BuildKind::Unit ? db().unit(o.defId).name
                                                       : db().building(o.defId).name)
                                .c_str(),
                            static_cast<double>(o.daysRemaining));
            }
            std::printf("\n");
        }
    }
    std::printf("\n");
}

void printStatus(const GameState& gs) {
    Faction me = gs.playerFaction();
    std::printf("\n[%s] %s | credits %d (+%d/week) | worlds:", factionShortName(me),
                gs.date().toString().c_str(), gs.faction(me).credits, gs.factionIncome(me));
    for (Faction f : playableFactions()) {
        std::printf(" %s %d", factionShortName(f), gs.planetsOwned(f));
    }
    std::printf("\n");
}

void printReport(const BattleReport& r) {
    std::printf("\n--- %s battle over %s: %s victory ---\n",
                r.domain == Domain::Space ? "Space" : "Ground", r.planetName.c_str(),
                factionShortName(r.victor));
    auto side = [](const SideSummary& s, const char* role) {
        std::printf("  %s %s: committed %d, destroyed %d, survived %d (withdrew %d), losses %d cr\n",
                    role, factionShortName(s.faction), s.committed, s.lost, s.survived, s.retreated,
                    s.creditsLost);
        for (const SummaryEntry& e : s.entries) {
            std::printf("     %-34s sent %2d  lost %2d  left %2d\n", db().unit(e.defId).name.c_str(),
                        e.committed, e.lost, e.survived);
        }
    };
    side(r.attackerSide, "attacker");
    side(r.defenderSide, "defender");
    if (r.structuresDestroyed > 0) {
        std::printf("  %d defence structures destroyed\n", r.structuresDestroyed);
    }
    std::printf("\n");
}

/// Runs the simulation forward, stopping early for a battle that needs the
/// player's decision.
void advance(GameState& gs, float days) {
    const float step = 0.25f;  // seconds of wall clock per iteration
    float target = static_cast<float>(gs.date().day) + days;
    int guard = 0;
    gs.setSpeed(GameSpeed::Fastest);
    while (static_cast<float>(gs.date().day) < target && guard++ < 400000) {
        if (gs.hasPendingPlayerBattle()) {
            const PendingBattle* pb = gs.pendingPlayerBattle();
            std::printf("\n!! %s battle at %s: %s attacks %s. Type 'auto' to resolve it.\n",
                        pb->setup.domain == Domain::Space ? "Space" : "Ground",
                        gs.planet(pb->setup.planet).def().name.c_str(),
                        factionShortName(pb->setup.attacker), factionShortName(pb->setup.defender));
            return;
        }
        if (gs.outcome() != GameOutcome::InProgress) return;
        gs.update(step);
    }
}

}  // namespace

int main(int argc, char** argv) {
    GameSetup setup;
    setup.campaign = 0;
    setup.playerFaction = Faction::Republic;
    setup.difficulty = Difficulty::Normal;

    bool autoPlay = false;
    int autoDays = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--campaign" && i + 1 < argc) {
            setup.campaign = std::atoi(argv[++i]);
        } else if (a == "--faction" && i + 1 < argc) {
            int f = std::atoi(argv[++i]);
            setup.playerFaction = playableFactions()[static_cast<size_t>(
                std::max(0, std::min(f, 2)))];
        } else if (a == "--difficulty" && i + 1 < argc) {
            setup.difficulty = static_cast<Difficulty>(std::max(0, std::min(3, std::atoi(argv[++i]))));
        } else if (a == "--seed" && i + 1 < argc) {
            setup.seed = static_cast<uint64_t>(std::atoll(argv[++i]));
        } else if (a == "--simulate" && i + 1 < argc) {
            autoPlay = true;
            autoDays = std::atoi(argv[++i]);
        } else if (a == "--help") {
            std::printf(
                "galactic-conquest-console [--campaign N] [--faction N] [--difficulty N]\n"
                "                          [--seed N] [--simulate DAYS]\n"
                "--simulate runs an AI-vs-AI game for DAYS days and prints the result.\n");
            return 0;
        }
    }
    setup.campaign = std::max(0, std::min(setup.campaign, static_cast<int>(db().campaigns().size()) - 1));

    GameState gs;
    gs.start(setup);

    if (autoPlay) {
        // Hands-off run: every battle the player is dragged into is auto-resolved.
        gs.setSpeed(GameSpeed::Fastest);
        int guard = 0;
        while (gs.date().day < autoDays && gs.outcome() == GameOutcome::InProgress &&
               guard++ < 4000000) {
            if (gs.hasPendingPlayerBattle()) {
                gs.autoResolvePendingBattle();
                continue;
            }
            gs.update(0.25f);
        }
        std::printf("After %d days (%s):\n", gs.date().day, gs.date().toString().c_str());
        for (Faction f : playableFactions()) {
            int units = 0;
            for (const UnitInstance& u : gs.units()) {
                if (u.alive && u.owner == f) ++units;
            }
            int techs = 0;
            for (Id t : db().factionTechs(f)) {
                if (gs.techKnown(f, t)) ++techs;
            }
            int capitals = 0, structures = 0;
            for (const UnitInstance& u : gs.units()) {
                if (u.alive && u.owner == f && u.def().unitClass == UnitClass::Capital) ++capitals;
            }
            for (int i = 0; i < gs.planetCount(); ++i) {
                for (Id bid : gs.planet(i).buildings) {
                    if (gs.buildingInst(bid).alive && gs.buildingInst(bid).owner == f) ++structures;
                }
            }
            std::printf(
                "  %-9s %2d worlds, %3d units (%d capital ships), %2d structures, %2d/%zu techs, "
                "%7d credits, income %6d/week\n",
                factionShortName(f), gs.planetsOwned(f), units, capitals, structures, techs,
                db().factionTechs(f).size(), gs.faction(f).credits, gs.factionIncome(f));
        }
        std::printf("  battles fought: %zu\n", gs.battleLog().size());
        return 0;
    }

    std::printf("=== GALACTIC CONQUEST (console) ===\n%s\nYou command the %s.\n",
                db().campaign(setup.campaign).name.c_str(), factionName(setup.playerFaction));
    printHelp();
    printStatus(gs);

    std::string line;
    while (gs.outcome() == GameOutcome::InProgress) {
        std::printf("> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) continue;
        Faction me = gs.playerFaction();

        if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "map") {
            for (int i = 0; i < gs.planetCount(); ++i) printPlanetLine(gs, i);
        } else if (cmd == "mine") {
            for (int i = 0; i < gs.planetCount(); ++i) {
                if (gs.planet(i).owner == me) printPlanetLine(gs, i);
            }
        } else if (cmd == "info") {
            std::string token;
            iss >> token;
            Id p = findPlanet(gs, token);
            if (p == kInvalid) {
                std::printf("No such world.\n");
            } else {
                printInfo(gs, p);
            }
        } else if (cmd == "build" || cmd == "struct") {
            std::string token;
            int n = -1;
            iss >> token >> n;
            Id p = findPlanet(gs, token);
            if (p == kInvalid || n < 0) {
                std::printf("Usage: %s <planet> <index>\n", cmd.c_str());
                continue;
            }
            std::vector<Id> options =
                cmd == "build" ? gs.buildableUnits(p, me) : gs.buildableBuildings(p, me);
            if (n >= static_cast<int>(options.size())) {
                std::printf("No such entry. Use 'info %s'.\n", token.c_str());
                continue;
            }
            OrderResult r = cmd == "build" ? gs.queueUnit(p, options[static_cast<size_t>(n)], me)
                                           : gs.queueBuilding(p, options[static_cast<size_t>(n)], me);
            std::printf("%s\n", r.message.c_str());
        } else if (cmd == "tech") {
            int n = -1;
            iss >> n;
            std::vector<Id> options = gs.researchableTechs(me);
            if (n < 0) {
                if (!gs.faction(me).research.empty()) {
                    const ResearchOrder& r = gs.faction(me).research.front();
                    std::printf("  researching %s (%.1f days left)\n", db().tech(r.techId).name.c_str(),
                                static_cast<double>(r.daysRemaining));
                }
                for (size_t i = 0; i < options.size(); ++i) {
                    const TechDef& t = db().tech(options[i]);
                    std::printf("  %2zu) %-34s %6d cr %4.1f d  unlocks: %s\n", i, t.name.c_str(),
                                gs.techCost(me, options[i]),
                                static_cast<double>(gs.techDays(me, options[i])), t.unlocksText.c_str());
                }
            } else if (n < static_cast<int>(options.size())) {
                std::printf("%s\n", gs.startResearch(me, options[static_cast<size_t>(n)]).message.c_str());
            }
        } else if (cmd == "move") {
            std::string a, b;
            iss >> a >> b;
            Id from = findPlanet(gs, a);
            Id to = findPlanet(gs, b);
            if (from == kInvalid || to == kInvalid) {
                std::printf("Usage: move <from> <to>\n");
                continue;
            }
            std::vector<Id> units = gs.allUnitsAt(from, me);
            if (units.empty()) {
                std::printf("You have no forces at %s.\n", gs.planet(from).def().name.c_str());
                continue;
            }
            std::printf("%s\n", gs.moveUnits(units, to).message.c_str());
        } else if (cmd == "land") {
            std::string token;
            iss >> token;
            Id p = findPlanet(gs, token);
            if (p == kInvalid) {
                std::printf("No such world.\n");
                continue;
            }
            std::vector<Id> troops = gs.unitsAt(p, me, Domain::Ground, false, true);
            OrderResult r = gs.invade(p, troops, me);
            std::printf("%s\n", r.message.c_str());
            while (gs.hasPendingPlayerBattle()) printReport(gs.autoResolvePendingBattle());
        } else if (cmd == "auto") {
            if (!gs.hasPendingPlayerBattle()) {
                std::printf("No battle is waiting.\n");
            }
            while (gs.hasPendingPlayerBattle()) printReport(gs.autoResolvePendingBattle());
        } else if (cmd == "wait") {
            float days = 7.0f;
            float given = 0.0f;
            if (iss >> given) days = given;
            size_t battlesBefore = gs.battleLog().size();
            advance(gs, days);
            for (size_t i = battlesBefore; i < gs.battleLog().size(); ++i) {
                const BattleReport& r = gs.battleLog()[i];
                std::printf("  battle at %s: %s victory (%d + %d units lost)\n", r.planetName.c_str(),
                            factionShortName(r.victor), r.attackerSide.lost, r.defenderSide.lost);
            }
            printStatus(gs);
        } else if (cmd == "log") {
            const std::deque<GameEvent>& events = gs.events();
            size_t start = events.size() > 20 ? events.size() - 20 : 0;
            for (size_t i = start; i < events.size(); ++i) {
                std::printf("  d%-4d %s\n", events[i].day, events[i].text.c_str());
            }
        } else {
            std::printf("Unknown command. Type 'help'.\n");
        }
    }

    if (gs.outcome() == GameOutcome::Victory) {
        std::printf("\nTOTAL VICTORY - the galaxy is yours after %s.\n", gs.date().toString().c_str());
    } else if (gs.outcome() == GameOutcome::Defeat) {
        std::printf("\nDEFEAT - your faction has been destroyed.\n");
    }
    return 0;
}
