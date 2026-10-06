#include "Dialogues.h"

#include "ByteReader.h"

bool Dialogues::parse(const uint8_t* data, size_t size) {
    ByteReader r(data, size);
    if (r.u32() != 4) return false;
    textId = r.u32();
    waveId = r.u32();
    uint32_t n = r.u32();
    if (n > 1000) return false;
    dialogues.resize(n);
    for (auto& d : dialogues) {
        uint32_t lines = r.u32();
        if (!r.ok || lines > 1000) return false;
        d.resize(lines);
        for (auto& l : d) {
            l.speaker = (int)r.u32();
            l.text = (int)r.u32();
            l.wave = (int)r.u32();
        }
    }
    auto list = [&](uint32_t& id, std::vector<int>& out) {
        id = r.u32();
        uint32_t k = r.u32();
        for (uint32_t i = 0; i < k && r.ok && i < 1000; ++i) out.push_back((int)r.u32());
    };
    list(hintId, hints);
    list(objectiveId, objectives);
    // defeat texts: id, title count + titles, text count + texts
    defeatId = r.u32();
    uint32_t titles = r.u32();
    for (uint32_t i = 0; i < titles && r.ok && i < 100; ++i) r.u32();
    uint32_t k = r.u32();
    for (uint32_t i = 0; i < k && r.ok && i < 100; ++i) defeatTexts.push_back((int)r.u32());
    if (!r.ok) defeatTexts.clear();
    return true;
}
