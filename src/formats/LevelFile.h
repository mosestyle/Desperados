// Level description files (data/levels/level_XX.dvd).
//
// The file is a sequence of chunks:  char tag[4]; u32 length; u8 body[length]
// Known tags (in file order): MISC BGND MOVE SGHT MASK WAYS ELEM FXBK MSIC "SND "
// "PAT " BOND "MAT " LIFT "AI  " BUIL SCRP JUMP CART DLGS
// Only BGND (minimap) is decoded so far; the rest is kept as raw bytes.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Image16.h"

struct LevelFile {
    std::string fileName;
    std::vector<uint8_t> bytes;
    struct Chunk { size_t offset = 0, size = 0; };
    std::map<std::string, Chunk> chunks;  // key: tag with trailing spaces trimmed
    std::vector<std::string> order;

    bool load(const std::string& relativePath);
    bool has(const std::string& tag) const { return chunks.count(tag) != 0; }
    const uint8_t* chunkData(const std::string& tag, size_t* size) const;

    // BGND: u32 version, pstr16 levelName, u16 w, u16 h, u32 codec, u32 csize, data
    bool readMinimap(std::string& levelName, Image16& minimap) const;
};
