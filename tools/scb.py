"""Desperados mission script (.scb) parser + disassembler (research tool).

Layout: text header lines, then per class: text lines (fileName/className, variables, functions with
addresses), 'nbOfQuads N\n' and N binary quads of 10 bytes: u8 op, 8 operand bytes, '~'.
Operand symbols are u16: top 2 bits = storage (0 = global/level, 0x4000 = class vars,
0x8000 = parameters/volatile, 0xC000 = temporaries), low 14 bits = byte offset.
"""
import sys, struct, json, re, os

NAT = {int(k): v for k, v in json.load(open(os.path.join(os.path.dirname(__file__), 'natives.json'))).items()} \
    if os.path.exists(os.path.join(os.path.dirname(__file__), 'natives.json')) else {}

def readline(b, i):
    j = b.index(b'\n', i)
    return b[i:j].decode('latin1'), j + 1

def parse(b):
    i = 0
    l, i = readline(b, i); ver = l
    l, i = readline(b, i); ncls = int(l.split()[1])
    classes = []
    for _ in range(ncls):
        l, i = readline(b, i)
        m = re.match(r'fileName (.*) , className (\S+)', l); cname = m.group(2)
        l, i = readline(b, i); nvar, svar = map(int, re.findall(r'\d+', l))
        variables = []
        l, i = readline(b, i)
        while not l.startswith('nbOfFunctions'):
            variables.append(l); l, i = readline(b, i)
        nfun = int(l.split()[1]); funcs = []
        for _ in range(nfun):
            l, i = readline(b, i)
            m = re.match(r'functionName (\S+) , address (\d+), nbOfParams (\d+), sizeOfRetVal (\d+), sizeOfParams (\d+)', l)
            f = dict(name=m.group(1), addr=int(m.group(2)), nparams=int(m.group(3)), ret=int(m.group(4)), psize=int(m.group(5)), params=[])
            l, i = readline(b, i)  # functionParameters
            l, i = readline(b, i)
            while not l.startswith(' sizeOfVolatile'):
                if l.strip(): f['params'].append(l)
                l, i = readline(b, i)
            funcs.append(f)
        l, i = readline(b, i)
        while not l.startswith('nbOfQuads'):
            l, i = readline(b, i)
        nq = int(l.split()[1]); quads = []
        for q in range(nq):
            quads.append(b[i:i+9]); assert b[i+9] == 0x7e, (cname, q); i += 10
        classes.append(dict(name=cname, nvar=nvar, svar=svar, vars=variables, funcs=funcs, quads=quads))
    return classes

def sym(v):
    s = v & 0xC000; o = v & 0x3FFF
    return {0: 'g', 0x4000: 'c', 0x8000: 'p', 0xC000: 't'}[s] + str(o)

BIN = {0x19: '+', 0x1a: '-', 0x1b: '*', 0x1c: '/', 0x1d: '+f', 0x1e: '-f', 0x1f: '*f', 0x20: '/f',
       0x21: '<=', 0x22: '<', 0x23: '>=', 0x24: '>', 0x25: '!=', 0x26: '==',
       0x27: '<=f', 0x28: '<f', 0x29: '>=f', 0x2a: '>f', 0x2b: '!=f', 0x2c: '==f'}

def dis(q):
    op = q[0]; a, b_, c = struct.unpack_from('<HHH', q, 1); u1 = struct.unpack_from('<I', q, 1)[0]; u5 = struct.unpack_from('<I', q, 5)[0]
    if op == 1: return 'nop'
    if op == 2: return f'push {sym(a)}'
    if op == 3: return f'enter vol={a} tmp={b_}'
    if op == 4: return 'endfunc'
    if op == 5: return f'call {u1}'
    if op == 6: return 'ret'
    if op == 7: return f'ret {sym(a)}'
    if op == 8: return f'{sym(a)} = param[{u5}]'
    if op == 9: return f'setparam[{u5}] = {sym(a)}'
    if op == 10: return f'{sym(a)} = getreturn'
    if op == 11: return f'narg {sym(a)}'
    if op == 12: return f'native {u1:#x} {NAT.get(u1, ("?",))[0]}'
    if op == 13: return f'{sym(a)} = nret'
    if op == 14: return f'goto {u1}'
    if op == 15: return f'if {sym(a)} != 0 goto {u5}'
    if op == 16: return f'if {sym(a)} == 0 goto {u5}'
    if op in (17, 18): return f'{sym(a)} = {sym(b_)}'
    if op == 19: return f'{sym(a)} = {u5 if u5 < 0x80000000 else u5 - 0x100000000}'
    if op == 20: return f'{sym(a)} = {struct.unpack_from("<f", q, 5)[0]:g}f'
    if op == 21: return f'{sym(a)} = -{sym(b_)}'
    if op == 22: return f'{sym(a)} = -f {sym(b_)}'
    if op == 23: return f'{sym(a)} = int({sym(b_)})'
    if op == 24: return f'{sym(a)} = float({sym(b_)})'
    if op in BIN: return f'{sym(a)} = {sym(b_)} {BIN[op]} {sym(c)}'
    return f'?? {q.hex()}'

if __name__ == '__main__':
    cl = parse(open(sys.argv[1], 'rb').read())
    for c in cl:
        print(f'class {c["name"]} vars={c["nvar"]}/{c["svar"]}b')
        for v in c['vars']: print('   var', v)
        starts = {f['addr']: f for f in c['funcs']}
        for k, q in enumerate(c['quads']):
            if k in starts:
                f = starts[k]; print(f'  func {f["name"]}({f["nparams"]} params, {f["psize"]}b) ret {f["ret"]}', f['params'])
            print(f'    {k:5}: {dis(q)}')
