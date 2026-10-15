// The game's catalogues (Game/DVArmsTrader.cpp, DVFirearm.cpp, DVPsychoanalyst.cpp,
// DVCharacterProfile.cpp): Data\Configuration\Weapons.dat (every firearm: range, magazine,
// damage tables) and Data\Configuration\Characters.dat (every character profile: energy, IQ,
// shooting skill, ... and the firearm he carries). Both are loaded once, at the start.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class SBFile;

// DVFirearm (0x74 bytes)
struct DVFirearm {
    uint32_t id = 0;            // +0x6c (heroes' special weapons: 0xfffffffe...)
    std::string name;
    uint32_t range = 0;         // +0x58 in map pixels (ground distance)
    uint16_t pointBlank = 0;    // +0x5c closer than this % of the range, a hit kills
    uint16_t magazine = 0;      // +0x5e bullets before reloading
    uint16_t bullets = 1;       // +0x60 bullets a shot
    int16_t spread = 0;         // +0x62 > 0: a scattering shot (shotgun, gattling)
    uint16_t heatIncrement = 0; // +0x68 (x 5)
    uint16_t heatDecrement = 0; // +0x64 (x 5)
    uint16_t maxHeat = 0;       // +0x66
    // damages at 0, 25, 50, 75 and 100 % of the range
    uint16_t high[5] = {}, medium[5] = {}, low[5] = {};  // +4, +0x20, +0x3c
    bool special = false;       // +0x70 (a non-lethal weapon)

    int LoadStateFromFile(SBFile& f);  // 0x082c7c30
    // DVFirearm::CalculateRandomBulletDamage (0x082c8090): the damage of a bullet that hits at
    // `distance`; 0 out of range, 12345 (kills) at point blank
    uint16_t CalculateRandomBulletDamage(uint32_t range, uint32_t distance) const;
};

class DVArmsTrader {
public:
    static DVArmsTrader* mpTheOnlyArmstraderInTown;
    DVArmsTrader();
    ~DVArmsTrader();
    bool LoadWeaponCatalogFromFile();               // 0x08198a70
    const DVFirearm* GetFirearmFromCatalog(uint32_t id) const;  // 0x08198e50
private:
    std::vector<std::unique_ptr<DVFirearm>> catalog;
};

// DVCharacterProfile (0x1c bytes)
struct DVCharacterProfile {
    uint32_t id = 0;             // +0
    std::string name;
    // DVcharacterProperties (+10): 0 energy (the life), 1 courage, 2 IQ, 3 fitness, 4 laziness,
    // 5 aggressiveness, 6 shooting skill
    uint8_t properties[7] = {};
    uint16_t speech = 0;         // +8
    uint32_t firearm = 0xffffffff;  // +4
    uint8_t hobbies[8] = {};     // +0x11
    int LoadStateFromFile(SBFile& f);  // 0x081f5e60
};

class DVPsychoanalyst {
public:
    static DVPsychoanalyst* mpTheOnlyPsychoanalistInTown;
    DVPsychoanalyst();
    ~DVPsychoanalyst();
    bool LoadCompendiumOfPsychologyFromFile();       // 0x08343480
    const DVCharacterProfile* GetCharacterFromCompendium(uint32_t id) const;  // 0x08343830
private:
    std::vector<std::unique_ptr<DVCharacterProfile>> compendium;
};

// the heroes' profiles (DVCooper::DVCooper ...): Cooper 0xfffffffe, Sam 0xfffffffd, Doc
// 0xfffffffc, Kate 0xfffffffb, Sanchez 0xfffffffa, Mia 0xfffffff9
inline uint32_t HeroProfileId(int kind) { return 0xffffffffu - (uint32_t)kind; }
