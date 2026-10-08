#!/usr/bin/env python3
"""Creates the header and source skeleton of one original class from the recovered layout.

    newclass.py <ClassName> [--force]

Writes engine/src/<area>/<Class>.h and .cpp (area: sb, dv, vm or misc), only if they do not exist
yet (unless --force). The header declares the bases, the fields (named f_<offset> after their
offset in the original 32-bit object) and every method of the class; the .cpp defines every method
as a stub (STUB(), logs once) so the program links. Translating a method = replacing its stub.

Needs the local reverse-engineering data (DECOMP, default /home/claude/decomp): layout.json,
hierarchy.json, vtables.json, funcs.json.
"""
import json, os, re, sys

DECOMP = os.environ.get('DECOMP', '/home/claude/decomp')
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src')

L = json.load(open(os.path.join(DECOMP, 'layout.json')))
H = json.load(open(os.path.join(DECOMP, 'hierarchy.json')))
V = json.load(open(os.path.join(DECOMP, 'vtables.json')))

# methods known to be static (no object), found while translating
STATICS = {'SBFile::OpenArchive', 'SBFile::CloseAllArchives', 'SBFile::CloseArchive', 'SBFile::AddAlternatePath',
           'SBFile::RemoveAlternatePath', 'SBFile::TryAlternatePath', 'SBFile::VirtualOpen', 'SBFile::GetValidPath',
           'SBFile::GetNumberOfArchiveEntries', 'SBFile::GetArchiveEntry', 'SBFile::Delete', 'SBFile::Exists',
           'SBFile::TranslateLastError'}

STRUCTS = {'SBDrawState', 'DVposition', 'DVmotionArea', 'DVprojectionArea', 'DVspriteScript', 'DVanimation', 'DVrawInput',
           'SBGeoPoint3D', 'VMNativeStack', 'SBUIInput', 'SBResourceWidgetID', 'DVcommand', 'RECT',
           'DVframeProgression', 'SBmemoryBlockDescriptor', 'DVcacheData', 'SBmemoryRange', 'DVdetection',
           'SBDrawSurfaceInfo', 'SBDrawViewport', 'DVpathRequest', 'DVviewParameters'}


def area(cls):
    for p in ('SB', 'DV', 'VM'):
        if cls.startswith(p):
            return p.lower()
    if cls.startswith('SC') or cls in ('ClassInformations', 'FunctionInformations', 'FunctionParameters', 'MemberVariable'):
        return 'vm'
    return 'misc'


def cpp_type(t):
    t = t.strip()
    t = re.sub(r'std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >', 'std::string', t)
    t = re.sub(r'std::__cxx11::basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t> >', 'std::wstring', t)
    t = t.replace('std::__cxx11::string', 'std::string').replace('std::__cxx11::wstring', 'std::wstring')
    t = t.replace('std::__cxx11::', 'std::')
    return t


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '<(':
            depth += 1
        elif ch in '>)':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [cpp_type(a) for a in out if a.strip() and a.strip() != 'void']


RET = {'void': 'void', 'undefined': 'void', 'bool': 'bool', 'char': 'char', 'undefined1': 'uint8_t',
       'undefined2': 'uint16_t', 'undefined4': 'int32_t', 'undefined8': 'int64_t', 'float': 'float',
       'double': 'double', 'uint': 'uint32_t', 'int': 'int', 'ushort': 'uint16_t', 'short': 'int16_t',
       'byte': 'uint8_t', 'uchar': 'uint8_t', 'ulonglong': 'uint64_t', 'longlong': 'int64_t', 'float10': 'double',
       'wchar_t': 'wchar_t'}


def ret_type(sig, cls, name):
    if not sig:
        return 'int32_t'
    m = re.match(r'(.*?)\s*(?:__thiscall\s+|__cdecl\s+)?' + re.escape(cls) + r'::', sig)
    if not m:
        m = re.match(r'(.*?)\s+\S*' + re.escape(name) + r'\s*\(', sig)
    r = (m.group(1) if m else 'undefined4').replace('__thiscall', '').replace('__cdecl', '').strip()
    if r in RET:
        return RET[r]
    if r.endswith('*'):
        base = r.rstrip('* ').strip()
        stars = r.count('*')
        if base in RET:
            b = RET[base]
            return (b if b != 'void' else 'void') + ' ' + '*' * stars
        if base in L or base in STRUCTS:
            return base + ' ' + '*' * stars
        return 'void ' + '*' * stars
    return 'int32_t'


TYPEMAP = {'undefined1': ('uint8_t', 1), 'byte': ('uint8_t', 1), 'char': ('char', 1), 'bool': ('bool', 1),
           'undefined2': ('uint16_t', 2), 'ushort': ('uint16_t', 2), 'short': ('int16_t', 2), 'wchar16': ('uint16_t', 2),
           'undefined4': ('int32_t', 4), 'int': ('int32_t', 4), 'uint': ('uint32_t', 4), 'float': ('float', 4),
           'undefined8': ('int64_t', 8), 'double': ('double', 8), 'longlong': ('int64_t', 8), 'ulonglong': ('uint64_t', 8)}


def field_type(counter):
    # prefer typed pointers to known classes, then the most common concrete type
    best = None
    for t, n in sorted(counter.items(), key=lambda kv: -kv[1]):
        if t.endswith('*'):
            base = t.rstrip('* ').strip()
            if base in L and t.count('*') == 1:
                return base + ' *', 4
    for t, n in sorted(counter.items(), key=lambda kv: -kv[1]):
        if t.endswith('*'):
            best = best or ('void *', 4)
            continue
        if t == 'byte' and 'byte' in counter and len(counter) == 1:
            return 'uint8_t', 1
        if t in TYPEMAP:
            return TYPEMAP[t]
    if best:
        return best
    if 'byte' in counter:
        return 'uint8_t', 1
    return 'int32_t', 4


def size_of(cls):
    c = L.get(cls)
    if not c:
        return None
    if c.get('size'):
        return c['size']
    m = 0
    for off, cnt in c['fields'].items():
        t, w = field_type(cnt)
        m = max(m, int(off) + w)
    for off, x in c['embedded'].items():
        s = size_of(x) or 4
        m = max(m, int(off) + s)
    return m or None


def bases_of(cls):
    return [(b, off) for b, off in H.get(cls, []) if not b.startswith('0x')]


def virtual_names(cls):
    names = set()
    for o, addr, fn in V.get(cls, []):
        if fn and '::' in fn:
            names.add(fn.split('(')[0].rsplit('::', 1)[1])
    return names


def gen(cls, force=False):
    c = L[cls]
    a = area(cls)
    os.makedirs(os.path.join(ROOT, a), exist_ok=True)
    hpath = os.path.join(ROOT, a, cls + '.h')
    cpath = os.path.join(ROOT, a, cls + '.cpp')
    if not force and (os.path.exists(hpath) or os.path.exists(cpath)):
        print('exists:', hpath)
        return
    bases = bases_of(cls)
    base_size = 0
    for b, off in bases:
        if off == 0:
            base_size = max(base_size, size_of(b) or 0)
    has_vtable = bool(V.get(cls))
    vnames = virtual_names(cls)
    refs = set()

    # ---- fields
    fields = []
    covered = set()
    emb = {int(k): v for k, v in c['embedded'].items()}
    for off in sorted(emb):
        x = emb[off]
        s = size_of(x) or 4
        if off < base_size:
            continue
        fields.append((off, x, 'f_%x' % off, 'embedded %s, %s bytes' % (x, s if s else '?')))
        refs.add(x)
        covered.update(range(off, off + s))
    lists = set()
    for off_s, cnt in sorted(c['fields'].items(), key=lambda kv: int(kv[0])):
        off = int(off_s)
        if off < base_size or off in covered:
            continue
        if off == 0 and has_vtable:
            continue
        t, w = field_type(cnt)
        others = ', '.join('%s x%d' % (k, v) for k, v in cnt.items())
        if t.endswith('*') and t.rstrip('* ').strip() in L:
            refs.add(t.rstrip('* ').strip())
        fields.append((off, t, 'f_%x' % off, others))
    fields.sort(key=lambda f: f[0])

    # ---- methods
    decls, defs, seen = [], [], set()
    for m in c['methods']:
        d = m['demangled'] or ''
        name = m['name']
        k = d.find('(')
        args = split_args(d[k + 1:d.rfind(')')]) if k >= 0 else []
        const = d.rstrip().endswith('const')
        is_ctor = name == cls.split('<')[0].split('::')[-1]
        is_dtor = name.startswith('~')
        # static: the decompiled signature has no 'this'
        # static: no 'this' in the decompiled signature and no hidden extra parameter
        gparams = 0
        if m['sig'] and '(' in m['sig']:
            inner = m['sig'][m['sig'].find('(') + 1:m['sig'].rfind(')')].strip()
            gparams = 0 if inner in ('', 'void') else len(split_args(inner))
        # Ghidra often drops an unused 'this' or merges it into a parameter, so a missing 'this' does
        # not prove a method is static; only methods listed in STATICS are made static.
        static = (cls + '::' + name) in STATICS
        key = (name, tuple(args), const)
        if key in seen:
            continue
        seen.add(key)
        for t in args:
            for w in re.findall(r'[A-Za-z_]\w*', t):
                if w in ('std', 'SBList', 'SBArray', 'SBListUnique', 'SBListAutoDelete', 'SBArrayAutoDelete', 'RECT'):
                    continue
                if w in L or w in STRUCTS or w.startswith(('DV', 'SB')):
                    refs.add(w)
        def pdecl(t, i):
            if t == '...':
                return '...'
            if '(*)' in t:
                return t.replace('(*)', '(*p%d)' % i, 1)
            return '%s p%d' % (t, i)
        params = ', '.join(pdecl(t, i + 1) for i, t in enumerate(args))
        if is_ctor:
            decls.append('    %s(%s);  // %s' % (name, params, m['addr']))
            defs.append('// %s\n%s::%s(%s) { STUB("%s::%s"); }\n' % (m['addr'], cls, name, params, cls, name))
            continue
        if is_dtor:
            v = 'virtual ' if has_vtable else ''
            decls.append('    %s~%s();  // %s' % (v, cls.split('::')[-1], m['addr']))
            defs.append('// %s\n%s::~%s() {}\n' % (m['addr'], cls, cls.split('::')[-1]))
            continue
        r = ret_type(m['sig'], cls, name)
        for w in re.findall(r'[A-Za-z_]\w*', r):
            if w in L or w in STRUCTS:
                refs.add(w)
        pre = 'static ' if static else ('virtual ' if name in vnames else '')
        decls.append('    %s%s %s(%s)%s;  // %s' % (pre, r, name, params, ' const' if const else '', m['addr']))
        if r == 'void':
            body = 'STUB("%s::%s");' % (cls, name)
        elif r.endswith('*'):
            body = 'STUB("%s::%s"); return nullptr;' % (cls, name)
        else:
            body = 'STUB("%s::%s"); return %s();' % (cls, name, r)
        defs.append('// %s\n%s %s::%s(%s)%s { %s }\n' % (m['addr'], r, cls, name, params, ' const' if const else '', body))

    # ---- write
    refs.discard(cls)
    inc = []
    for b, off in bases:
        inc.append('#include "%s/%s.h"' % (area(b), b))
    for off, t, n, note in fields:
        if t in L and not t.endswith('*'):
            inc.append('#include "%s/%s.h"' % (area(t), t))
    fwd = sorted(r for r in refs if r not in [b for b, _ in bases] and not any(t == r for _, t, _, _ in fields))
    out = []
    out.append('// %s (original class; translated from the decompiled game, see engine/TRANSLATING.md)' % cls)
    out.append('#pragma once')
    out.append('#include "sb/SBCommon.h"')
    for i in sorted(set(inc)):
        out.append(i)
    out.append('')
    for r in fwd:
        kw = 'struct' if r in STRUCTS else 'class'
        out.append('%s %s;' % (kw, r))
    out.append('')
    bl = ', '.join('public %s' % b for b, off in bases)
    out.append('class %s%s {' % (cls, (' : ' + bl) if bl else ''))
    out.append('public:')
    out += decls
    out.append('')
    if size_of(cls):
        out.append('    // original size 0x%x' % size_of(cls))
    for off, t, n, note in fields:
        out.append('    %s %s;  // +0x%x  %s' % (t, n, off, note))
    out.append('};')
    open(hpath, 'w').write('\n'.join(out) + '\n')
    src = ['// %s (see %s.h)' % (cls, cls), '#include "%s/%s.h"' % (a, cls), '']
    src += defs
    open(cpath, 'w').write('\n'.join(src))
    print('wrote', hpath, cpath)


if __name__ == '__main__':
    force = '--force' in sys.argv
    for cls in [a for a in sys.argv[1:] if not a.startswith('--')]:
        if cls not in L:
            print('unknown class', cls)
            continue
        gen(cls, force)
