// ELEM chunk of a level file: placed characters, animals and animated scenery.
//
// The chunk is a serialized object list whose exact layout is not fully known yet,
// so elements are found by their signature: a u16-length file name that matches a
// sprite file, followed by a u16-length set name.
//   scenery (data/animations): s16 x, s16 y (top-left), s16 z (height of its base), 3 flag bytes
//   actors (data/characters):  u8 hasAlt [, alt file, alt set], 18 bytes collision boxes,
//                              s16 x, s16 y (feet), 6 bytes, u8 floor, u8 direction (0-15)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct LevelElement {
    enum Kind { Scenery, Actor, Dummy, Item } kind = Scenery;
    std::string folder;  // "characters" or "animations"
    std::string file, set;
    int x = 0, y = 0, z = 0;
    int dir = 0, floor = 0;
};

// Known sprite file stems (lower case, without .dvf) per folder.
struct SpriteIndex {
    std::unordered_set<std::string> characters, animations;
    void build();
};

std::vector<LevelElement> scanElements(const uint8_t* data, size_t size, const SpriteIndex& index);
