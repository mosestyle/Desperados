"""SGHT chunk: 3D sight obstacles (walls, rocks...) that block enemies' view."""
import struct
def parse_sght(m):
    o = 0; ver, n = struct.unpack_from('<IH', m, o); o += 6; obs = []
    for i in range(n):
        c, = struct.unpack_from('<H', m, o); o += 2
        pts = [struct.unpack_from('<4f', m, o + 16 * j) for j in range(c)]; o += 16 * c
        six = struct.unpack_from('<6f', m, o); o += 24
        link = m[o]; o += 1
        lk = None
        if link: lk = struct.unpack_from('<HH', m, o); o += 4
        fa, fb, fc, fd = m[o], m[o+1], m[o+2], m[o+3]; o += 4
        f1, f2 = struct.unpack_from('<2f', m, o); o += 8
        b2 = m[o]; o += 1
        f3, = struct.unpack_from('<f', m, o); o += 4
        obs.append(dict(pts=pts, six=six, link=lk, flags=(fa, fb, fc, fd), f=(f1, f2, f3), b2=b2))
    return dict(ver=ver, obstacles=obs, end=o, size=len(m))
