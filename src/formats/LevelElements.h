// ELEM chunk of a level file: placed characters, animals and animated scenery.
//
// The chunk is a serialized object list whose exact layout is not fully known yet,
// so elements are found by their signature: a u16-length file name that matches a
// sprite file, followed by a u16-length set name.
//   scenery (data/animations): s16 x, s16 y (top-left), s16 z (height of its base), 3 flag bytes
//   actors (data/characters):  u8 hasAlt [, alt file, alt set], 18 bytes collision boxes,
//                              s16 x, s16 y (top-left of the anchor box; feet = +anchor),
//                              6 bytes, u8 floor, u8 direction (0-15)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

// Two bytes before every element: u8 subtype, u8 class.
//   class 0  = player hero (subtype: 1 Cooper, 2 Doc, 3 Sam, 4 Kate, 5 Sanchez, 6 Mia, 7 Mr Leone)
//   class 1  = NPC (subtype 1 enemy, 2 civilian)
//   class 2  = animal (1 horse, 16 dog, 17 cow/donkey, 18 hen, 19 pig, 20 crow, 21 crocodile)
//   class 8  = scripted object, 16 = animated scenery, 17 = inventory item
enum class Faction { None, Hero, Enemy, Civilian, Animal };

struct LevelElement {
    enum Kind { Scenery, Actor, Dummy, Item } kind = Scenery;
    int cls = -1, sub = 0;
    Faction faction = Faction::None;
    std::string folder;  // "characters" or "animations"
    std::string file, set;
    int x = 0, y = 0, z = 0;
    int dir = 0, floor = 0;
    // NPCs: profile id (characters.dat), patrol path index (-1 = none), script class, start animation
    int profile = 0, path = -1, startAnim = -1;
    std::string script;
    size_t offset = 0;  // where the element's file name starts in the ELEM chunk
    int boxX1 = 0, boxY1 = 0;  // scripted objects: bottom-right of the hit box (top-left = x,y)
    // items (accessories.dvf): type = sub; in a hero's bag (count) or lying on the map (x, y, floor);
    // a saddle can be on a horse (link = the horse's element index)
    bool carried = false;
    int count = 0, link = -1;
};

// Known sprite file stems (lower case, without .dvf) per folder.
struct SpriteIndex {
    std::unordered_set<std::string> characters, animations;
    void build();
};

std::vector<LevelElement> scanElements(const uint8_t* data, size_t size, const SpriteIndex& index);
