"""Heuristic scanner for the ELEM chunk of .dvd level files (research tool)."""
import struct, os, re

def chunks(b):
    i = 0; ch = {}
    while i + 8 <= len(b):
        t = b[i:i+4].decode('latin1').strip(); n = struct.unpack_from('<I', b, i + 4)[0]
        ch[t] = b[i+8:i+8+n]; i += 8 + n
    return ch

def pstr(e, o):
    if o + 2 > len(e): return None, o
    n = struct.unpack_from('<H', e, o)[0]
    if n == 0 or n > 64 or o + 2 + n > len(e): return None, o
    s = e[o+2:o+2+n]
    if not all(32 <= c < 127 or c >= 0xC0 for c in s): return None, o
    return s.decode('latin1'), o + 2 + n

def scan_elements(e, dvf_index):
    """dvf_index: lower-case file stem -> (folder, set-name set). Returns list of dicts."""
    out = []; o = 0
    while o < len(e) - 6:
        f, o2 = pstr(e, o)
        if f and f.lower() in dvf_index:
            s, o3 = pstr(e, o2)
            folder, sets = dvf_index[f.lower()]
            if s and s.lower() in sets:
                el = dict(file=f, set=s, folder=folder, at=o)
                if f.lower() == 'accessories':
                    el['kind'] = 'item'
                elif folder == 'animations':
                    x, y, z = struct.unpack_from('<hhh', e, o3)
                    el.update(kind='object', x=x, y=y, z=z, flags=e[o3+6:o3+9].hex())
                else:
                    p = o3 + 1
                    if e[o3] == 1:                      # alternative sprite (file, set) follows
                        af, p = pstr(e, p); as_, p = pstr(e, p)
                        el['alt'] = (af, as_)
                    x, y = struct.unpack_from('<hh', e, p + 18)
                    tail = e[p+22:p+22+8]
                    el.update(kind='actor', x=x, y=y, posture=tail[6], dir=tail[7] & 15)
                    if f.lower() == 'zombie': el['kind'] = 'dummy'
                    o = p; out.append(el); continue
                out.append(el); o = o3; continue
        o += 1
    return out
