#include "LevelElements.h"

#include <algorithm>
#include <cctype>

#include "../core/FileSystem.h"
#include "ByteReader.h"

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

void SpriteIndex::build() {
    auto add = [](const std::string& folder, std::unordered_set<std::string>& out) {
        for (const auto& name : fs_::listDir(folder)) {
            std::string l = lower(name);
            if (l.size() > 4 && l.compare(l.size() - 4, 4, ".dvf") == 0) out.insert(l.substr(0, l.size() - 4));
        }
    };
    add("data/characters", characters);
    add("data/animations", animations);
}

// Reads a u16-length printable string at `o`; returns false if it doesn't look like one.
static bool readName(const uint8_t* d, size_t size, size_t o, std::string& out, size_t& next) {
    if (o + 2 > size) return false;
    size_t n = d[o] | (d[o + 1] << 8);
    if (n == 0 || n > 64 || o + 2 + n > size) return false;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = d[o + 2 + i];
        if (c < 32 || (c >= 127 && c < 0xC0)) return false;
    }
    out.assign((const char*)d + o + 2, n);
    next = o + 2 + n;
    return true;
}

static Faction factionOf(int cls, int sub) {
    if (cls == 0) return Faction::Hero;
    if (cls == 1) return sub == 1 ? Faction::Enemy : Faction::Civilian;
    if (cls == 2) return Faction::Animal;
    return Faction::None;
}

std::vector<LevelElement> scanElements(const uint8_t* d, size_t size, const SpriteIndex& index) {
    std::vector<LevelElement> out;
    size_t o = 0;
    while (o + 6 < size) {
        std::string file, set;
        size_t afterFile, afterSet;
        if (!readName(d, size, o, file, afterFile)) { ++o; continue; }
        const std::string key = lower(file);
        const bool isChar = index.characters.count(key) != 0, isAnim = index.animations.count(key) != 0;
        if ((!isChar && !isAnim) || !readName(d, size, afterFile, set, afterSet)) { ++o; continue; }

        LevelElement e;
        e.offset = o;
        e.file = file;
        e.set = set;
        if (o >= 2) { e.sub = d[o - 2]; e.cls = d[o - 1]; }
        // Decide how to read the element: by class byte when it is a known one, else by folder.
        bool asActor;
        if (e.cls == 0 || e.cls == 1 || e.cls == 2) asActor = true;
        else if (e.cls == 8 || e.cls == 0x10) asActor = false;
        else if (e.cls == 0x11) asActor = false;
        else asActor = isChar && !isAnim;

        ByteReader r(d, size);
        if (key == "accessories" || e.cls == 0x11) {
            e.kind = LevelElement::Item;
            e.folder = "characters";
            out.push_back(e);
            o = afterSet;
            continue;
        }
        if (!asActor) {
            e.kind = key == "zombie" ? LevelElement::Dummy : LevelElement::Scenery;
            e.folder = isAnim ? "animations" : "characters";
            r.pos = afterSet;
            e.x = r.s16();
            e.y = r.s16();
            e.z = r.s16();
            if (e.cls == 8) {  // scripted object: 3 flag bytes, then the bottom-right of its hit box
                r.skip(3);
                e.boxX1 = r.s16();
                e.boxY1 = r.s16();
            }
            if (!r.ok) break;
            out.push_back(e);
            o = afterSet;
            continue;
        }
        e.kind = key == "zombie" ? LevelElement::Dummy : LevelElement::Actor;
        e.folder = isChar ? "characters" : "animations";
        e.faction = factionOf(e.cls, e.sub);
        size_t p = afterSet + 1;
        if (afterSet < size && d[afterSet] == 1) {  // alternative sprite follows
            std::string altFile, altSet;
            size_t a1, a2;
            if (readName(d, size, p, altFile, a1) && readName(d, size, a1, altSet, a2)) p = a2;
        }
        r.pos = p + 18;
        e.x = r.s16();
        e.y = r.s16();
        int hasStartAnim = r.u16();
        int startAnim = r.u16();
        r.u16();
        e.floor = r.u8();  // meaning unknown (not the floor); the floor is taken from the motion areas
        e.dir = r.u8() & 15;
        if (hasStartAnim == 1) e.startAnim = startAnim;
        if (e.cls == 1) {  // NPC: script class, profile, ..., patrol path
            size_t len = r.u16();
            if (len < 64 && r.need(len)) { e.script.assign((const char*)r.here(), len); r.skip(len); }
            e.profile = r.u16();
            r.u16();
            r.u16();
            r.u16();
            e.path = r.s16();
            if (!r.ok) { e.path = -1; r.ok = true; }
        }
        if (!r.ok) break;
        out.push_back(e);
        o = p;
    }
    return out;
}
