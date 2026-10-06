"""Linear disassembly of a range of desperados.exe (research only): dis2.py exe start end"""
import sys, pefile, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def cstr(va):
    o = va - base
    if not (0 <= o < len(img)): return None
    s = img[o:o+80].split(b'\0')[0]
    if len(s) >= 3 and all(32 <= c < 127 or c in (9, 10) for c in s): return s.decode()
def dis(a, b):
    out = []
    for ins in md.disasm(img[a-base:b-base], a):
        line = f'{ins.address:08x}  {ins.mnemonic:7} {ins.op_str}'
        for tok in ins.op_str.replace('[', ' ').replace(']', ' ').replace(',', ' ').split():
            if tok.startswith('0x') and len(tok) >= 8:
                s = cstr(int(tok, 16))
                if s: line += f'   ; "{s!s:.60}"'
        out.append(line)
    return out
if __name__ == '__main__':
    print('\n'.join(dis(int(sys.argv[2], 16), int(sys.argv[3], 16))))
