"""Desperados .dvf sprite file parser (research tool)."""
import struct

def parse_dvf(b):
    ver, nf, _z, maxw, maxh = struct.unpack_from('<HHHHH', b, 0)
    off = 0x1e
    frames = []
    for i in range(nf):
        size, w, h, unk = struct.unpack_from('<IHHH', b, off)
        frames.append((off + 10, w, h, size))
        off += 10 + size
    nsets, = struct.unpack_from('<H', b, off); off += 2
    sets = []
    for s in range(nsets):
        name = b[off:off+32].split(b'\0')[0].decode('latin1'); off += 32
        ndirs, = struct.unpack_from('<H', b, off); off += 2
        off += 32                       # unknown, zero so far
        nanims, = struct.unpack_from('<I', b, off); off += 4
        off += 14                       # unknown, zero so far
        mw, mh, ax, ay = struct.unpack_from('<HHII', b, off); off += 12
        off += 20                       # unknown, zero so far
        records = []
        for r in range(nanims * ndirs):
            pre, = struct.unpack_from('<I', b, off); off += 4
            n, a, flags, rx, ry, d, anim = struct.unpack_from('<HHHIIHH', b, off); off += 18
            rname = b[off:off+32].split(b'\0')[0].decode('latin1').strip(); off += 32
            entries = []
            for e in range(n):
                fr, dur, e3, x, y, e6 = struct.unpack_from('<HHHhhI', b, off); off += 14
                entries.append((fr, dur, e3, x, y, e6))
            tail = pre
            records.append(dict(name=rname, dir=d, anim=anim, loop=a, flags=flags, anchor=(rx, ry), entries=entries, tail=tail))
        sets.append(dict(name=name, ndirs=ndirs, nanims=nanims, maxw=mw, maxh=mh, anchor=(ax, ay), records=records))
    return dict(ver=ver, frames=frames, maxw=maxw, maxh=maxh, sets=sets, end=off, size=len(b))

def frame_rgba(b, frame):
    """Decode one frame to an RGBA numpy array. Blue 0x001F = transparent, green 0x07C0 = shadow."""
    import numpy as np
    off, w, h, size = frame
    p = off
    img = np.zeros((h, w, 4), np.uint8)
    for y in range(h):
        x0, x1 = struct.unpack_from('<hh', b, p); p += 4
        n = x1 - x0 + 1
        if n <= 0: continue          # empty row is stored as (0, -1)
        px = np.frombuffer(b, '<u2', n, p).astype(np.uint32); p += 2 * n
        R = ((px >> 11) & 31) * 255 // 31; G = ((px >> 5) & 63) * 255 // 63; B = (px & 31) * 255 // 31
        A = np.full(n, 255, np.uint32)
        A[px == 0x001F] = 0
        sh = px == 0x07C0
        R[sh] = 0; G[sh] = 0; B[sh] = 0; A[sh] = 110
        img[y, x0:x1 + 1] = np.stack([R, G, B, A], 1)
    return img
