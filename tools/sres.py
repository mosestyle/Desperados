"""SRES resource files (texts.res): 'SRES', u32 version, u32 count; per resource: tag[4], u32 id, body.
TEXT body: u32 ?, u16 n, n * (u16 len, UTF-16LE chars).  WAVE body: u32 ?, u16 n, n * (u16 len, ASCII path)."""
import struct
def parse_texts(b):
    assert b[:4] == b'SRES'
    n = struct.unpack_from('<I', b, 8)[0]; o = 12; res = []
    for _ in range(n):
        tag = b[o:o+4].decode(); rid = struct.unpack_from('<I', b, o + 4)[0]; o += 8
        u, cnt = struct.unpack_from('<IH', b, o); o += 6; ent = []
        for _ in range(cnt):
            l = struct.unpack_from('<H', b, o)[0]; o += 2
            if tag == 'TEXT': ent.append(b[o:o+2*l].decode('utf-16-le')); o += 2 * l
            else: ent.append(b[o:o+l].decode('latin1')); o += l
        res.append((tag, rid, u, ent))
    return res, o
