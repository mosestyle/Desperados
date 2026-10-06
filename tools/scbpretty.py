"""Pretty-print .scb scripts: folds constant loads, native args and returns into call expressions."""
import sys, struct, json, os
sys.path.insert(0, os.path.dirname(__file__)); import scb
NARGS = {int(k): v for k, v in json.load(open(os.path.join(os.path.dirname(__file__), 'native_nargs.json'))).items()}
def pretty(c):
    out = []; starts = {f['addr']: f for f in c['funcs']}; val = {}; args = []; pend = None
    jt = set()
    for q in c['quads']:
        if q[0] in (14,): jt.add(struct.unpack_from('<I', q, 1)[0])
        if q[0] in (15, 16): jt.add(struct.unpack_from('<I', q, 5)[0])
    for k, q in enumerate(c['quads']):
        if k in starts: f = starts[k]; out.append(f'\n  {f["name"]}():'); val = {}
        if k in jt: out.append(f'   L{k}:'); val = {}
        op = q[0]; a, b, cc = struct.unpack_from('<HHH', q, 1); u1 = struct.unpack_from('<I', q, 1)[0]; u5 = struct.unpack_from('<I', q, 5)[0]
        S = lambda s: val.get(s, scb.sym(s))
        if op == 19: val[a] = str(u5 if u5 < 0x80000000 else u5 - 0x100000000); continue
        if op == 20: val[a] = f'{struct.unpack_from("<f", q, 5)[0]:g}f'; continue
        if op == 8: val[a] = f'arg{u5//4}'; continue
        if op == 21: val[a] = '-' + S(b); continue
        if op == 11: args.append(S(a)); continue
        if op == 12:
            nm = scb.NAT.get(u1, ('?',))[0][1:]
            pend = f'{nm}({", ".join(args)})'; args = []
            nq = c['quads'][k+1] if k + 1 < len(c['quads']) else b''
            if nq and nq[0] == 13: continue
            out.append('     ' + pend); pend = None; continue
        if op == 13: val[a] = pend; pend = None; continue
        if op in (17, 18): val[a] = S(b) if (b & 0xC000) == 0xC000 else scb.sym(b)
        if op in (17, 18) and (a & 0xC000) == 0xC000: continue
        if op in scb.BIN and (a & 0xC000) == 0xC000: val[a] = f'({S(b)} {scb.BIN[op]} {S(cc)})'; continue
        if op == 15: out.append(f'     if {S(a)}: goto L{u5}'); continue
        if op == 16: out.append(f'     if not {S(a)}: goto L{u5}'); continue
        if op == 7: out.append(f'     return {S(a)}'); continue
        if op in (17, 18): out.append(f'     {scb.sym(a)} = {S(b)}'); continue
        if op in (1, 3, 4): continue
        if op == 6: out.append('     return'); continue
        out.append('     ' + scb.dis(q))
    return out
if __name__ == '__main__':
    for c in scb.parse(open(sys.argv[1], 'rb').read()):
        if len(sys.argv) > 2 and sys.argv[2] not in c['name']: continue
        print(f'class {c["name"]}'); print('\n'.join(pretty(c)))
