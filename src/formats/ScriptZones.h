// SCRP chunk of a level file: locations and sectors used by the mission scripts.
//
//   u32 version (1), u16 count, per item:
//     u16 n, n * (u16 x, u16 y)        1 point = a location, more = a sector (polygon)
//     u16 hasLayer, u16 layer          (meaning not fully known)
//     u8 hasScript [, u16 len, class name]   sector with EnterZone/ExitZone events
// Scripts address them by index: GetLocation(i).
#pragma once
#include <SDL.h>

#include <cstdint>
#include <string>
#include <vector>

struct ScriptZone {
    std::vector<SDL_Point> pts;
    int flag = 0, layer = 0;
    std::string script;
    bool isPoint() const { return pts.size() == 1; }
    bool contains(float x, float y) const;
    SDL_FPoint centre() const;
};

bool parseScriptZones(const uint8_t* data, size_t size, std::vector<ScriptZone>& out);
