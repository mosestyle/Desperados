#include "Paths.h"

#include "ByteReader.h"

static void parseCommands(const uint8_t* d, size_t n, std::vector<WayCommand>& out) {
    ByteReader r(d, n);
    while (r.remaining() > 0) {
        uint8_t op = r.u8();
        switch (op) {
        case 0x00: return;
        case 0x02: { WayCommand c{WayCommand::Face}; c.a = r.u16(); if (r.ok) out.push_back(c); break; }
        case 0x05: { WayCommand c{WayCommand::LookAt}; c.a = r.s16(); c.b = r.s16(); if (r.ok) out.push_back(c); break; }
        case 0x07: { WayCommand c{WayCommand::Wait}; c.a = r.u16(); if (r.ok) out.push_back(c); break; }
        case 0x81: case 0x82: r.skip(4); break;
        case 0x83: r.skip(2); break;
        default: return;  // unknown command: stop reading this block
        }
        if (!r.ok) return;
    }
}

static void parseOrders(const uint8_t* d, size_t n, Waypoint& wp) {
    ByteReader r(d, n);
    r.u16();
    wp.mode = r.u8();
    r.u16();
    int options = r.u16();
    if (!r.ok || options <= 0 || options > 16) return;
    wp.options.resize(options);
    for (auto& o : wp.options) { o.probability = r.u8(); r.u16(); }
    for (auto& o : wp.options) {
        size_t len = r.u16();
        if (!r.need(len)) return;
        parseCommands(r.here(), len, o.commands);
        r.skip(len);
    }
}

bool parsePaths(const uint8_t* data, size_t size, std::vector<PatrolPath>& out) {
    ByteReader r(data, size);
    if (r.u32() != 1) return false;
    int paths = r.u16();
    out.resize(paths);
    for (auto& path : out) {
        int n = r.u16();
        for (int i = 0; i < n && r.ok; ++i) {
            Waypoint wp;
            wp.x = r.s16();
            wp.y = r.s16();
            r.u16();
            r.u16();
            uint8_t isScript = r.u8();
            size_t len = r.u16();
            if (!r.need(len)) return false;
            if (isScript) wp.script.assign((const char*)r.here(), len);
            else parseOrders(r.here(), len, wp);
            r.skip(len);
            path.push_back(std::move(wp));
        }
    }
    return r.ok;
}
