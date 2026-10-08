#include "dv/DVElement.h"

#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVFrameHolder.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

static uint32_t gCreationCounter = 0;  // gulCreationCounter

// SBDrawManager::CreateColor (0x0814ed60)
static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((b >> 3) | ((r & 0xf8u) * 0x100 + (g & 0xfcu) * 8));
}

// DVElement::DVElement (0x08249160)
DVElement::DVElement() {
    creation = gCreationCounter++;
    active = true;
    shadows = true;
    sprite.reset(new DVSprite());
    shadowKey = DVEngine::mpEngine ? DVEngine::mpEngine->StateGet(0xf) : 0x1f;
}

DVElement::~DVElement() {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (d) {
        if (surface != 0xffffffff) d->DeleteSurface(surface);
        if (surface2 != 0xffffffff) d->DeleteSurface(surface2);
    }
}

void DVElement::MakeSurfaces(int extraW2, int extraH2, bool second) {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    surface = d->CreateSurface(sprite->width, sprite->height);
    if (second) surface2 = d->CreateSurface((uint16_t)(sprite->width + extraW2), (uint16_t)(sprite->height + extraH2));
}

const char* DVElement::KindName() const {
    switch (kind) {
    case KIND_COOPER: return "Cooper";
    case KIND_DOC: return "Doc";
    case KIND_SAM: return "Sam";
    case KIND_KATE: return "Kate";
    case KIND_SANCHEZ: return "Sanchez";
    case KIND_MIA: return "Mia";
    case KIND_MRLEONE: return "Mr Leone";
    case KIND_VILLAIN: return "villain";
    case KIND_CIVILIAN: return "civilian";
    case KIND_HORSE: return "horse";
    case KIND_OBJECT: return "object";
    case KIND_TARGET: return "target";
    case KIND_FX: return "scenery";
    }
    if (kind >= 0x210 && kind <= 0x215) return "animal";
    if (kind >= 0x1101 && kind <= 0x1117) return "item";
    return "?";
}

void DVElement::Refresh(uint32_t, float, const SBGeoBoundingBox2D&, bool) {}

// what DVElementObject / DVElementActorNPC::Refresh do to draw: the frame into the element's
// surface (masked), then onto the screen with the shadow colour at the frame holder's percentage
void DVElement::DrawSprite(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool masks, bool hiddenMasks,
                           bool shadowBlit) {
    DVSprite* s = sprite.get();
    if (!s->IsOnScreen(view, zoom)) return;
    DVspriteRenderingInfo info;
    info.surface = surface;
    info.hidden = hiddenMasks;
    info.outlineColor = colorOutline;
    info.applyMasks = masks;
    s->CreateTargetSprite(info);
    SBGeoBoundingBox2D src, dst;
    s->GenerateBlitBox(view, zoom, src, dst);
    if (!src.IsOK() || !dst.IsOK()) return;
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (shadowBlit && shadows) {
        uint16_t pct = DVFrameHolder::mpFrameHolder ? DVFrameHolder::mpFrameHolder->shadowPercent : 40;
        d->BlitAlphaKeying(surface, &src, screen, &dst, shadowKey, pct, BLIT_COLORKEY);
    } else {
        d->Blit(surface, &src, screen, &dst, BLIT_COLORKEY);
    }
}

// ---- actors -------------------------------------------------------------------------------

// DVElementActorPC / NPC / Animal / Horse ::LoadStateFromFile (0x08241a70, 0x08234f20,
// 0x082045e0, 0x082167f0)
int DVElementActor::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_ACTOR);
    if (kind >= KIND_COOPER && kind <= KIND_MIA) {
        uint16_t len = f.U16();
        f.Skip(len, 1);
        profile = f.U16();
        MakeSurfaces(4, 0, true);
        return n + 4 + len;
    }
    if (kind == KIND_VILLAIN || kind == KIND_CIVILIAN) {
        scriptName = f.String16();
        int len = (int)scriptName.size();
        // the AI attributes (DVArtificialMalignity / Bonhomie::LoadAttributesFromFile)
        int ai;
        if (kind == KIND_VILLAIN) {
            profile = f.U32();
            aiShort = f.U16();
            aiAttitude = f.U8() & 0x7f;
            aiByte = f.U8();
            ai = 8;
        } else {
            aiValue = f.U32();
            aiAttitude = f.U8() & 0x7f;
            aiByte = f.U8();
            ai = 6;
        }
        // DVPath::LoadFromFile: the patrol route
        pathIndex = f.U16();
        f.U16();  // (stored in the AI: +0x24)
        MakeSurfaces(2, 2, true);
        return ai + 2 + 4 + n + len;
    }
    if (kind == KIND_HORSE) {
        uint16_t len = f.U16();
        f.Skip(len, 1);
        f.U16();
        MakeSurfaces(0, 0, true);
        return n + 4 + len;
    }
    // animals (and Mr Leone's dog, kind 7)
    uint16_t len = f.U16();
    f.Skip(len, 1);
    MakeSurfaces(0, 0, true);
    f.Skip(2, 1);
    return n + 4 + len;
}

// DVElementActorNPC::Refresh (0x0822cae0), the drawing part
void DVElementActor::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active) return;
    DrawSprite(screen, zoom, view, true, silhouettes, true);
}

// ---- objects ------------------------------------------------------------------------------

DVElementObject::DVElementObject() {
    classFlags = 1;
    colorOutline = color565(0xff, 0xf7, 'Z');
    colorFocus = colorOutline;
}

// DVElementObject::LoadStateFromFile (0x0825b8f0)
int DVElementObject::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_OBJECT);
    active = sprite->visible;
    value9a = sprite->objectValue;
    animation = sprite->animation;
    u9c = f.U16();
    if (objectType == 0xc) {
        u94 = f.U16();
        u96 = f.U16();
        if (u96 < u94) u96 = (uint16_t)(u96 + 0x10);
        u98 = sprite->pos.direction;
        n += 6;
    } else {
        n += 2;
    }
    MakeSurfaces(2, 0, objectType == 9);
    return n;
}

// DVElementObject::Refresh (0x0825bfc0)
void DVElementObject::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active) return;
    bool hidden = (objectType == 0xb && animation <= 0x13 && ((0xc0001u >> (animation & 0x1f)) & 1)) ? true : silhouettes;
    DrawSprite(screen, zoom, view, true, hidden, shadows);
}

// ---- animated scenery ---------------------------------------------------------------------

DVElementFX::DVElementFX() { classFlags = 0; }

// DVElementFX::LoadStateFromFile (0x0824afb0)
int DVElementFX::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_FX);
    shadowKind = f.U8();
    active = f.U8() != 0;
    restore = f.U8() != 0;
    MakeSurfaces(0, 0, false);
    return n + 3;
}

// DVElementFX::Refresh (0x08249ea0): no masks, a shadow if the scenery has one
void DVElementFX::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active) return;
    DVSprite* s = sprite.get();
    if (!s->IsOnScreen(view, zoom)) return;
    DVspriteRenderingInfo info;
    info.surface = surface;
    info.hidden = true;
    s->CreateTargetSprite(info);
    SBGeoBoundingBox2D src, dst;
    s->GenerateBlitBox(view, zoom, src, dst);
    if (!src.IsOK() || !dst.IsOK()) return;
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (shadowKind == 1 && shadows) {
        uint16_t pct = DVFrameHolder::mpFrameHolder ? DVFrameHolder::mpFrameHolder->shadowPercent : 40;
        d->BlitAlphaKeying(surface, &src, screen, &dst, shadowKey, pct, BLIT_COLORKEY);
    } else {
        d->Blit(surface, &src, screen, &dst, BLIT_COLORKEY);
    }
}

// DVElementFX::Hourglass (0x0824b330)
void DVElementFX::Hourglass() {
    if (!active) return;
    sprite->PerformVirginIncrement(0);
}

// DVElementTarget::LoadStateFromFile (0x08265870)
int DVElementTarget::LoadStateFromFile(SBFile& f) {
    int n = DVElementFX::LoadStateFromFile(f);
    sprite->pos.ComputePositionSprite();
    int16_t x = f.S16(), y = f.S16();
    sprite->pos.SetPositionMap(SBGeoPoint2D((float)x, (float)y));
    sprite->pos.layer = f.U16();
    uint16_t sec = f.U16();
    sprite->pos.sector = sprite->grid && sec != 0xffff ? sprite->grid->Sector(sec) : nullptr;
    sprite->pos.valid |= 6;
    uint16_t ob = f.U16();
    if (ob == 0) sprite->pos.SetObstacle(nullptr);
    else sprite->pos.SetObstacle(sprite->grid ? sprite->grid->ViewArea(ob) : nullptr);
    sprite->pos.valid |= 1;
    value144 = f.U32();
    scriptName = f.String16();
    return n + 0x10 + (int)scriptName.size();
}

void DVElementTarget::Hourglass() {}
