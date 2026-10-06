"""BUIL chunk: buildings (abstract interiors entered through doors) and stand-alone doors.
u32 version 4, u16 nBuildings, per building: u16 (ignored), u16 n + n*u16 sectors, u16 nDoors + doors;
u16 nDoors + stand-alone doors. Door: u8 type, u8 f4, u8 lockPC, u8 f8, u8 lockVillain, u8 lockCivilian,
u8 c[4], u16 n + n*(s16 x,s16 y) outline, u16 3, 3*(s16 x, s16 y, u16 sector, u16 layer) (outside,
middle, inside), s16 element (-1 none) [+ u8, u8]."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__)); import elem
def door(b, o):
    t, f4, lp, f8, lv, lc = b[o:o+6]; c = b[o+6:o+10]; o += 10
    n = struct.unpack_from('<H', b, o)[0]; o += 2
    out = [struct.unpack_from('<hh', b, o+4*i) for i in range(n)]; o += 4*n
    k = struct.unpack_from('<H', b, o)[0]; o += 2
    pts = [struct.unpack_from('<hhHH', b, o+8*i) for i in range(k)]; o += 8*k
    e = struct.unpack_from('<h', b, o)[0]; o += 2
    extra = None
    if e != -1: extra = (b[o], b[o+1]); o += 2
    return dict(type=t, f4=f4, lockPC=lp, f8=f8, lockV=lv, lockC=lc, c=c.hex(), outline=out, pts=pts, elem=e, extra=extra), o
def parse(b):
    ver, n = struct.unpack_from('<IH', b, 0); o = 6; bs = []
    for i in range(n):
        a, k = struct.unpack_from('<HH', b, o); o += 4
        secs = list(struct.unpack_from('<%dH' % k, b, o)); o += 2*k
        nd = struct.unpack_from('<H', b, o)[0]; o += 2; ds = []
        for j in range(nd): d, o = door(b, o); ds.append(d)
        bs.append(dict(a=a, sectors=secs, doors=ds))
    nd = struct.unpack_from('<H', b, o)[0]; o += 2; free = []
    for j in range(nd): d, o = door(b, o); free.append(d)
    return bs, free, o
if __name__ == '__main__':
    G = sys.argv[1]
    for l in range(1, 26):
        b = elem.chunks(open(G + '/data/levels/level_%02d.dvd' % l, 'rb').read())['BUIL']
        bs, free, o = parse(b)
        print('level', l, 'buildings', len(bs), 'doors', sum(len(x['doors']) for x in bs), 'free', len(free), 'ok' if o == len(b) else 'LEFT %d' % (len(b)-o))
        if len(sys.argv) > 2 and int(sys.argv[2]) == l:
            for i, x in enumerate(bs):
                print(' B%d a=%d sectors=%s' % (i, x['a'], x['sectors']))
                for d in x['doors']: print('   ', {k: v for k, v in d.items() if k != 'outline'})
            for d in free: print('  free', {k: v for k, v in d.items() if k != 'outline'})
