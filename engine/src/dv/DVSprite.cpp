#include "dv/DVSprite.h"

#include <cmath>
#include <cstring>

#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVFrameHolder.h"
#include "dv/DVMask.h"
#include "dv/DVSector.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

// the folders of DesperadosWinMain's path strings
static const char* kDirCharacters = "Data\\Characters";  // 0x085dd9e4
static const char* kDirAnimations = "Data\\Animations";  // 0x085dd9fc

// DVSprite::DVSprite (0x083968e0)
DVSprite::DVSprite() {
    animRows.assign(0xb9, -1);
    animRowsAlt.assign(0xb9, -1);
    frameHolder = DVFrameHolder::mpFrameHolder;
    grid = DVFastFindGrid::mpFastFindGrid;
    draw = SBDrawManager::mpDrawManager;
    boxOnMap.Set(SBGeoPoint2D(0, 0), SBGeoPoint2D(1, 1));
}

// DVSprite::LoadSpriteFromFile (0x083971a0)
int DVSprite::LoadSpriteFromFile(SBFile& f, DVframeKind kind) {
    int a = LoadFrameInfoFromFile(f, kind);
    int b = LoadPositionInfoFromFile(f, kind);
    return a + b;
}

// DVSprite::FindProfile (0x083a0be0): moves to the profile named `name` in the dvf file
static bool findProfile(SBFile& f, const char* name) {
    uint16_t count = f.U16();
    uint8_t hdr[0x74];
    f.Serialize(hdr, 0x74);
    unsigned i = 0;
    while (strncmp((const char*)hdr, name, 32) != 0) {
        if ((int)(count - 1) <= (int)(i & 0xffff)) break;
        int16_t dirs, anims;
        memcpy(&dirs, hdr + 0x20, 2);
        memcpy(&anims, hdr + 0x42, 2);
        int16_t rowsN = (int16_t)(dirs * anims);
        for (int16_t r = 0; r != rowsN; ++r) {
            uint8_t rh[0x36];
            f.Serialize(rh, 0x36);
            int16_t entries;
            memcpy(&entries, rh + 4, 2);
            for (int16_t e = entries; e != 0; --e) f.Skip(0xe, 1);
        }
        f.Serialize(hdr, 0x74);
        ++i;
    }
    bool ok = strncmp((const char*)hdr, name, 32) == 0;
    if (ok) f.Skip(-0x74, 1);
    return ok;
}

// reads a profile's rows (the part of LoadFrameInfoFromFile done for the profile and the
// alternative profile)
static int readRows(SBFile& dvf, unsigned firstFrame, std::vector<DVspriteScript>& rows, std::vector<int16_t>& animRows,
                    uint16_t& maxW, uint16_t& maxH, SBGeoVector2D* anchor) {
    uint8_t hdr[0x74];
    dvf.Serialize(hdr, 0x74);
    int16_t dirs, anims;
    int32_t ax, ay;
    memcpy(&dirs, hdr + 0x20, 2);
    memcpy(&anims, hdr + 0x42, 2);
    memcpy(&ax, hdr + 0x58, 4);
    memcpy(&ay, hdr + 0x5c, 4);
    if (anchor) *anchor = SBGeoVector2D((float)ax, (float)ay);
    uint16_t n = (uint16_t)(dirs * anims);
    rows.assign(n, DVspriteScript());
    int frames = 0;
    for (unsigned r = 0; r < n; ++r) {
        uint8_t rh[0x36];
        dvf.Serialize(rh, 0x36);
        uint16_t entries, key, animId, dir;
        int32_t rx, ry;
        memcpy(&entries, rh + 4, 2);
        memcpy(&key, rh + 6, 2);
        memcpy(&rx, rh + 0xa, 4);
        memcpy(&ry, rh + 0xe, 4);
        memcpy(&dir, rh + 0x12, 2);
        memcpy(&animId, rh + 0x14, 2);
        if (animId < animRows.size() && animRows[animId] == -1) animRows[animId] = (int16_t)r;
        DVspriteScript& s = rows[r];
        s.keyEntry = key;
        s.anchor = SBGeoPoint2D((float)rx, (float)ry);
        s.animId = animId;
        s.dirIndex = dir;
        s.frames.resize(entries);
        s.durations.resize(entries);
        s.steps.resize(entries);
        s.offsets.resize(entries);
        s.sounds.resize(entries);
        float total = 0;
        unsigned ticks = 0;
        for (unsigned e = 0; e < entries; ++e) {
            uint16_t frame = dvf.U16();
            int16_t dur = dvf.S16();
            uint16_t step = dvf.U16();
            int16_t x = dvf.S16(), y = dvf.S16();
            uint16_t snd = dvf.U16();
            dvf.U16();
            s.frames[e] = (uint32_t)frame + firstFrame;
            s.durations[e] = dur;
            s.steps[e] = step;
            s.offsets[e] = SBGeoVector2D((float)x, (float)y);
            s.sounds[e] = snd;
            total += (float)step;
            ticks = (uint16_t)(ticks + (dur == 0 ? 1 : dur));
        }
        s.averageStep = ticks ? total / (float)(uint16_t)ticks : 0;
        frames += entries;
    }
    (void)maxW;
    (void)maxH;
    return frames;
}

// DVSprite::LoadFrameInfoFromFile (0x083971e0)
int DVSprite::LoadFrameInfoFromFile(SBFile& f, DVframeKind kind) {
    std::string name = f.String16();
    fileName = name;
    std::string profile = f.String16();
    int size = (int)name.size() + 4 + (int)profile.size();
    std::string altName, altProfile;
    bool hasAlt = false;
    if (kind == FRAME_ACTOR || kind == FRAME_OBJECT) {
        hasAlt = f.U8() != 0;
        if (hasAlt) {
            altName = f.String16();
            fileNameAlt = altName;
            altProfile = f.String16();
            size += (int)altName.size() + 4 + (int)altProfile.size();
        }
        size += 1;
    }
    const char* dir = kind == FRAME_FX ? kDirAnimations : kDirCharacters;
    std::string path = std::string(dir) + "\\" + name + ".dvf";
    SBFile dvf;
    if (dvf.Open(path, 1) != 0) {
        SBError(true, "DVSprite.cpp", 0x1031, "Unable to read DVF file %s in directory %s, file error", (name + ".dvf").c_str(), dir);
        return 0;
    }
    uint8_t head[0x1e];
    dvf.Serialize(head, 0x1e);
    uint16_t version, frameCount, maxW, maxH;
    memcpy(&version, head, 2);
    memcpy(&frameCount, head + 2, 2);
    memcpy(&maxW, head + 6, 2);
    memcpy(&maxH, head + 8, 2);
    if (version != 0x200) {
        SBError(true, "DVSprite.cpp", 0x103a, "Incompatible dvf version please check, error on %s", name.c_str());
        return 0;
    }
    DVEngine* e = DVEngine::mpEngine;
    uint16_t key = e ? e->StateGet(0xf) : 0x1f;
    bool night = e && e->night;
    uint16_t nightPct = e ? e->StateGet(0xe) : 100;
    unsigned first = frameHolder->AddRowFromDVFStream(dvf, frameCount, fileName, key, night, nightPct, kind == FRAME_FX);
    if (!findProfile(dvf, profile.c_str())) {
        SBError(true, "DVSprite.cpp", 0x1044, "Unable to read profile named %s from the dvf file", profile.c_str());
        return 0;
    }
    width = maxW;
    height = maxH;
    readRows(dvf, first, rows, animRows, maxW, maxH, &pos.anchor);
    dvf.Close();
    if (hasAlt) {
        std::string altPath = std::string(dir) + "\\" + altName + ".dvf";
        if (dvf.Open(altPath, 1) != 0) {
            SBError(true, "DVSprite.cpp", 0x1093, "Unable to read DVF file %s in directory %s, file error", (altName + ".dvf").c_str(), dir);
            return 0;
        }
        dvf.Serialize(head, 0x1e);
        memcpy(&version, head, 2);
        memcpy(&frameCount, head + 2, 2);
        memcpy(&maxW, head + 6, 2);
        memcpy(&maxH, head + 8, 2);
        if (version != 0x200) return 0;
        if (width < maxW) width = maxW;
        if (height < maxH) height = maxH;
        unsigned firstAlt = frameHolder->AddRowFromDVFStream(dvf, frameCount, fileNameAlt, key, night, nightPct, kind == FRAME_FX);
        if (!findProfile(dvf, altProfile.c_str())) {
            SBError(true, "DVSprite.cpp", 0x10aa, "Unable to read profile named %s from the dvf file", altProfile.c_str());
            return 0;
        }
        readRows(dvf, firstAlt, rowsAlt, animRowsAlt, maxW, maxH, nullptr);
        dvf.Close();
    }
    return size;
}

// DVSprite::LoadPositionInfoFromFile (0x08398bd0)
int DVSprite::LoadPositionInfoFromFile(SBFile& f, DVframeKind kind) {
    if (kind == FRAME_ACTOR) {
        // the two move boxes, centred on the feet
        for (int b = 0; b < 2; ++b) {
            uint8_t idx = f.U8();
            uint16_t x0 = f.U16(), y0 = f.U16(), x1 = f.U16(), y1 = f.U16();
            float cx = (float)((unsigned)x1 + x0) * 0.5f, cy = (float)((unsigned)y1 + y0) * 0.5f;
            SBGeoBoundingBox2D box(SBGeoPoint2D((float)x0 - cx, (float)y0 - cy), SBGeoPoint2D((float)x1 - cx, (float)y1 - cy));
            if (b == 0) {
                pos.pathIndex = idx;
                pos.moveBoxUp = box;
            } else {
                pos.pathIndexAlt = idx;
                pos.anchor = SBGeoVector2D(cx, cy);
                pos.moveBox = box;
            }
        }
        int16_t x = f.S16(), y = f.S16();
        pos.posSprite = SBGeoPoint2D(std::floor((float)x), std::floor((float)y));
        pos.valid = 4;
        pos.ComputePositionMap();
        pos.layer = f.U16();
        uint16_t sectorIdx = f.U16();
        pos.sector = sectorIdx == 0xffff || !grid ? nullptr : grid->Sector(sectorIdx);
        if (!pos.sector || (pos.sector->type & 3) != 3)
            SBError(true, "DVSprite.cpp", 0x1ab, "VERBOTEN : Character not lying on motion area (%f,%f) !", pos.posMap.x, pos.posMap.y);
        uint16_t obstacle = f.U16();
        if (obstacle == 0) pos.SetObstacle(nullptr);
        else pos.SetObstacle(grid ? grid->ViewArea(obstacle) : nullptr);
        pos.uf0 = f.U8();
        pos.ComputePositionAll();
        displayOrder = pos.pos3D.y;
        uint8_t dir = f.U8();
        pos.direction = dir & 0xf;
        pos.directionWanted = dir & 0xf;
        row = dir;
        entry = 0;
        return 0x1e;
    }
    if (kind == FRAME_FX) {
        int16_t x = f.S16(), y = f.S16();
        uint16_t h = f.U16();
        pos.pos3D = SBGeoVector3D((float)x + pos.anchor.x, (float)y + pos.anchor.y + (float)h, (float)h);
        pos.valid = 1;
        pos.ComputePositionAll();
        displayOrder = pos.pos3D.y;
        return 6;
    }
    if (kind == FRAME_OBJECT) {
        visible = (f.U8() ^ 1) != 0;
        objectValue = f.U16();
        pos.posSprite = SBGeoPoint2D(0, 0);
        pos.valid = 4;
        int size = 3;
        if (visible) {
            int16_t x = f.S16(), y = f.S16();
            pos.posSprite = SBGeoPoint2D(std::floor((float)x), std::floor((float)y));
            pos.valid = 4;
            pos.layer = f.U16();
            uint16_t sectorIdx = f.U16();
            pos.sector = sectorIdx == 0xffff || !grid ? nullptr : grid->Sector(sectorIdx);
            uint16_t obstacle = f.U16();
            if (obstacle != 0) {
                pos.SetObstacle(grid ? grid->ViewArea(obstacle) : nullptr);
                pos.ComputePositionAll();
            }
            uint16_t dir = f.U16();
            pos.direction = dir & 0xf;
            pos.directionWanted = dir & 0xf;
            uint16_t anim = f.U16();
            int16_t r = anim < animRows.size() ? animRows[anim] : -1;
            if (r == -1) {
                if (animRows.size() > 7 && animRows[7] == -1) {
                    int a = -1;
                    do ++a;
                    while (a + 1 < (int)animRows.size() && animRows[a] == -1);
                    animation = (uint32_t)a;
                } else {
                    animation = 7;
                }
                row = 0;
                entry = 0;
            } else {
                row = (uint16_t)r;
                animation = anim;
                entry = 0;
            }
            size = 0x11;
        }
        pos.moveBoxUp = SBGeoBoundingBox2D(SBGeoPoint2D(-2, -2), SBGeoPoint2D(2, 2));
        pos.ComputePositionAll();
        displayOrder = pos.pos3D.y;
        return size;
    }
    return 0;
}

const DVspriteScript* DVSprite::CurrentRow() const {
    const auto& R = Rows();
    return row < R.size() ? &R[row] : nullptr;
}

uint32_t DVSprite::CurrentFrameId(unsigned next) const {
    const DVspriteScript* r = CurrentRow();
    if (!r || r->frames.empty()) return 0xffffffff;
    unsigned e = entry + next;
    if (e >= r->frames.size()) e = (unsigned)r->frames.size() - 1;
    return r->frames[e];
}

// DVSprite::ComputeDisplayOrder (0x0839a430)
void DVSprite::ComputeDisplayOrder(DVElement* other, bool before) {
    if (!other) {
        displayOrder = pos.pos3D.y;
        return;
    }
    (void)before;
}

// DVSprite::IsOnScreen (0x0839cca0): also keeps the sprite's box on the map (+0x208)
bool DVSprite::IsOnScreen(const SBGeoBoundingBox2D& view, float zoom) {
    uint32_t id = CurrentFrameId();
    float w = (float)frameHolder->GetSpriteWidth(id), h = (float)frameHolder->GetSpriteHeight(id);
    if ((pos.valid & 4) == 0) pos.ComputePositionSprite();
    const DVspriteScript* r = CurrentRow();
    SBGeoPoint2D corner = pos.posSprite;
    if (r && entry < r->offsets.size()) corner = r->offsets[entry] + pos.posSprite;
    boxOnMap.Set(corner, corner + SBGeoVector2D(w, h));
    return view.IsIntersecting(boxOnMap);
}

// DVSprite::GenerateBlitBox (0x0839d710): the part of the sprite's surface to draw (src) and
// where on the screen (dst), clipped to the view
bool DVSprite::GenerateBlitBox(const SBGeoBoundingBox2D& view, float zoom, SBGeoBoundingBox2D& src,
                               SBGeoBoundingBox2D& dst) {
    const DVspriteScript* r = CurrentRow();
    SBGeoPoint2D corner = pos.posSprite;
    if (r && entry < r->offsets.size()) corner = pos.posSprite + r->offsets[entry];
    SBGeoVector2D size((float)width, (float)height);
    SBGeoPoint2D viewCorner = view.p0;
    dst.Set(corner, corner + size);
    dst = view.Clip(dst);
    if (!dst.IsSomewhere()) return false;
    SBGeoVector2D inSprite = dst.p0 - corner;
    if (zoom != 0.5f) {
        src.Set(inSprite, inSprite + SBGeoVector2D(dst.p1.x - dst.p0.x, dst.p1.y - dst.p0.y));
        if (zoom == 2.0f) {
            dst.Set(SBGeoPoint2D((dst.p0.x - viewCorner.x) * 2, (dst.p0.y - viewCorner.y) * 2),
                    SBGeoPoint2D((dst.p1.x - viewCorner.x) * 2, (dst.p1.y - viewCorner.y) * 2));
        } else {
            dst.Set(dst.p0 - viewCorner, dst.p1 - viewCorner);
        }
    } else {
        int ax = (int)(dst.p0.x - viewCorner.x), ay = (int)(dst.p0.y - viewCorner.y);
        int bx = (int)(dst.p1.x - viewCorner.x), by = (int)(dst.p1.y - viewCorner.y);
        int ox = ax & 1, oy = ay & 1;
        float sx = std::floor(inSprite.x), sy = std::floor(inSprite.y);
        dst.Set(SBGeoPoint2D((float)((ax + ox) >> 1), (float)((ay + oy) >> 1)),
                SBGeoPoint2D((float)((bx - 1 + (bx & 1)) >> 1), (float)((by - 1 + (by & 1)) >> 1)));
        SBGeoPoint2D s0((float)((int)sx + ox), (float)((int)sy + oy));
        src.Set(s0, s0 + SBGeoVector2D((dst.p1.x - dst.p0.x) * 2, (dst.p1.y - dst.p0.y) * 2));
    }
    return dst.IsOK() && src.IsOK();
}

// DVSprite::CreateTargetSprite (0x0839fbd0): unpacks the current frame into the surface and
// removes what the background's masks hide
bool DVSprite::CreateTargetSprite(const DVspriteRenderingInfo& info) {
    uint32_t id = CurrentFrameId(info.flags & 1);
    if (id == 0xffffffff) return false;
    // nothing changed since the last time: the surface still holds the right picture
    uint64_t key = (uint64_t)id ^ ((uint64_t)(uint32_t)(int)pos.posSprite.x << 20) ^
                   ((uint64_t)(uint32_t)(int)pos.posSprite.y << 40) ^ ((uint64_t)info.flags << 60) ^
                   ((uint64_t)info.applyMasks << 59) ^ ((uint64_t)pos.layer << 52) ^ ((uint64_t)info.shadowColor << 8);
    width = frameHolder->GetSpriteWidth(id);
    height = frameHolder->GetSpriteHeight(id);
    if (key == lastTargetKey && draw->IsSurface(info.surface)) return true;
    SBDrawViewport vp;
    if (!draw->GetSurfaceViewport(info.surface, vp, VIEWPORT_WRITE)) return false;
    if (width > vp.width) width = vp.width;
    if (height > vp.height) height = vp.height;
    if (info.flags & 2) frameHolder->UnCompressFrameWipeShadow(vp, id);
    else if (info.flags & 8) frameHolder->UnCompressFrameIntoTheShadow(vp, id, info.shadowColor);
    else frameHolder->UnCompressFrame(vp, id);
    drawn = true;
    if (info.applyMasks && grid) {
        std::vector<DVMask*> masks;
        SBGeoBoundingBox2D box = boxOnMap;
        if (info.maskKind == 0) grid->GetMasksAppliedToCharacter(masks, pos.layer, box, pos.posMap);
        if (!masks.empty()) masked = true;
        const DVspriteScript* r = CurrentRow();
        SBGeoPoint2D corner = pos.posSprite;
        if (r && entry < r->offsets.size()) corner = r->offsets[entry] + pos.posSprite;
        for (DVMask* m : masks) {
            if (info.hidden) DrawMaskOverSpriteHidden(*m, vp, corner);
            else DrawMaskOverSprite(*m, vp, corner, info.outlineColor);
        }
    }
    draw->ReleaseSurfaceViewport(vp);
    lastTargetKey = key;
    return true;
}

// DVSprite::DrawMaskOverSprite (0x083a01b0) and DrawMaskOverSpriteHidden (0x083a0730).
// The mask bitmap: per row a length byte, then runs: a control byte c (bit 7 = one data byte
// repeated, else c & 0x7f literal data bytes), each data byte = 8 pixels, highest bit first.
// Where a bit is set, the sprite's pixel becomes the transparent colour; with an outline colour,
// the pixel at a transparent / opaque edge gets that colour instead (the silhouette line).
template <bool OUTLINE>
static void maskOverSprite(const DVMask& m, SBDrawViewport& vp, const SBGeoPoint2D& corner, uint16_t outline,
                           uint16_t spriteW, uint16_t spriteH, uint16_t shadowKey) {
    const uint16_t bits = vp.bits;
    const uint16_t clear = bits == 0xf ? 0x3e0 : 0x7c0;
    if (!vp.data || (bits != 0xf && bits != 0x10)) return;
    uint32_t c24 = (uint32_t)(int)(m.box.p0.x - corner.x);
    uint32_t maskW = (uint32_t)(int)(m.box.p1.x - m.box.p0.x);
    int16_t s21 = (int16_t)c24;
    int skipX;        // local_1c: mask columns left of the sprite
    uint32_t cols;    // local_14
    if (s21 < 1) {
        skipX = -s21;
        cols = (maskW & 0xffff) - skipX;
        if ((int)(maskW & 0xffff) < skipX) return;
        if ((int)spriteW < (int)cols) cols = spriteW;
        c24 = 0;
    } else {
        cols = (uint32_t)spriteW - s21;
        if ((int)spriteW < s21) return;
        if ((int)(maskW & 0xffff) <= (int)cols) cols = maskW & 0xffff;
        skipX = 0;
    }
    uint32_t r22 = (uint32_t)(int)(m.box.p0.y - corner.y);
    uint32_t maskH = (uint32_t)(int)(m.box.p1.y - m.box.p0.y);
    s21 = (int16_t)r22;
    int skipY;
    uint32_t rowsN;   // local_20
    if (s21 < 1) {
        skipY = -s21;
        rowsN = (maskH & 0xffff) - skipY;
        if (rowsN == 0 || (int)(maskH & 0xffff) < skipY) return;
        if ((int)spriteH < (int)rowsN) rowsN = spriteH;
        r22 = 0;
    } else {
        rowsN = (uint32_t)spriteH - s21;
        if (rowsN == 0 || (int)spriteH < s21) return;
        if ((int)(maskH & 0xffff) <= (int)rowsN) rowsN = maskH & 0xffff;
        skipY = 0;
    }
    const uint32_t pitch = vp.pitch >> 1 & 0xffff;
    uint16_t* rowPtr = vp.data + (size_t)pitch * (int16_t)r22;
    const float maskWf = m.box.p1.x - m.box.p0.x;
    uint32_t base = 0;  // uVar10: start of the current mask row
    for (int i = 0; i < skipY; ++i) base = base + 1 + m[base & 0xffff];
    const int endRow = (int)(rowsN & 0xffff) + (int16_t)r22;
    if (endRow <= (int)(r22 & 0xffff)) return;
    const int skipRounded = skipX + (skipX < 0 ? 7 : 0);  // iVar13
    const int firstCol = (int16_t)c24;                   // iVar14
    const int lastCol = firstCol - 1 + (int)(cols & 0xffff);  // iVar1
    for (;;) {
        base &= 0xffff;
        uint32_t at = 1;         // local_20: read position in the row
        uint8_t past = 0;        // uVar17: the row ended before the sprite
        uint32_t ctrlPos = 1;    // uVar26
        uint8_t cur = m[base + 1];
        uint32_t groups = 0;     // uVar12
        bool ended = false;
        uint8_t runLeft = 0, repeat = 0, bitsLeft = 0;  // local_31, local_41, local_25
        for (;;) {
            uint32_t g = groups & 0xffff;
            uint8_t ctrl = cur;
            bool stop = true;
            uint8_t hi = 1;
            if ((float)g <= maskWf * 0.125f) {
                hi = past;
                stop = ended;
            }
            past = hi;
            groups = (ctrl & 0x7f) + g;
            if (groups * 8 - skipX != 0 && skipX <= (int)(groups * 8)) {
                int inRun = (skipRounded >> 3) - (int)g;
                uint32_t keep = at;
                runLeft = (uint8_t)((ctrl & 0x7f) - inRun);
                at = inRun + 1 + at;
                if ((int8_t)ctrl < 0) at = (uint16_t)(keep + 1);
                repeat = ctrl >> 7;
                cur = m[(at & 0xffff) + base];
                bitsLeft = (uint8_t)(8 - (skipX - (skipRounded & 0xf8)));
                break;
            }
            if ((int8_t)ctrl < 0) at = at + 2;
            else at = (uint16_t)((((uint32_t)ctrl + 1) & 0x7f) + (uint16_t)ctrlPos);
            ctrlPos = at & 0xffff;
            cur = m[ctrlPos + base];
            ended = stop;
            if (stop) break;
        }
        uint8_t data = cur;
        for (uint32_t x = c24 & 0xffff; (int)x < firstCol + (int)(cols & 0xffff); ++x) {
            uint8_t flag = 1;
            if ((float)(int)(x + skipX) <= maskWf + (float)firstCol) flag = past;
            if (flag == 0) {
                bitsLeft = (uint8_t)(bitsLeft - 1);
                if ((data >> (bitsLeft & 7)) & 1) {
                    if (OUTLINE) {
                        uint16_t a = rowPtr[x], b = (int)x < lastCol ? rowPtr[x + 1] : a;
                        if (a == shadowKey) a = clear;
                        if (b == shadowKey) b = clear;
                        if ((int)x < lastCol && a != b && (a == clear || b == clear)) rowPtr[x] = outline;
                        else rowPtr[x] = clear;
                    } else {
                        rowPtr[x] = clear;
                    }
                }
                if (bitsLeft == 0) {
                    runLeft = (uint8_t)(runLeft - 1);
                    if (runLeft == 0) {
                        uint8_t c = m[((at + 1) & 0xffff) + base];
                        at = at + 2;
                        runLeft = c & 0x7f;
                        data = m[(at & 0xffff) + base];
                        repeat = c >> 7;
                    } else if ((repeat & 1) == 0) {
                        at = at + 1;
                        data = m[(at & 0xffff) + base];
                    }
                    bitsLeft = 8;
                }
            }
        }
        base = base + 1 + m[base];
        rowPtr += pitch;
        r22 = r22 + 1;
        if (endRow <= (int)(r22 & 0xffff)) return;
    }
}

void DVSprite::DrawMaskOverSprite(const DVMask& m, SBDrawViewport& vp, const SBGeoPoint2D& corner, uint16_t outline) {
    uint16_t key = DVEngine::mpEngine ? DVEngine::mpEngine->StateGet(0xf) : 0x1f;
    maskOverSprite<true>(m, vp, corner, outline, width, height, key);
}

void DVSprite::DrawMaskOverSpriteHidden(const DVMask& m, SBDrawViewport& vp, const SBGeoPoint2D& corner) {
    maskOverSprite<false>(m, vp, corner, 0, width, height, 0);
}

static uint16_t dur(const DVspriteScript& r, unsigned e) { return e < r.durations.size() ? (uint16_t)r.durations[e] : 0; }
static unsigned count(const DVspriteScript& r) { return (unsigned)(r.durations.size() & 0xffff); }

extern int desprand();

// DVSprite::IncrementFrame (0x0839a780). Progressions: 0/6 loop, 1 loop (no end report), 2 one
// frame per tick, 3 two entries at a time, 4/5 loop that starts at random, 7 turning, 8/9 stop
// before the end, 0xb restart, 0xc jump to the end, 0xd backwards.
bool DVSprite::IncrementFrame(int p) {
    const auto& R = Rows();
    if (row >= R.size()) return false;
    const DVspriteScript& r = R[row];
    unsigned n = count(r);
    switch (p) {
    case 0:
    case 6: {
        uint16_t t = (uint16_t)(tick + 1);
        tick = t;
        unsigned e = entry;
        if (dur(r, e) < t) {
            tick = 0;
            e = e + 1;
            entry = (uint16_t)e;
            t = 0;
        }
        if (n <= (e & 0xffff)) {
            entry = 0;
            e = 0;
        }
        e &= 0xffff;
        if (e != n - 1) return false;
        if (t == dur(r, e)) return true;
        return dur(r, e) == 0;
    }
    case 1: {
        uint16_t t = (uint16_t)(tick + 1);
        tick = t;
        unsigned e = entry;
        if (dur(r, e) < t) {
            tick = 0;
            e = e + 1;
            entry = (uint16_t)e;
        }
        if (n <= (e & 0xffff)) entry = 0;
        return false;
    }
    case 2: {
        tick = 0;
        uint16_t e = (uint16_t)(entry + 1);
        entry = e;
        unsigned e3 = entry;
        if (n < e) {
            entry = 0;
            e3 = 0;
        }
        return (e3 & 0xffff) == n;
    }
    case 3: {
        unsigned t = (uint16_t)(tick + 1);
        tick = (uint16_t)t;
        unsigned e = entry;
        if (dur(r, e) < (t & 0xffff)) {
            tick = 0;
            e = e + 2;
            entry = (uint16_t)e;
            t = 0;
        }
        if ((e & 0xffff) == n) {
            entry = 0;
            e = 0;
        }
        if ((e & 0xffff) != n - 2) return false;
        return (t & 0xffff) == dur(r, e & 0xffff);
    }
    case 4:
    case 5: {
        uint16_t t = tick;
        if ((unsigned)entry + t == 0) {
            if (desprand() > (p == 4 ? 0x82 : 0x146)) return false;
            t = tick;
        }
        tick = (uint16_t)(t + 1);
        unsigned e = entry;
        if (dur(r, e) < (uint16_t)(t + 1)) {
            tick = 0;
            e = e + 1;
            entry = (uint16_t)e;
        }
        if (n <= (e & 0xffff)) entry = 0;
        return false;
    }
    case 7: {
        unsigned t = (uint16_t)(tick + 1);
        tick = (uint16_t)t;
        unsigned e = entry;
        if (entry == r.keyEntry) {
            if ((t & 0xffff) < 9) {
                int16_t d = (int16_t)pos.direction;
                uint16_t nd = (uint16_t)(d + 0xe);
                if ((int16_t)(d - 2) > -1) nd = (uint16_t)(d - 2);
                pos.direction = nd & 0xf;
                pos.directionWanted = nd & 0xf;
                return false;
            }
            tick = 0;
            entry = (uint16_t)(e + 1);
            return false;
        }
        if (dur(r, e) < (t & 0xffff)) {
            tick = 0;
            e = e + 1;
            entry = (uint16_t)e;
            t = 0;
        }
        if (n <= (e & 0xffff)) {
            entry = 0;
            e = 0;
        }
        if ((e & 0xffff) != n - 1) return false;
        return (t & 0xffff) == dur(r, e & 0xffff);
    }
    case 8:
    case 9: {
        int last = (int)n - (p == 8 ? 1 : 2);
        if ((int)entry < last) {
            uint16_t t = (uint16_t)(tick + 1);
            tick = t;
            if (dur(r, entry) < t) {
                tick = 0;
                entry = (uint16_t)(entry + 1);
            }
        }
        return false;
    }
    case 0xb:
        entry = 0;
        return false;
    case 0xc:
        entry = (uint16_t)(n - 1);
        return false;
    case 0xd: {
        uint16_t t = (uint16_t)(tick + 1);
        tick = t;
        unsigned e = entry;
        if (dur(r, e) < t) {
            tick = 0;
            e = e - 1;
            entry = (uint16_t)e;
            t = 0;
        }
        int16_t se = (int16_t)e;
        if (se < 0) {
            se = (int16_t)(n - 1);
            entry = (uint16_t)se;
        }
        if (se != 0) return false;
        if (t == dur(r, 0)) return true;
        return dur(r, 0) == 0;
    }
    }
    return false;
}

// DVSprite::PerformVirginIncrement (0x0839b150): 3 = the row ended, 0 = back on the key frame
int DVSprite::PerformVirginIncrement(int p) {
    bool ended = IncrementFrame(p);
    int r = 2;
    if (tick == 0) {
        const DVspriteScript* cr = CurrentRow();
        r = (cr && entry == cr->keyEntry) ? 0 : 2;
    }
    return ended ? 3 : r;
}

// DVSprite::ResetSpriteFrame (0x0839b5b0)
void DVSprite::ResetSpriteFrame(bool toEnd) {
    if (toEnd) {
        const DVspriteScript* r = CurrentRow();
        entry = (uint16_t)((r ? count(*r) : 0) - 1);
        tick = 0xffff;
        return;
    }
    entry = 0;
    tick = 0xffff;
}

// DVSprite::IsEmptyFrame (0x083a1610)
bool DVSprite::IsEmptyFrame() const {
    uint32_t id = CurrentFrameId();
    return frameHolder->GetSpriteWidth(id) == 1 && frameHolder->GetSpriteHeight(id) == 1;
}

uint16_t DVSprite::GetCurrentSoundID() const {
    const DVspriteScript* r = CurrentRow();
    if (!r || entry >= r->sounds.size() || tick != 0) return 0;
    return r->sounds[entry];
}
