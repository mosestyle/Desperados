#include "dv/DVHikingGuide.h"

#include "dv/DVFastFindGrid.h"
#include "sb/SBFile.h"

DVHikingGuide* DVHikingGuide::mpInstance = nullptr;

DVHikingGuide::DVHikingGuide() { mpInstance = this; }
DVHikingGuide::~DVHikingGuide() {
    if (mpInstance == this) mpInstance = nullptr;
}

DVposition DVWaypoint::Position() const {
    DVposition p;
    p.p = SBGeoPoint2D((float)x, (float)y);
    p.sector = sector;
    p.layer = (int16_t)layer;
    return p;
}

// DVWaypoint::LoadDataFromFile: s16 x, s16 y, the sector (DVFastFindGrid::SerializeSectorPointer:
// its number, 0xffff = none), u16 layer, u8 "is a script", u16 length, then the macro data or the
// script's name (scripts come with the script engine; the name is kept)
int DVWaypoint::LoadDataFromFile(SBFile& f) {
    x = f.S16();
    y = f.S16();
    uint16_t s = f.U16();
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    sector = nullptr;
    if (s != 0xffff) {
        if (!grid || s >= grid->SectorCount())
            SBError(true, "DVFastFindGrid.cpp", 0x228f, "VERBOTEN : Serializing unknown sector.");
        else
            sector = grid->Sector(s);
    }
    layer = f.U16();
    isScript = f.U8() != 0;
    dataLen = f.U16();
    int n = 0xb;
    if (dataLen != 0) {
        if (!isScript) {
            data.resize(dataLen);
            f.Serialize(data.data(), dataLen);
        } else {
            script.resize(dataLen);
            f.Serialize(&script[0], dataLen);
        }
        n += dataLen;
    }
    return n;
}

int DVHikingGuide::LoadNewPathFromFile(SBFile& f) {
    auto list = std::make_unique<DVWaypointList>();
    uint16_t count = f.U16();
    int n = 2;
    for (uint16_t i = 0; i < count; ++i) {
        auto w = std::make_unique<DVWaypoint>();
        n += w->LoadDataFromFile(f);
        list->push_back(std::move(w));
    }
    paths.push_back(std::move(list));
    return n;
}

int DVHikingGuide::LoadAllPathesFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 1)
        SBError(true, "DVHikingGuide.cpp", 0x61, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk Ways", f.version, 1);
    paths.clear();
    uint16_t count = f.U16();
    int n = 6;
    for (uint16_t i = 0; i < count; ++i) n += LoadNewPathFromFile(f);
    return n;
}

int DVPath::LoadFromFile(SBFile& f) {
    index = f.U16();
    waypoints = index == 0xffff || !DVHikingGuide::mpInstance ? nullptr : DVHikingGuide::mpInstance->Path(index);
    return 2;
}

// a route of more than one waypoint is walked forward to its end, then backward
void DVPath::Next() {
    previous = current;
    size_t n = Count();
    if (n < 2) return;
    if (forward) {
        if ((int)current < (int)n - 1) {
            current++;
            return;
        }
        current--;
        forward = false;
        return;
    }
    if (current != 0) {
        current--;
        return;
    }
    current = 1;
    forward = true;
}

void DVPath::Back() {
    bool f = !forward;
    forward = f;
    previous = current;
    size_t n = Count();
    if (n >= 2) {
        if (!forward) {
            if (current == 0) {
                current = 1;
                f = true;
            } else {
                current--;
            }
        } else if ((int)current < (int)n - 1) {
            current++;
        } else {
            current--;
            f = false;
        }
    }
    forward = !f;
}

// DVdirectionFlag: 1 = only when walking forward, 2 = only backward, else both
bool DVPath::HasRightDirection(int flag) const {
    if (flag == 2 && forward) return false;
    return flag != 1 || forward;
}
