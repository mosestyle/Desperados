"""MASK chunk: foreground cut-outs that hide characters standing behind scenery."""
import struct

def parse_masks(m):
    ver, ngroups = struct.unpack_from('<IH', m, 0); o = 6
    masks = []
    for g in range(ngroups):              # group 0: outdoor scenery, others: probably buildings/floors
      n, = struct.unpack_from('<H', m, o); o += 2
      for i in range(n):
        flags = m[o]; o += 1
        polys = []
        for k in range(2):                 # flag bit 0: sort line, bit 1: second line
            if not flags & (1 << k): polys.append([]); continue
            c, = struct.unpack_from('<H', m, o); o += 2
            pts = [struct.unpack_from('<hh', m, o + 4 * j) for j in range(c)]; o += 4 * c
            polys.append(pts)
        height = 0
        if flags & 0x10:                   # bit 4: extra height value
            height, = struct.unpack_from('<h', m, o); o += 2
        x, y, w, h, size = struct.unpack_from('<hhhhH', m, o); o += 10
        data = m[o:o+size]; o += size
        masks.append(dict(group=g, flags=flags, height=height, line=polys[0], line2=polys[1], rect=(x, y, w, h), rle=data))
    return dict(ver=ver, groups=ngroups, masks=masks, end=o, size=len(m))

def decode_bits(mk):
    """-> list of rows, each a bytes object of ceil(w/8) bytes (MSB = leftmost pixel)."""
    x, y, w, h = mk['rect']; d = mk['rle']; p = 0; rows = []
    stride = (w + 7) // 8
    for r in range(h):
        n = d[p]; p += 1; end = p + n; row = bytearray()
        while p < end:
            c = d[p]; p += 1
            if c >= 0x80: row += bytes([d[p]]) * (c - 0x80); p += 1
            else: row += d[p:p+c]; p += c
        rows.append(bytes(row))
    return rows, p
