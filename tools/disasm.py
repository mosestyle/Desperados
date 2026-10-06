"""Small helper to disassemble functions of desperados.exe (research only)."""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
pe = pefile.PE(sys.argv[1]); base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)
def cstr(va):
    try:
        d = pe.get_data(va - base, 80).split(b'\0')[0]
        if len(d) >= 3 and all(32 <= c < 127 for c in d): return d.decode()
    except Exception: pass
    return None
def func(va, maxn=600):
    b = pe.get_data(va - base, maxn * 8)
    out = []
    for ins in md.disasm(b, va):
        line = f'{ins.address:08x}  {ins.mnemonic:7} {ins.op_str}'
        for tok in ins.op_str.replace('[', ' ').replace(']', ' ').replace(',', ' ').split():
            if tok.startswith('0x') and len(tok) >= 8:
                s = cstr(int(tok, 16))
                if s: line += f'   ; "{s}"'
        out.append(line)
        if ins.mnemonic == 'ret' or len(out) >= maxn: break
    return out
if __name__ == '__main__':
    for a in sys.argv[2:]:
        print(f'===== {a}'); print('\n'.join(func(int(a, 16))))
