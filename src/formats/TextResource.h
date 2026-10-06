// Text resources (localisation/<lang>/data/interface/texts.res).
//
//   "SRES", u32 version, u32 count; per resource: tag[4], u32 id, u32 ?, u16 n, n entries:
//     TEXT: u16 length, UTF-16LE characters      (dialogue lines, objectives, hints, tips)
//     WAVE: u16 length, ASCII path               (voice file of the matching TEXT line)
// Ids: 0x01000000 + 2*mission = dialogue text, +1 = its voice files;
//      0x01000034 + mission = objectives; 0x0100004F.. = hints; 0x01000055.. = defeat texts.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class TextResource {
public:
    bool load(const std::string& relativePath);
    const std::vector<std::string>* get(uint32_t id) const;
    std::string line(uint32_t id, int index) const;  // "" when missing

private:
    std::map<uint32_t, std::vector<std::string>> res_;  // UTF-8
};

// Texts of the current language (loaded on first use).
const TextResource& gameTexts();
// "localisation/<language>/data/interface/" of the loaded texts (voice files live below it).
const std::string& gameInterfaceFolder();
