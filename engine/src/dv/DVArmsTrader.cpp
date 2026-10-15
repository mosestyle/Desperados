#include "dv/DVArmsTrader.h"

#include "dv/DVEngine.h"
#include "sb/SBFile.h"

extern int desprand();

DVArmsTrader* DVArmsTrader::mpTheOnlyArmstraderInTown = nullptr;
DVPsychoanalyst* DVPsychoanalyst::mpTheOnlyPsychoanalistInTown = nullptr;

static std::string Name16(SBFile& f) {
    char b[16];
    f.Serialize(b, 16);
    size_t n = 0;
    while (n < 16 && b[n]) ++n;
    return std::string(b, n);
}

// ---- firearms ---------------------------------------------------------------------------------

int DVFirearm::LoadStateFromFile(SBFile& f) {
    id = f.U32();
    name = Name16(f);
    range = f.U16();
    pointBlank = f.U16();
    magazine = f.U16();
    bullets = f.U16();
    spread = f.S16();
    heatIncrement = (uint16_t)(f.U16() * 5);
    heatDecrement = (uint16_t)(f.U16() * 5);
    maxHeat = f.U16();
    for (int i = 0; i < 5; ++i) {
        low[i] = f.U8();
        high[i] = f.U8();
        medium[i] = f.U8();
    }
    return 1;
}

uint16_t DVFirearm::CalculateRandomBulletDamage(uint32_t r, uint32_t distance) const {
    if (special) return 0xfd66;
    DVEngine* e = DVEngine::mpEngine;
    uint32_t d = distance << ((e && e->night) ? 1 : 0);   // at night the shots count twice as far
    if (r < d) return 0;
    if ((uint32_t)pointBlank * r > d * 100) return 12345;  // point blank: dead
    uint32_t pct = (d * 100 / (r ? r : 1)) & 0xffff;
    uint32_t band = pct > 0x18 ? 1 : 0;
    if (pct > 0x31) band = (pct > 0x4a ? 1 : 0) | 2;
    float t = (float)((int)pct - (int)band * 25) * 0.01f;
    if (band > 3) band = 3;
    uint32_t h0 = high[band], h1 = high[band + 1], m0 = medium[band], m1 = medium[band + 1], l0 = low[band], l1 = low[band + 1];
    uint8_t r1 = (uint8_t)desprand();
    uint32_t lo = (uint32_t)((float)((int)l1 - (int)l0) * t + (float)l0) & 0xffff;
    uint32_t hi = (uint32_t)((float)((int)h1 - (int)h0) * t + (float)h0) & 0xffff;
    uint32_t me = (uint32_t)((float)((int)m1 - (int)m0) * t + (float)m0) & 0xffff;
    uint8_t r2 = (uint8_t)desprand();
    float v;
    if (me < ((uint32_t)((float)((int)hi - (int)lo) * ((float)r1 / 255.0f) + (float)lo) & 0xffff))
        v = (float)lo + ((float)r2 / 255.0f) * (float)((int)me - (int)lo);
    else
        v = ((float)r2 / 255.0f) * (float)((int)hi - (int)me) + (float)me;
    uint16_t out = (uint16_t)(int)v;
    return out ? out : 1;
}

DVArmsTrader::DVArmsTrader() { mpTheOnlyArmstraderInTown = this; }
DVArmsTrader::~DVArmsTrader() {
    if (mpTheOnlyArmstraderInTown == this) mpTheOnlyArmstraderInTown = nullptr;
}

bool DVArmsTrader::LoadWeaponCatalogFromFile() {
    catalog.clear();
    SBFile f;
    if (f.Open("Data\\Configuration\\Weapons.dat", 1) != 0) {
        SBError(true, "DVArmsTrader.cpp", 0x41, "Unable to locate weapon profile file");
        return false;
    }
    uint16_t version = f.U16();
    if (version != 4)
        SBError(true, "DVArmsTrader.cpp", 0x4a, "Version Check Failed on %s (file version %u, expected version %u).",
                "weapon profiles", version, 4);
    uint16_t n = f.U16();
    for (uint16_t i = 0; i < n; ++i) {
        auto w = std::make_unique<DVFirearm>();
        w->LoadStateFromFile(f);
        if (w->id == 0xfffffffe) w->range = 0x5dc;   // Doc's sniper colt
        catalog.push_back(std::move(w));
    }
    return true;
}

const DVFirearm* DVArmsTrader::GetFirearmFromCatalog(uint32_t id) const {
    for (const auto& w : catalog)
        if (w->id == id) return w.get();
    return nullptr;
}

// ---- characters ------------------------------------------------------------------------------

int DVCharacterProfile::LoadStateFromFile(SBFile& f) {
    id = f.U32();
    name = Name16(f);
    f.Serialize(properties, 7);
    speech = f.U16();
    firearm = f.U32();
    f.Serialize(hobbies, 8);
    return 1;
}

DVPsychoanalyst::DVPsychoanalyst() { mpTheOnlyPsychoanalistInTown = this; }
DVPsychoanalyst::~DVPsychoanalyst() {
    if (mpTheOnlyPsychoanalistInTown == this) mpTheOnlyPsychoanalistInTown = nullptr;
}

bool DVPsychoanalyst::LoadCompendiumOfPsychologyFromFile() {
    compendium.clear();
    SBFile f;
    if (f.Open("Data\\Configuration\\Characters.dat", 1) != 0) {
        SBError(true, "DVPsychoanalyst.cpp", 0x3d, "Unable to locate character profile file");
        return false;
    }
    uint16_t version = f.U16();
    if (version != 4)
        SBError(true, "DVPsychoanalyst.cpp", 0x46, "Version Check Failed on %s (file version %u, expected version %u).",
                "character profiles", version, 4);
    uint16_t n = f.U16();
    for (uint16_t i = 0; i < n; ++i) {
        auto p = std::make_unique<DVCharacterProfile>();
        p->LoadStateFromFile(f);
        compendium.push_back(std::move(p));
    }
    return true;
}

const DVCharacterProfile* DVPsychoanalyst::GetCharacterFromCompendium(uint32_t id) const {
    for (const auto& p : compendium)
        if (p->id == id) return p.get();
    return compendium.empty() ? nullptr : compendium.front().get();
}
