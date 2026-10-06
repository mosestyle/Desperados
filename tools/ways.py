"""WAYS chunk (DVHikingGuide): patrol paths made of waypoints."""
import struct
def parse_ways(w):
    o = 0
    ver, = struct.unpack_from('<I', w, o); o += 4
    n, = struct.unpack_from('<H', w, o); o += 2
    paths = []
    for p in range(n):
        c, = struct.unpack_from('<H', w, o); o += 2
        wps = []
        for i in range(c):
            x, y, sector, a, b, l = struct.unpack_from('<hhHHBH', w, o); o += 11
            data = w[o:o+l]; o += l
            wps.append(dict(x=x, y=y, sector=sector, a=a, script=b, data=data))
        paths.append(wps)
    return dict(ver=ver, paths=paths, end=o, size=len(w))
