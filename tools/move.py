"""MOVE chunk (DVFastFindGrid): walkable areas per floor layer."""
import struct
class R:
    def __init__(s, b, o=0): s.b = b; s.o = o
    def u16(s): v = struct.unpack_from('<H', s.b, s.o)[0]; s.o += 2; return v
    def s16(s): v = struct.unpack_from('<h', s.b, s.o)[0]; s.o += 2; return v
    def u32(s): v = struct.unpack_from('<I', s.b, s.o)[0]; s.o += 4; return v
def poly(r): n = r.u16(); return [(r.s16(), r.s16()) for _ in range(n)]
def parse_move(m):
    r = R(m); ver = r.u32(); layers = []
    for li in range(r.u16()):
        a = r.u16(); n = r.u16(); areas = []
        for k in range(n):
            outline = poly(r)
            lines = [(r.s16(), r.s16(), r.s16(), r.s16()) for _ in range(r.u16())]
            holes = [poly(r) for _ in range(r.u16())]
            areas.append(dict(outline=outline, lines=lines, holes=holes))
        layers.append(dict(a=a, areas=areas))
    return dict(ver=ver, layers=layers, end=r.o)
