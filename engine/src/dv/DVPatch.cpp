// DVPatch: see DVPatch.h
#include "dv/DVPatch.h"

#include <algorithm>

#include "dv/DVDoor.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVMask.h"
#include "dv/DVSector.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"
#include "sb/SBPictureJpeg.h"

DVPatch::DVPatch() = default;

DVPatch::~DVPatch() {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (d && surface != 0xffffffff) d->DeleteSurface(surface);
}

int DVPatch::LoadFromFile(SBFile& f, DVFastFindGrid& grid) {
    int start = f.Tell();
    trigger = triggerInit = f.U8() != 0;
    hasFx = f.U8() != 0;
    if (hasFx) {
        fxOff = f.U8() != 0;
        fxChange = f.U8() != 0;
        fxOn = f.U8() != 0;
        uint16_t i = f.U16();
        DVEngine* e = DVEngine::mpEngine;
        const auto& els = e->Elements();
        fx = i < els.size() ? dynamic_cast<DVElementFX*>(els[i]) : nullptr;
        if (fx) {
            fx->patch = this;
            if (!fxOff) fx->active = false;
            else ShowFx(0x94, false);
        } else {
            SBError(false, "DVPatch.cpp", 0x1b4, "A patch's animation (element %u) is not a scenery animation", i);
        }
    }
    u4 = f.U8() != 0;
    layer = f.U16();
    u13e = f.U16();
    permanent = (f.U8() ^ 1) != 0;
    uint16_t x = f.U16(), y = f.U16(), w = f.U16(), h = f.U16();
    at = SBGeoPoint2D((float)(x << 6), (float)(y << 6));
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    uint16_t pw = (uint16_t)(w << 6), ph = (uint16_t)((h & 0x3ff) << 6);
    SBPictureJpeg pic;
    if (!pic.LoadFromStream(f)) SBError(true, "DVPatch.cpp", 0x1c2, "A patch's picture could not be read");
    if (d) {
        surface = d->CreateSurface(pw, ph);
        if (pic.GetWidth() == pw && pic.GetHeight() == ph) d->UploadPicture(surface, pic.Pixels().data(), pw, ph);
        else if (pic.GetWidth() && pic.GetHeight()) {
            // (a picture of another size: what fits)
            std::vector<uint16_t> px((size_t)pw * ph, 0);
            for (uint16_t yy = 0; yy < ph && yy < pic.GetHeight(); ++yy)
                for (uint16_t xx = 0; xx < pw && xx < pic.GetWidth(); ++xx)
                    px[(size_t)yy * pw + xx] = pic.Pixels()[(size_t)yy * pic.GetWidth() + xx];
            d->UploadPicture(surface, px.data(), pw, ph);
        }
    }
    // the masks (by layer) and sight obstacles that are there while off / while on
    for (int on = 0; on < 2; ++on) {
        uint16_t n = f.U16();
        for (uint16_t i = 0; i < n; ++i) {
            uint16_t l = f.U16(), k = f.U16();
            DVMask* m = grid.LayerMask(l, k);
            if (!m) continue;
            m->active = on == 0;
            (on ? masksOn : masksOff).push_back(m);
        }
    }
    for (int on = 0; on < 2; ++on) {
        uint16_t n = f.U16();
        for (uint16_t i = 0; i < n; ++i) {
            DVSightObstacle* o = grid.SightObstacle(f.U16());
            if (!o) continue;
            o->enabled = on == 0;
            (on ? obstaclesOn : obstaclesOff).push_back(o);
        }
    }
    // the click areas: a patch area and a plane (one per layer) while off, the same while on
    auto sector = [&](uint32_t type) {
        auto* s = new DVSector(type);
        s->LoadFromFile(f);
        if (s->polygon.pts.empty()) {
            delete s;
            return (DVSector*)nullptr;
        }
        s->owner = this;
        own.emplace_back(s);
        return s;
    };
    if (DVSector* s = sector(SECTOR_PATCH)) s->flag30 = true, sectorsOff.push_back(s);
    if (DVSector* s = sector(SECTOR_PATCH)) s->flag30 = false, sectorsOn.push_back(s);
    if (DVSector* s = sector(SECTOR_PLANE)) s->flag30 = true, sectorsOff.push_back(s);
    if (DVSector* s = sector(SECTOR_PLANE)) s->flag30 = false, sectorsOn.push_back(s);
    zone = sector(u4 ? 0x104u : 0x184u);
    if (zone) zone->flag30 = true;
    zone2 = sector(u4 ? 4u : 0x84u);
    if (zone2) zone2->flag30 = true;
    int16_t px = f.S16(), py = f.S16();
    point = SBGeoPoint2D((float)px, (float)py);
    // DVFastFindGrid::AddPatch: the areas on their layers (the planes on every layer)
    for (const auto* list : {&sectorsOff, &sectorsOn})
        for (DVSector* s : *list) {
            if ((s->type & SECTOR_PLANE) == SECTOR_PLANE)
                for (uint16_t l = 0; l < grid.layerCount; ++l) grid.AddLayerSector(s, l);
            else
                grid.AddLayerSector(s, layer);
        }
    for (auto& s : own) grid.AdoptSector(s.release());  // (the grid has them now)
    own.clear();
    return f.Tell() - start;
}

void DVPatch::ShowFx(uint32_t anim, bool fromEnd) {
    if (!fx) return;
    fx->active = true;
    DVSprite* s = fx->sprite.get();
    if (s->HasAnimation(anim)) s->row = (uint16_t)s->animRows[anim];
    s->animation = anim;
    s->entry = 0;
    s->ResetSpriteFrame(fromEnd);
}

void DVPatch::SwapDoors() {
    if (!door) return;
    uint8_t now[4] = {door->pcLocked, door->docPickable, door->villainLocked, door->civilianLocked};
    door->pcLocked = doorFlags[0] != 0;
    door->docPickable = doorFlags[1] != 0;
    door->villainLocked = doorFlags[2] != 0;
    door->civilianLocked = doorFlags[3] != 0;
    for (int i = 0; i < 4; ++i) doorFlags[i] = now[i];
}

// the picture and the background's piece under it change places
void DVPatch::SwapBackground() {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    DVEngine* e = DVEngine::mpEngine;
    if (!d || !e || surface == 0xffffffff) return;
    uint16_t w = d->SurfaceWidth(surface), h = d->SurfaceHeight(surface);
    uint32_t tmp = d->CreateSurface(w, h);
    SBGeoBoundingBox2D box(at, at + SBGeoVector2D((float)w, (float)h));
    uint32_t bg = e->BackgroundSurface();
    d->Blit(bg, &box, tmp, nullptr, 0);
    d->Blit(surface, nullptr, bg, &box, 0);
    d->Blit(tmp, nullptr, surface, nullptr, 0);
    d->DeleteSurface(tmp);
}

void DVPatch::SwapObjects(bool on) {
    for (DVMask* m : masksOff) m->active = !on;
    for (DVMask* m : masksOn) m->active = on;
    for (DVSightObstacle* o : obstaclesOff) o->enabled = !on;
    for (DVSightObstacle* o : obstaclesOn) o->enabled = on;
    for (DVSector* s : sectorsOff) s->flag30 = !on;
    for (DVSector* s : sectorsOn) s->flag30 = on;
}

void DVPatch::Apply() {
    if (hasFx && animating) {
        if (fx) fx->active = false;
        animating = false;
        ApplyFinal();
    }
    if (!applied) {
        SwapDoors();
        if (!fxChange || !fx) {
            if (fx) fx->active = false;
            ApplyFinal();
            return;
        }
        ShowFx(0x95, false);
    } else {
        if (permanent) return;
        SwapBackground();
        SwapDoors();
        if (!fxChange || !fx) {
            if (fx) fx->active = false;
            ApplyFinal();
            return;
        }
        ShowFx(0x95, true);  // backwards
    }
    animating = true;
}

void DVPatch::ApplyFinal() {
    if (permanent) trigger = false;
    if (!applied) {
        applied = true;
        SBLog("patch at %.0f,%.0f on", at.x, at.y);
        if (fxOn) ShowFx(0x96, false);
        else if (fx) fx->active = false;
        SwapBackground();
        SwapObjects(true);
        return;
    }
    if (permanent) return;
    applied = false;
    SBLog("patch at %.0f,%.0f off", at.x, at.y);
    if (fxOff) ShowFx(0x94, false);
    else if (fx) fx->active = false;
    SwapObjects(false);
}

void DVPatch::ForceReset() {
    if (!applied) {
        if (animating) SwapDoors();
    } else {
        SwapBackground();
        if (!animating) SwapDoors();
    }
    applied = false;
    trigger = triggerInit;
    if (hasFx) {
        if (fxOff) ShowFx(0x94, false);
        else if (fx) fx->active = false;
        animating = false;
    }
    SwapObjects(false);
}

void DVPatch::Enter(DVElementActor* a) {
    while (a) {
        if (std::find(inside.begin(), inside.end(), a) == inside.end()) inside.push_back(a);
        // (a horse brings its rider)
        a = a->IsHorse() ? a->rider : nullptr;
    }
}

void DVPatch::Leave(DVElementActor* a) {
    while (a) {
        inside.erase(std::remove(inside.begin(), inside.end(), a), inside.end());
        a = a->IsHorse() ? a->rider : nullptr;
    }
}
