// data/configuration/weapons.dat
//   u16 version (4), u16 count, count * 51 bytes:
//     s32 id, char name[16], u16 range, u16 ?, u16 ammo, u16, u16, u16, u16, u16, u8 table[15]
// Heroes' weapons are named "Colt Cooper", "Winchester Sam", "Pistolet Kate", "Fusil Sanchez",
// "Colt Doc", "Sarbacanne Mia".
#pragma once
#include <map>
#include <string>

struct Weapon {
    std::string name;
    int range = 400, ammo = 6;
};

class WeaponTable {
public:
    bool load();
    const Weapon* find(const std::string& name) const;
private:
    std::map<std::string, Weapon> byName_;
};
