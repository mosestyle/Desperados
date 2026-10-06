// Character / object sprite files (.dvf).
//
//   u16 version (0x200), u16 frameCount, u16 0, u16 maxW, u16 maxH, pad to 0x1E
//   frameCount x frame:  u32 size, u16 w, u16 h, u16 1, then h rows of
//                        s16 x0, s16 x1 (inclusive, (0,-1) = empty row), (x1-x0+1) RGB565 pixels
//                        pixel 0x001F = transparent, 0x07C0 = shadow
//   u16 setCount, then per set:
//     char name[32], u16 dirCount, 32 bytes ?, u32 animCount, 14 bytes ?,
//     u16 maxW, u16 maxH, u32 anchorX, u32 anchorY, 20 bytes ?
//     animCount*dirCount records:
//       u32 ?, u16 entryCount, u16 keyEntry, u16 flags, u32 anchorX, u32 anchorY,
//       u16 dir, u16 animId, char name[32],
//       entryCount x { u16 frame, u16 duration (ticks @25Hz), s16 step (px moved),
//                      s16 x, s16 y (frame top-left inside the anchor box), u32 event (sound) }
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct SpriteEntry {
    uint16_t frame = 0, duration = 1;
    int16_t step = 0, x = 0, y = 0;
    uint32_t event = 0;
};

struct SpriteRecord {
    uint16_t anim = 0, dir = 0, keyEntry = 0, flags = 0;
    int32_t anchorX = 0, anchorY = 0;
    std::string name;
    std::vector<SpriteEntry> entries;
};

struct SpriteSet {
    std::string name;
    int dirCount = 1, animCount = 0;
    int anchorX = 0, anchorY = 0;
    std::vector<SpriteRecord> records;
    // Record for (anim, dir); falls back to dir 0, then to the first record.
    const SpriteRecord* find(int anim, int dir) const;
};

class SpriteFile {
public:
    struct Frame { uint32_t offset = 0; uint16_t w = 0, h = 0; };

    bool load(const std::string& absolutePath);
    bool parse(std::vector<uint8_t> bytes);
    const SpriteSet* set(const std::string& name) const;  // case-insensitive
    const std::vector<SpriteSet>& sets() const { return sets_; }
    const std::vector<Frame>& frames() const { return frames_; }
    // Decodes a frame into ARGB8888 (w*h pixels).
    bool decode(int frame, std::vector<uint32_t>& argb) const;
    int id() const { return id_; }
    void setId(int id) { id_ = id; }

private:
    std::vector<uint8_t> data_;
    std::vector<Frame> frames_;
    std::vector<SpriteSet> sets_;
    int id_ = 0;
};
