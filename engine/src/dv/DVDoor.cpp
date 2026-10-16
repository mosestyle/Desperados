#include "dv/DVDoor.h"
#include "dv/DVPatch.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>

#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVScript.h"
#include "dv/DVSector.h"
#include "sb/SBFile.h"

static float Dist(const SBGeoPoint2D& a, const SBGeoPoint2D& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static DVSectorMotionArea* AsArea(DVSector* s) {
    return s && (s->type & 3) == 3 ? static_cast<DVSectorMotionArea*>(s) : nullptr;
}

// ---- DVDoor ---------------------------------------------------------------------------------

int DVDoor::LoadFromFile(SBFile& f, DVFastFindGrid& grid, DVSectorMotionArea* building) {
    type = f.U8();
    enabled = f.U8() != 0;
    pcLocked = f.U8() != 0;
    docPickable = f.U8() != 0;
    villainLocked = f.U8() != 0;
    civilianLocked = f.U8() != 0;
    uint8_t patchBytes[4];
    for (uint8_t& b4 : patchBytes) b4 = f.U8();
    // the picture one clicks on (a DVSectorPlane, 0x1040)
    auto* pl = new DVSector(SECTOR_DOOR_PLANE | 0x40);
    int n = pl->LoadFromFile(f);
    uint16_t count = f.U16();
    int16_t x = f.S16(), y = f.S16();
    uint16_t areaA = f.U16(), layA = f.U16();
    if (count != 3) SBError(true, "DVDoor.cpp", 0x4bd, "A door is not valid (%d points) (%d,%d) !", count, x, y);
    pA = SBGeoPoint2D(x, y);
    layerA = layA;
    a = AsArea(grid.Sector(areaA));
    uint16_t planeLayer = layA;
    x = f.S16();
    y = f.S16();
    f.U16();
    f.U16();
    mid = SBGeoPoint2D(x, y);
    x = f.S16();
    y = f.S16();
    uint16_t areaB = f.U16(), layB = f.U16();
    if (layA < layB && layB != grid.layerCount) planeLayer = layB;
    pB = SBGeoPoint2D(x, y);
    if (!building) {
        layerB = layB;
        b = AsArea(grid.Sector(areaB));
    } else {
        layerB = (uint16_t)(grid.layerCount - 1);
        b = building;
    }
    if (a) a->AddDoor(this, true);
    if (b) b->AddDoor(this, false);
    if (pl->polygon.pts.empty()) {
        delete pl;
    } else {
        pl->owner = this;
        plane = pl;
        grid.AdoptSector(pl);
        grid.AddLayerSector(pl, planeLayer);
    }
    switch (type) {
    case DOOR_BUILDING_VILLAINS:
        act[0] = 3, act[1] = 0xc2, act[2] = 0xc, act[3] = 3;
        break;
    default:
        act[0] = act[1] = act[2] = act[3] = 3;
        break;
    }
    uint16_t p = f.U16();
    if (p == 0xffff) return n + 0x26;
    bool whenPassing = f.U8() != 0;  // the patch is applied when one passes
    bool locks = f.U8() != 0;        // ... and changes the door's locks
    DVPatch* pt = grid.Patch(p);
    if (pt && locks) {
        pt->door = this;
        for (int i = 0; i < 4; ++i) pt->doorFlags[i] = patchBytes[i];
    }
    patch = whenPassing ? pt : nullptr;
    return n + 0x28;
}

bool DVDoor::IsActorAutorized(bool enter, const DVElementActor* who) const {
    if (!enabled) return false;
    if (!who) return true;
    if (type > 5) return true;
    bool hero = who->IsHero();
    bool villain = who->kind == KIND_VILLAIN;
    bool civilian = who->kind == KIND_CIVILIAN;
    auto heroOk = [&] { return !pcLocked || (who->kind == KIND_DOC && docPickable); };
    auto full = [&] {
        auto* house = dynamic_cast<DVSectorBuilding*>(b);
        return enter && house && house->capacity <= house->occupants.size();
    };
    switch (type) {
    case DOOR_BUILDING:
        if (villain || civilian) {
            if (full()) return false;
            return villain ? !villainLocked : !civilianLocked;
        }
        return hero && heroOk();
    case DOOR_BUILDING_VILLAINS:
        if (villain || civilian) {
            if (full() || !villain) return false;
            return !villainLocked;
        }
        return hero && heroOk();
    case DOOR_LIFT_TOP:
    case DOOR_LIFT_BOTTOM: {
        uint32_t k = b ? b->liftKind : 0;
        if (k == 2) return hero || villain;              // a ladder: not the townsfolk
        if (k == 4) return who->kind == KIND_COOPER;     // a wall: Cooper climbs
        if (k == 1) return !(who->kind >= KIND_ANIMAL1 && who->kind <= KIND_ANIMAL1 + 5);
        return true;
    }
    default:
        if (villain) return !villainLocked;
        if (civilian || who->kind == KIND_HORSE) return !civilianLocked;
        if (hero) return heroOk();
        return false;
    }
}

static uint32_t DoorAction(const DVDoor* d, uint32_t v, uint32_t anim, bool crawlToo) {
    if (d->type == DOOR_GATE || d->type == DOOR_DEFAULT) {
        if (anim == 9 || anim == 0xc5) return 9;
    }
    (void)crawlToo;
    if (anim == 5 || anim == 0xc4 || anim == 199) {
        if (v - 3u <= 1) return 5;
    }
    return v;
}
uint32_t DVDoor::Action1(bool enter, uint32_t anim) const { return DoorAction(this, enter ? act[0] : act[2], anim, true); }
uint32_t DVDoor::Action2(bool enter, uint32_t anim) const { return DoorAction(this, enter ? act[1] : act[3], anim, false); }

// ---- DVSectorBuilding -------------------------------------------------------------------------

void DVSectorBuilding::Enter(DVElementActor* who) {
    if (std::find(occupants.begin(), occupants.end(), who) == occupants.end()) occupants.push_back(who);
}

void DVSectorBuilding::Leave(DVElementActor* who) {
    occupants.erase(std::remove(occupants.begin(), occupants.end(), who), occupants.end());
}

// ---- the grid's part ------------------------------------------------------------------------

void DVFastFindGrid::AddLayerSector(DVSector* s, uint16_t layer) {
    if (layerSectors.size() < layerCount) layerSectors.resize(layerCount);
    if (layer < layerSectors.size()) layerSectors[layer].push_back(s);
}

void DVFastFindGrid::AdoptSector(DVSector* s) {
    ownSectors.emplace_back(s);
    AddSector(s);
}

// DVSectorLift::LoadFromFile (0x0835b640): the lift's two doors (the lower one first), the
// picture one clicks on, the doors' points
static int LoadLift(SBFile& f, DVFastFindGrid& grid, DVSectorMotionArea* lift, std::vector<std::unique_ptr<DVDoor>>& doors) {
    auto* d0 = new DVDoor();
    auto* d1 = new DVDoor();
    doors.emplace_back(d0);
    doors.emplace_back(d1);
    d0->type = DOOR_LIFT_BOTTOM;
    d1->type = DOOR_LIFT_TOP;
    uint8_t kind = f.U8();
    auto set = [](DVDoor* d, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
        d->act[0] = a0, d->act[1] = a1, d->act[2] = a2, d->act[3] = a3;
    };
    switch (kind) {
    case 0:
        lift->liftKind = 0;
        set(d0, 3, 3, 3, 3);
        set(d1, 3, 3, 3, 3);
        break;
    case 1:
        lift->liftKind = 1;
        set(d0, 3, 4, 4, 3);
        set(d1, 3, 4, 4, 3);
        break;
    case 2:
        lift->liftKind = 2;
        set(d0, 3, 0xb, 0xe, 3);
        set(d1, 3, 0xc2, 0xc, 3);
        break;
    case 3:
        lift->liftKind = 4;
        set(d0, 3, 0x2d, 0x30, 3);
        set(d1, 3, 0x2f, 0x2e, 3);
        break;
    default:
        break;
    }
    uint16_t last = (uint16_t)(grid.layerCount - 1);
    uint16_t area = f.U16(), layer0 = f.U16();
    d0->layerA = layer0;
    d0->layerB = last;
    d0->a = AsArea(grid.Sector(area));
    d0->b = lift;
    lift->AddDoor(d0, false);
    if (d0->a) d0->a->AddDoor(d0, true);
    area = f.U16();
    uint16_t layer1 = f.U16();
    d1->layerA = layer1;
    d1->layerB = last;
    d1->a = AsArea(grid.Sector(area));
    d1->b = lift;
    lift->AddDoor(d1, false);
    if (d1->a) d1->a->AddDoor(d1, true);
    auto* pl = new DVSector(SECTOR_LIFT_PLANE);
    int n = pl->LoadFromFile(f);
    pl->type = SECTOR_LIFT_PLANE | 0x40;
    pl->owner = lift;
    grid.AdoptSector(pl);
    grid.AddLayerSector(pl, layer0);
    if (layer1 != layer0) grid.AddLayerSector(pl, layer1);
    auto pt = [&f] {
        int16_t x = f.S16(), y = f.S16();
        return SBGeoPoint2D(x, y);
    };
    d0->pA = pt();
    d0->mid = pt();
    d0->pB = pt();
    d1->pA = pt();
    d1->mid = pt();
    d1->pB = pt();
    f.U16();  // +0xce
    return n + 0x23;
}

int DVFastFindGrid::LoadLiftsFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 2)
        SBError(true, "DVFastFindGrid.cpp", 0x840, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk lifts", f.version, 2);
    uint16_t count = f.U16();
    int n = 6;
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t id = f.U16();
        auto* lift = dynamic_cast<DVSectorMotionArea*>(Sector(id));
        if (!lift || !lift->IsLift()) {
            SBError(true, "DVFastFindGrid.cpp", 0x84e, "Illegal lift sector : %d !\n", id);
            return -1;
        }
        n += 2 + LoadLift(f, *this, lift, doors);
    }
    return n;
}

// DVSectorBuilding::LoadFromFile (0x0835c030): who is inside (indices in the characters), its
// doors; the ones inside are off the map, at the last door's inner point
int DVFastFindGrid::LoadBuildingsFromFile(SBFile& f, const std::vector<DVElementActor*>& actors) {
    f.version = f.U32();
    if (f.version != 4)
        SBError(true, "DVFastFindGrid.cpp", 0x874, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk buildings", f.version, 4);
    uint16_t count = f.U16();
    int n = 8;
    for (uint16_t i = 0; i < count; ++i) {
        auto* house = new DVSectorBuilding();
        AdoptSector(house);
        buildings.push_back(house);
        f.U16();  // the capacity (the original sets 100)
        uint16_t inside = f.U16();
        std::vector<DVElementActor*> who;
        for (uint16_t k = 0; k < inside; ++k) {
            uint16_t idx = f.U16();
            if (idx < actors.size()) who.push_back(actors[idx]);
        }
        uint16_t nd = f.U16();
        int size = 6 + 2 * inside;
        DVDoor* lastDoor = nullptr;
        for (uint16_t k = 0; k < nd; ++k) {
            auto* d = new DVDoor();
            doors.emplace_back(d);
            size += d->LoadFromFile(f, *this, house);
            lastDoor = d;
        }
        for (DVElementActor* a : who) {
            a->active = false;
            DVPositionInterface& pos = a->sprite->pos;
            pos.layer = (uint16_t)(layerCount - 1);
            pos.sector = house;
            if (lastDoor) pos.SetPositionMap(lastDoor->pB);
            pos.SetObstacle(nullptr);
            pos.ComputePositionAll();
            house->occupants.push_back(a);
        }
        n += size;
    }
    uint16_t gates = f.U16();
    for (uint16_t k = 0; k < gates; ++k) {
        auto* d = new DVDoor();
        doors.emplace_back(d);
        n += d->LoadFromFile(f, *this, nullptr);
        if (d->type != DOOR_DEFAULT && d->type != DOOR_GATE)
            SBError(true, "DVFastFindGrid.cpp", 0x88e,
                    "VERBOTEN : Non-building & non-lift doors must be of type DEFAULT or GATE (%d,%d) !", (int)d->mid.x,
                    (int)d->mid.y);
    }
    return n;
}

DVDoor* DVFastFindGrid::NearestDoor(const SBGeoPoint2D& p, float maxDist) const {
    DVDoor* best = nullptr;
    float bestD = maxDist * maxDist;
    for (const auto& d : doors) {
        float dx = d->mid.x - p.x, dy = d->mid.y - p.y;
        float q = dx * dx + dy * dy;
        if (q < bestD) {
            bestD = q;
            best = d.get();
        }
    }
    return best;
}

// GetSectorScreen / GetSector (0x082acda0 / 0x082ac680): from the top layer down, the first layer
// with something under the point decides: a door's picture, a lift (its area or its picture), a
// walkable area; anything else (a house's body) is not walkable
bool DVFastFindGrid::GetSectorScreen(const SBGeoPoint2D& p, DVSectorMotionArea*& area, uint16_t& layer, DVDoor*& door) const {
    area = nullptr;
    door = nullptr;
    for (int l = (int)layerSectors.size() - 1; l >= 0; --l) {
        DVSector* doorPlane = nullptr;
        DVSector* liftArea = nullptr;
        DVSector* walk = nullptr;
        DVSector* liftPlane = nullptr;
        DVSector* other = nullptr;
        for (DVSector* s : layerSectors[(size_t)l]) {
            if (!s->flag30 || !s->box.IsInside_p(p) || !s->polygon.IsInside_p(p)) continue;
            if (s->type & SECTOR_DOOR_PLANE) {
                if (!doorPlane) doorPlane = s;
            } else if (s->type & SECTOR_LIFT) {
                if (!liftArea) liftArea = s;
            } else if ((s->type & 3) == 3) {
                if (!walk) walk = s;
            } else if (s->type & SECTOR_LIFT_PLANE) {
                if (!liftPlane) liftPlane = s;
            } else if (!other) {
                other = s;
            }
        }
        if (doorPlane) {
            door = static_cast<DVDoor*>(doorPlane->owner);
            area = door ? door->b : nullptr;
            layer = door ? door->layerB : 0;
            return door != nullptr;
        }
        if (liftArea) {
            area = static_cast<DVSectorMotionArea*>(liftArea);
            layer = (uint16_t)l;
            return true;
        }
        if (walk) {
            area = static_cast<DVSectorMotionArea*>(walk);
            layer = (uint16_t)l;
            return true;
        }
        if (liftPlane) {
            area = static_cast<DVSectorMotionArea*>(liftPlane->owner);
            layer = (uint16_t)(layerCount - 1);
            return area != nullptr;
        }
        if (other) return false;
    }
    return false;
}

// FindPathDoors / FindPathDoorsNodes (0x082c0cb0 / 0x082c13f0): A* over the doors. A door is
// passed one way or the other; its cost is the walk to it from where one is, its estimate the
// distance from its far end to the goal. Inside a house the walk counts 100.
bool DVFastFindGrid::FindPathDoors(const DVElementActor* who, const SBGeoPoint2D& from, DVSectorMotionArea* fromArea,
                                   const SBGeoPoint2D& to, DVSectorMotionArea* toArea,
                                   std::vector<std::pair<DVDoor*, bool>>& out) const {
    out.clear();
    if (!fromArea || !toArea) return false;
    struct State {
        float g;
        DVDoor* door;
        bool enter;
    };
    std::map<std::pair<DVDoor*, bool>, std::pair<float, std::pair<DVDoor*, bool>>> best;  // g, parent
    using Q = std::pair<float, std::pair<DVDoor*, bool>>;
    std::priority_queue<Q, std::vector<Q>, std::greater<Q>> open;
    auto walk = [](DVSectorMotionArea* area, const SBGeoPoint2D& a, const SBGeoPoint2D& b) {
        return area->IsBuilding() ? 100.0f : Dist(a, b);
    };
    auto push = [&](DVDoor* d, bool enter, float g, std::pair<DVDoor*, bool> parent) {
        auto key = std::make_pair(d, enter);
        auto it = best.find(key);
        if (it != best.end() && it->second.first <= g) return;
        best[key] = {g, parent};
        DVSectorMotionArea* next = d->To(enter);
        float h = next && !next->IsBuilding() ? Dist(d->Exit(enter), to) : 0.0f;
        open.push({g + h, key});
    };
    for (size_t i = 0; i < fromArea->doors.size(); ++i) {
        DVDoor* d = fromArea->doors[i];
        bool enter = fromArea->doorSides[i];
        if (!d->IsActorAutorized(enter, who)) continue;
        push(d, enter, walk(fromArea, from, d->Entry(enter)), {nullptr, false});
    }
    std::pair<DVDoor*, bool> found{nullptr, false};
    while (!open.empty()) {
        Q q = open.top();
        open.pop();
        DVDoor* d = q.second.first;
        bool enter = q.second.second;
        float g = best[q.second].first;
        DVSectorMotionArea* here = d->To(enter);
        if (!here) continue;
        if (here == toArea) {
            found = q.second;
            break;
        }
        SBGeoPoint2D at = d->Exit(enter);
        for (size_t i = 0; i < here->doors.size(); ++i) {
            DVDoor* d2 = here->doors[i];
            if (d2 == d) continue;
            bool enter2 = here->doorSides[i];
            if (!d2->IsActorAutorized(enter2, who)) continue;
            push(d2, enter2, g + walk(here, at, d2->Entry(enter2)) + 1.0f, q.second);
        }
    }
    if (!found.first) return false;
    for (auto k = found; k.first; k = best[k].second) out.push_back(k);
    std::reverse(out.begin(), out.end());
    return true;
}

// ---- the actors' side -----------------------------------------------------------------------

DVSectorMotionArea* DVElementActor::Area() const {
    return sprite ? AsArea(sprite->pos.sector) : nullptr;
}

DVSectorBuilding* DVElementActor::Building() const {
    DVSectorMotionArea* a = Area();
    return a && a->IsBuilding() ? static_cast<DVSectorBuilding*>(a) : nullptr;
}

// DVElementActor::PassDoor (0x0820f7e0): he is now on the door's other side; into a house he
// goes off the map, out of one he comes back
void DVElementActor::PassDoor(DVDoor* d, bool enter) {
    DVPositionInterface& pos = sprite->pos;
    if (DVSectorBuilding* house = Building()) {
        house->Leave(this);
        active = true;
    }
    pos.layer = d->ToLayer(enter);
    pos.sector = d->To(enter);
    if (DVSectorBuilding* house = Building()) {
        active = false;
        house->Enter(this);
        pos.SetObstacle(nullptr);
    } else {
        Teleport(pos.posMap, pos.layer, pos.sector);
    }
    if (d->patch) d->patch->Apply();  // (closing behind him)
}

// the lift's animation for a move from `from` to `to` (DVElementActor::Translate 0x17 in a lift:
// DVSectorLift::GetActionUpwards / GetActionDownwards): down = toward its lower door
uint32_t DVLiftAnimation(const DVSectorMotionArea* lift, const SBGeoPoint2D& from, const SBGeoPoint2D& to, uint32_t anim) {
    bool down = false;
    if (lift->doors.size() >= 2) {
        SBGeoVector2D axis = lift->doors[1]->pB - lift->doors[0]->pB;  // bottom -> top
        SBGeoVector2D mv = from - to;
        down = axis.x * mv.x + axis.y * mv.y >= 0.0f;
    }
    switch (lift->liftKind) {
    case 0:
        return anim == 0xc4 ? 0xc4u : anim == 5 ? 5u : 3u;
    case 1:
        return anim == 0xc4 ? 0xc4u : anim == 5 ? 5u : 4u;
    case 2:
        return down ? 0xeu : 0xbu;
    case 4:
        return down ? 0x30u : 0x2du;
    default:
        return 4;
    }
}
