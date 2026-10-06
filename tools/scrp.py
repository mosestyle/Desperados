"""SCRP chunk: script locations (1 point) and sectors (polygons), optionally bound to a script class.
u32 version(1), u16 count, per item: u16 n, n*(u16 x,u16 y), u16 hasLayer, u16 layer, u8 hasScript, [u16 len, name]"""
import struct
def parse_scrp(b):
    ver, n = struct.unpack_from('<IH', b, 0); o = 6; items = []
    for i in range(n):
        k = struct.unpack_from('<H', b, o)[0]; o += 2
        pts = [struct.unpack_from('<HH', b, o + 4 * j) for j in range(k)]; o += 4 * k
        a, c = struct.unpack_from('<HH', b, o); o += 4
        hs = b[o]; o += 1; name = None
        if hs:
            l = struct.unpack_from('<H', b, o)[0]; o += 2; name = b[o:o+l].decode('latin1'); o += l
        items.append(dict(pts=pts, flag=a, layer=c, script=name))
    return items, o
