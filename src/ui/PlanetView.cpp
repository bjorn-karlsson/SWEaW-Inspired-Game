#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ui/App.h"

namespace gc {
    namespace ui {

        // ---------------------------------------------------------------------------
        // The world view. Scrolling all the way in on a world dives the camera down to
        // it and opens this screen: three orbital holding slots along the top, the
        // planet with its ten-division surface slot in the middle, the planet dossier
        // on the right, and the structure sockets underneath. Units are dragged from
        // slot to slot with the mouse; dragging troops onto the surface lands them,
        // dragging them back into orbit puts them aboard the transports.
        // ---------------------------------------------------------------------------
        namespace {

            const Color kSlotFill{ 16, 22, 30, 226 };
            const Color kSlotFillHot{ 26, 44, 58, 236 };
            const Color kCellFill{ 22, 30, 40, 235 };
            const Color kCellHot{ 38, 58, 76, 240 };
            const Color kConsoleEdge{ 58, 104, 74 };
            const Color kReadout{ 126, 226, 132 };
            const Color kReadoutDim{ 72, 132, 84 };

            std::string credits(int value) {
                std::string s = std::to_string(std::abs(value));
                for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ".");
                return (value < 0 ? "-" : "") + s;
            }

            std::string classTag(UnitClass c) {
                switch (c) {
                case UnitClass::Capital: return "CAP";
                case UnitClass::Cruiser: return "CRU";
                case UnitClass::Frigate: return "FRG";
                case UnitClass::Corvette: return "COR";
                case UnitClass::Fighter: return "FTR";
                case UnitClass::Bomber: return "BMB";
                case UnitClass::Infantry: return "INF";
                case UnitClass::Vehicle: return "VEH";
                case UnitClass::Artillery: return "ART";
                case UnitClass::AirSupport: return "AIR";
                default: return "???";
                }
            }

            float smoothstep(float t) {
                t = std::max(0.0f, std::min(1.0f, t));
                return t * t * (3.0f - 2.0f * t);
            }

        }  // namespace

        // Defined in GalaxyScreen.cpp; shared so the tiles look the same everywhere.
        void drawUnitGlyphShared(Gfx& g, const Rect& r, UnitClass c, Color col);
        void drawStructureGlyphShared(Gfx& g, const Rect& r, const BuildingDef& b, Color col);

        // ---------------------------------------------------------------------------
        // Entering and leaving
        // ---------------------------------------------------------------------------
        void App::enterPlanetView(Id planet) {
            if (planet == kInvalid) return;
            planetViewTarget_ = planet;
            selectPlanet(planet);
            planetViewDir_ = 1;
        }

        void App::leavePlanetView() {
            planetViewDir_ = -1;
            drag_ = UnitDrag{};
        }

        void App::updatePlanetTransition(float dt) {
            if (planetViewDir_ != 0) {
                planetViewT_ += static_cast<float>(planetViewDir_) * dt / 0.42f;
                if (planetViewT_ >= 1.0f) {
                    planetViewT_ = 1.0f;
                    planetViewDir_ = 0;
                }
                else if (planetViewT_ <= 0.0f) {
                    planetViewT_ = 0.0f;
                    planetViewDir_ = 0;
                    planetViewTarget_ = kInvalid;
                }
            }

            // The camera dives towards the world as the view opens.
            if (planetViewT_ > 0.0f && planetViewTarget_ != kInvalid) {
                float e = smoothstep(planetViewT_);
                viewCamera_ = lerp(camera_, game_.planet(planetViewTarget_).def().pos, e);
                viewZoom_ = zoom_ * (1.0f + 11.0f * e);
            }
            else {
                viewCamera_ = camera_;
                viewZoom_ = zoom_;
            }
        }

        // ---------------------------------------------------------------------------
        // Drag and drop plumbing
        // ---------------------------------------------------------------------------
        void App::addDropTarget(const Rect& r, int slot, bool surface) {
            dropTargets_.push_back(DropTarget{ r, slot, surface });
        }

        bool App::drawUnitCell(const Rect& r, Id unitId, int fromSlot, bool fromSurface, bool compact) {
            const UnitInstance& u = game_.unit(unitId);
            const UnitDef& d = u.def();
            Faction me = game_.playerFaction();
            bool mine = u.owner == me;
            float mx = static_cast<float>(input_.mouseX);
            float my = static_cast<float>(input_.mouseY);
            bool hover = r.contains(mx, my);
            bool selected = std::find(selectedUnits_.begin(), selectedUnits_.end(), unitId) != selectedUnits_.end();
            bool beingDragged =
                drag_.active && std::find(drag_.units.begin(), drag_.units.end(), unitId) != drag_.units.end();

            Color base = beingDragged ? Color(18, 24, 30, 120) : (hover ? kCellHot : kCellFill);
            gfx_.rect(r, base);
            gfx_.rectOutline(r, selected ? pal::kAccent : (mine ? kConsoleEdge : Color(70, 54, 54)));

            Color fc = pal::faction(u.owner);
            Rect icon{ r.x + S(2), r.y + S(2), r.w - S(4), r.h * (compact ? 0.62f : 0.56f) };
            drawUnitGlyphShared(gfx_, icon, d.unitClass, beingDragged ? fc.scaled(0.4f) : fc);
            gfx_.text(r.x + S(3), r.y + S(2), classTag(d.unitClass), kReadoutDim, F(1));
            if (d.isHero) gfx_.textRight(r.right() - S(3), r.y + S(2), "*", pal::kWarning, F(1));

            if (!compact) {
                std::string name = d.name;
                size_t cut = name.find(' ');
                if (name.size() > 12 && cut != std::string::npos && cut <= 12) name = name.substr(0, cut);
                gfx_.textCentred(r.x + r.w * 0.5f, icon.bottom() + S(2), name.substr(0, 12), pal::kText, F(1));
            }
            progressBar(gfx_, Rect{ r.x + S(3), r.bottom() - S(6), r.w - S(6), S(4) }, u.health,
                u.health > 0.6f ? kReadout : (u.health > 0.3f ? pal::kWarning : pal::kDanger),
                Color(10, 16, 20));

            if (hover) {
                tipUnit_ = u.defId;
                if (mine && !drag_.active) gfx_.requestCursor(CursorKind::Hand);
                if (mine && input_.mouseDown && !drag_.armed && !drag_.active) {
                    drag_.armed = true;
                    drag_.fromSlot = fromSlot;
                    drag_.fromSurface = fromSurface;
                    drag_.startPos = Vec2(mx, my);
                    drag_.units.clear();
                    // Dragging a selected unit takes the whole selection with it.
                    if (selected) {
                        for (Id id : selectedUnits_) {
                            const UnitInstance& s = game_.unit(id);
                            if (s.alive && s.owner == me && s.planet == u.planet && s.landed == u.landed) {
                                drag_.units.push_back(id);
                            }
                        }
                    }
                    if (drag_.units.empty()) drag_.units.push_back(unitId);
                }
                if (mine && input_.mouseClicked && !drag_.active) return true;
            }
            return false;
        }

        void App::resolveUnitDrop() {
            if (!drag_.active || drag_.units.empty()) {
                drag_ = UnitDrag{};
                return;
            }
            Faction me = game_.playerFaction();
            Vec2 mouse(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
            const DropTarget* hit = nullptr;
            for (const DropTarget& t : dropTargets_) {
                if (t.rect.contains(mouse.x, mouse.y)) hit = &t;
            }
            if (hit == nullptr) {
                drag_ = UnitDrag{};
                return;
            }

            Id planet = game_.unit(drag_.units.front()).planet;
            if (planet == kInvalid) {
                drag_ = UnitDrag{};
                return;
            }

            if (hit->surface) {
                // Onto the surface: land the troops (which may start a ground battle).
                std::vector<Id> troops;
                for (Id id : drag_.units) {
                    const UnitInstance& u = game_.unit(id);
                    if (u.def().domain() == Domain::Ground && !u.landed) troops.push_back(id);
                }
                bool allAlreadyDown = true;
                for (Id id : drag_.units) {
                    const UnitInstance& u = game_.unit(id);
                    if (u.def().domain() != Domain::Ground || !u.landed) allAlreadyDown = false;
                }
                if (troops.empty() && allAlreadyDown) {
                    // Shuffling troops around the surface: nothing to do.
                }
                else if (troops.empty()) {
                    setStatus("Only ground forces can be landed on a world");
                }
                else {
                    setStatus(game_.invade(planet, troops, me).message);
                    selectedUnits_.clear();
                }
            }
            else if (drag_.fromSurface) {
                // Off the surface: back aboard the transports, into the chosen slot.
                OrderResult r = game_.liftToOrbit(drag_.units, me);
                setStatus(r.message);
                if (r.ok) {
                    for (Id id : drag_.units) game_.setUnitSlot(id, hit->slot, me);
                }
            }
            else {
                int moved = 0;
                for (Id id : drag_.units) {
                    if (game_.setUnitSlot(id, hit->slot, me).ok) ++moved;
                }
                if (moved > 0) {
                    setStatus(std::to_string(moved) + " unit(s) moved to slot " + std::to_string(hit->slot + 1));
                }
            }
            drag_ = UnitDrag{};
        }

        void App::drawDraggedUnits() {
            if (!drag_.active || drag_.units.empty()) return;
            gfx_.requestCursor(CursorKind::Move);
            float mx = static_cast<float>(input_.mouseX);
            float my = static_cast<float>(input_.mouseY);
            int shown = 0;
            for (Id id : drag_.units) {
                if (shown >= 4) break;
                const UnitInstance& u = game_.unit(id);
                if (!u.alive) continue;
                Rect r{ mx - S(24) + static_cast<float>(shown) * S(8), my - S(20) + static_cast<float>(shown) * S(6),
                       S(48), S(40) };
                gfx_.rect(r, Color(30, 46, 60, 220));
                gfx_.rectOutline(r, pal::kAccent);
                drawUnitGlyphShared(gfx_, r.inset(S(5)), u.def().unitClass, pal::faction(u.owner));
                ++shown;
            }
            if (drag_.units.size() > 4) {
                gfx_.text(mx + S(24), my + S(16), "+" + std::to_string(drag_.units.size() - 4), pal::kAccent, F(2));
            }
        }

        // ---------------------------------------------------------------------------
        // The view itself
        // ---------------------------------------------------------------------------
        void App::drawPlanetView() {
            if (planetViewT_ <= 0.0f || planetViewTarget_ == kInvalid) return;
            const float alphaF = smoothstep(std::max(0.0f, (planetViewT_ - 0.35f) / 0.65f));
            const int alpha = static_cast<int>(alphaF * 255.0f);
            if (alpha <= 4) return;

            const float w = static_cast<float>(gfx_.width());
            const float barH = static_cast<float>(gfx_.height()) - mapViewport().h;
            const float h = static_cast<float>(gfx_.height()) - barH;
            const Id pid = planetViewTarget_;
            const PlanetState& p = game_.planet(pid);
            const PlanetDef& pd = p.def();
            Faction me = game_.playerFaction();
            Color c = pal::faction(p.owner);
            bool mine = p.owner == me;

            gfx_.rect(Rect{ 0, 0, w, h }, Color(4, 7, 12, static_cast<int>(alphaF * 246.0f)));

            auto fade = [&](Color col) { return col.withAlpha(static_cast<int>(col.a * alphaF)); };

            // --- title ---
            gfx_.textCentred(w * 0.5f, S(10), pd.name, fade(c), F(5));
            gfx_.textCentred(w * 0.5f, S(10) + lineH(5) + S(4),
                pd.region + "   -   " + factionShortName(p.owner) +
                (pd.spaceOnly ? "   -   SPACE-ONLY SYSTEM" : ""),
                fade(pal::kTextDim), F(2));

            const float infoW = std::min(S(390.0f), w * 0.27f);
            const float leftW = w - infoW - S(48);
            const float leftCx = S(24) + leftW * 0.5f;
            const float top = S(10) + lineH(5) + lineH(2) + S(12);

            // ------------------------------------------------------------------
            // Three orbital holding slots
            // ------------------------------------------------------------------
            const float slotGap = S(12);
            const float slotW = (leftW - slotGap * 2.0f) / 3.0f;
            const float slotH = std::min(h * 0.21f, S(180.0f));
            const float cellW = S(58);
            const float cellH = S(50);

            for (int slot = 0; slot < kOrbitSlots; ++slot) {
                Rect box{ S(24) + static_cast<float>(slot) * (slotW + slotGap), top, slotW, slotH };
                std::vector<Id> units = game_.unitsInSlot(pid, mine ? me : p.owner, slot);
                bool hot = drag_.active && box.contains(static_cast<float>(input_.mouseX),
                    static_cast<float>(input_.mouseY));
                gfx_.rect(box, fade(hot ? kSlotFillHot : kSlotFill));
                gfx_.rectOutline(box, fade(hot ? pal::kAccent : c.scaled(0.8f)), 2);
                if (mine) addDropTarget(box, slot, false);

                gfx_.text(box.x + S(10), box.y + S(7), "ORBIT SLOT " + std::to_string(slot + 1), fade(c), F(2));
                gfx_.textRight(box.right() - S(10), box.y + S(9), std::to_string(units.size()) + " UNITS",
                    fade(pal::kTextDim), F(1));
                gfx_.line(box.x + S(8), box.y + S(28), box.right() - S(8), box.y + S(28), fade(c.withAlpha(110)));

                // Grid of unit tiles.
                int perRow = std::max(1, static_cast<int>((box.w - S(12)) / (cellW + S(4))));
                int maxRows = std::max(1, static_cast<int>((box.h - S(38)) / (cellH + S(4))));
                int capacity = perRow * maxRows;
                for (size_t i = 0; i < units.size(); ++i) {
                    if (static_cast<int>(i) >= capacity) {
                        gfx_.text(box.x + S(10), box.bottom() - S(14),
                            "+" + std::to_string(units.size() - static_cast<size_t>(capacity)) + " MORE",
                            fade(pal::kWarning), F(1));
                        break;
                    }
                    int row = static_cast<int>(i) / perRow;
                    int col = static_cast<int>(i) % perRow;
                    Rect cell{ box.x + S(8) + static_cast<float>(col) * (cellW + S(4)),
                              box.y + S(34) + static_cast<float>(row) * (cellH + S(4)), cellW, cellH };
                    if (drawUnitCell(cell, units[i], slot, false, false)) toggleUnitSelection(units[i]);
                }
                if (units.empty()) {
                    gfx_.textCentred(box.x + box.w * 0.5f, box.y + box.h * 0.5f - lineH(1) * 0.5f,
                        mine ? "DRAG UNITS HERE" : "EMPTY", fade(Color(80, 92, 100)), F(1));
                }
            }

            // ------------------------------------------------------------------
            // The world, with the surface slot set into it
            // ------------------------------------------------------------------
            float globeTop = top + slotH + S(20);
            float globeR = std::max(S(80.0f), std::min((h - globeTop - S(176)) * 0.5f, leftW * 0.26f));
            Vec2 globe{ leftCx, globeTop + globeR + S(8) };

            if (pd.spaceOnly) {
                gfx_.triangle(Vec2(globe.x, globe.y - globeR), Vec2(globe.x + globeR, globe.y),
                    Vec2(globe.x - globeR, globe.y), fade(c.scaled(0.8f)));
                gfx_.triangle(Vec2(globe.x, globe.y + globeR), Vec2(globe.x + globeR, globe.y),
                    Vec2(globe.x - globeR, globe.y), fade(c.scaled(0.55f)));
                gfx_.textCentred(globe.x, globe.y + globeR + S(14), "NO SURFACE - HOLD ORBIT TO TAKE THIS SYSTEM",
                    fade(pal::kWarning), F(1));
            }
            else {
                gfx_.circle(globe.x, globe.y, globeR, fade(c.scaled(0.26f)));
                gfx_.circle(globe.x - globeR * 0.28f, globe.y - globeR * 0.28f, globeR * 0.55f,
                    fade(c.scaled(0.44f)));
                gfx_.circleOutline(globe.x, globe.y, globeR, fade(c));

                // The surface slot: ten cells laid out on the planet itself.
                std::vector<Id> troops = game_.unitsAt(pid, mine ? me : p.owner, Domain::Ground, true);
                const int cols = 5;
                const int rows = 2;
                float gw = globeR * 1.5f;
                float gh = gw * 0.42f;
                Rect grid{ globe.x - gw * 0.5f, globe.y - gh * 0.35f, gw, gh };
                bool hot = drag_.active &&
                    grid.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY));
                gfx_.rect(grid, fade(hot ? Color(30, 58, 40, 210) : Color(12, 22, 18, 190)));
                gfx_.rectOutline(grid, fade(hot ? pal::kAccent : kConsoleEdge), 2);
                gfx_.text(grid.x, grid.y - lineH(1) - S(4), "SURFACE  " + std::to_string(troops.size()) + " / " +
                    std::to_string(kGroundSlotCapacity),
                    fade(kReadout), F(1));
                if (mine || p.owner != me) addDropTarget(grid, -1, true);

                float cw = (grid.w - S(6)) / static_cast<float>(cols);
                float ch = (grid.h - S(6)) / static_cast<float>(rows);
                for (int i = 0; i < cols * rows; ++i) {
                    Rect cell{ grid.x + S(3) + static_cast<float>(i % cols) * cw,
                              grid.y + S(3) + static_cast<float>(i / cols) * ch, cw - S(2), ch - S(2) };
                    if (i < static_cast<int>(troops.size())) {
                        if (drawUnitCell(cell, troops[static_cast<size_t>(i)], -1, true, true)) {
                            toggleUnitSelection(troops[static_cast<size_t>(i)]);
                        }
                    }
                    else {
                        gfx_.rect(cell, fade(Color(10, 18, 16, 170)));
                        gfx_.rectOutline(cell, fade(Color(44, 60, 50)));
                    }
                }
                if (troops.size() > static_cast<size_t>(cols * rows)) {
                    gfx_.text(grid.right() + S(6), grid.y,
                        "+" + std::to_string(troops.size() - static_cast<size_t>(cols * rows)),
                        fade(pal::kWarning), F(1));
                }
            }

            // ------------------------------------------------------------------
            // Structure sockets
            // ------------------------------------------------------------------
            auto drawSlots = [&](const char* label, Domain domain, float y) {
                int cap = game_.buildSlotCapacity(pid, domain);
                if (cap <= 0) return;
                std::vector<Id> built;
                for (Id bid : p.buildings) {
                    const BuildingInstance& b = game_.buildingInst(bid);
                    if (b.alive && b.def().domain == domain) built.push_back(bid);
                }
                float sw = S(46);
                float sh = S(38);
                float totalW = static_cast<float>(cap) * (sw + S(5));
                float x = leftCx - totalW * 0.5f;
                gfx_.textRight(x - S(10), y + sh * 0.5f - lineH(1) * 0.5f, label, fade(pal::kTextDim), F(1));
                for (int i = 0; i < cap; ++i) {
                    Rect s{ x + static_cast<float>(i) * (sw + S(5)), y, sw, sh };
                    bool filled = i < static_cast<int>(built.size());
                    gfx_.rect(s, fade(filled ? Color(24, 34, 30, 235) : Color(13, 16, 20, 210)));
                    gfx_.rectOutline(s, fade(filled ? kConsoleEdge : Color(56, 44, 44)));
                    if (filled) {
                        const BuildingDef& bd = game_.buildingInst(built[static_cast<size_t>(i)]).def();
                        drawStructureGlyphShared(gfx_, s.inset(S(6)), bd, fade(c));
                        if (s.contains(static_cast<float>(input_.mouseX), static_cast<float>(input_.mouseY))) {
                            tipBuilding_ = bd.id;
                        }
                    }
                    else {
                        gfx_.textCentred(s.x + s.w * 0.5f, s.y + s.h * 0.5f - lineH(1) * 0.5f, "-",
                            fade(Color(80, 68, 68)), F(1));
                    }
                }
                };
            float structY = std::min(globe.y + globeR + S(20), h - S(140));
            drawSlots("ORBIT", Domain::Space, structY);
            if (!pd.spaceOnly) drawSlots("SURFACE", Domain::Ground, structY + S(44));

            // ------------------------------------------------------------------
            // Planet dossier
            // ------------------------------------------------------------------
            Rect info{ w - infoW - S(24), top, infoW, h - top - S(76) };
            gfx_.rect(info, fade(Color(10, 14, 18, 246)));
            gfx_.rectOutline(info, fade(pal::kBorderBright));
            float y = info.y + S(10);
            gfx_.text(info.x + S(12), y, pd.name, fade(c), F(3));
            y += lineH(3) + S(8);
            y += wrappedText(gfx_, Rect{ info.x + S(12), y, info.w - S(24), S(110) }, pd.description,
                fade(pal::kTextDim), F(1)) +
                S(8);
            for (Trait t : pd.traits) {
                gfx_.text(info.x + S(12), y, std::string(traitName(t)) + ":", fade(pal::kAccent), F(1));
                y += lineH(1) + S(2);
                gfx_.text(info.x + S(24), y, traitDescription(t), fade(pal::kTextDim), F(1));
                y += lineH(1) + S(5);
            }
            y += S(6);
            gfx_.line(info.x + S(12), y, info.right() - S(12), y, fade(pal::kBorder));
            y += S(8);
            auto infoRow = [&](const std::string& label, const std::string& value, Color vc) {
                gfx_.text(info.x + S(12), y, label, fade(kReadoutDim), F(1));
                gfx_.textRight(info.right() - S(12), y, value, fade(vc), F(1));
                y += lineH(1) + S(5);
                };
            infoRow("WEEKLY INCOME", credits(game_.planetIncome(pid)), kReadout);
            infoRow("ORBIT UNITS",
                std::to_string(game_.usedUnitSlots(pid, p.owner, Domain::Space)) + " / " +
                std::to_string(game_.unitSlotCapacity(pid, Domain::Space)) + " BUILT",
                pal::kText);
            infoRow("SURFACE UNITS",
                pd.spaceOnly ? "none"
                : std::to_string(game_.unitsAt(pid, p.owner, Domain::Ground, true).size()) +
                " / " + std::to_string(kGroundSlotCapacity),
                pal::kText);
            infoRow("ORBITAL STRUCTURES",
                std::to_string(game_.usedBuildSlots(pid, Domain::Space)) + " / " +
                std::to_string(game_.buildSlotCapacity(pid, Domain::Space)),
                pal::kText);
            infoRow("SURFACE STRUCTURES",
                pd.spaceOnly ? "none"
                : std::to_string(game_.usedBuildSlots(pid, Domain::Ground)) + " / " +
                std::to_string(game_.buildSlotCapacity(pid, Domain::Ground)),
                pal::kText);
            infoRow("SHIPYARD TIER", std::to_string(game_.bestProductionTier(pid, p.owner, Domain::Space)),
                pal::kText);
            infoRow("GROUND FACILITY TIER", std::to_string(game_.bestProductionTier(pid, p.owner, Domain::Ground)),
                pal::kText);
            if (game_.isContested(pid)) infoRow("STATUS", "CONTESTED", pal::kDanger);

            // Enemy or neutral forces present get their own short list.
            for (int fi = 0; fi < kFactionCount; ++fi) {
                Faction f = factionFromIndex(fi);
                if (f == me) continue;
                std::vector<Id> theirs = game_.allUnitsAt(pid, f);
                if (theirs.empty()) continue;
                y += S(4);
                gfx_.text(info.x + S(12), y, std::string(factionShortName(f)) + " FORCES PRESENT",
                    fade(pal::faction(f)), F(1));
                y += lineH(1) + S(4);
                std::vector<std::pair<Id, int>> groups;
                for (Id id : theirs) {
                    Id defId = game_.unit(id).defId;
                    bool found = false;
                    for (auto& kv : groups) {
                        if (kv.first == defId) {
                            ++kv.second;
                            found = true;
                        }
                    }
                    if (!found) groups.push_back({ defId, 1 });
                }
                for (const auto& kv : groups) {
                    if (y > info.bottom() - S(16)) break;
                    gfx_.text(info.x + S(24), y,
                        std::to_string(kv.second) + "x " + db().unit(kv.first).name.substr(0, 24),
                        fade(pal::kTextDim), F(1));
                    y += lineH(1) + S(3);
                }
            }

            // ------------------------------------------------------------------
            // Footer
            // ------------------------------------------------------------------
            Rect back{ info.x + info.w * 0.5f - S(110), info.bottom() + S(12), S(220), S(34) };
            ButtonStyle st;
            st.textScale = F(2);
            if (alphaF > 0.9f && button(gfx_, input_, back, "BACK TO THE MAP", true, st)) leavePlanetView();
            gfx_.textCentred(leftCx, h - S(18),
                "Drag units between the orbit slots; drag troops onto the surface to land them.  "
                "Scroll out, TAB or ESC returns to the map.",
                fade(pal::kTextDim), F(1));
        }

    }  // namespace ui
}  // namespace gc