// data/configuration/characters.dat: hero and enemy profiles.
//   u16 version (4), u16 count, count * 41 bytes:
//     s32 id (heroes negative), char name[16], u8 health, u8 courage, u8 intelligence, u8 ?, u8 ?, u8 ?,
//     u8 marksmanship, u8 weapon, ... behaviour percentages
// NPC elements in a level refer to a profile by its id.
#pragma once
#include <map>
#include <string>

struct Profile {
    int id = 0;
    std::string name;
    int health = 100, courage = 50, intelligence = 50, attention = 50, marksmanship = 50;
};

class ProfileTable {
public:
    bool load();
    const Profile& get(int id) const;
private:
    std::map<int, Profile> byId_;
    Profile fallback_;
};
