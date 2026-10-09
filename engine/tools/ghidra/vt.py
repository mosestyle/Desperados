#!/usr/bin/env python3
# vt.py ADDR COUNT: print vtable entries with symbol names (from game.dis function labels)
import sys, struct, re
addr=int(sys.argv[1],16); n=int(sys.argv[2])
elf=open('/mnt/user-data/uploads/Desperados/desperados32','rb').read()
# section headers to map vaddr->offset
e_shoff=struct.unpack_from('<I',elf,0x20)[0]; e_shentsize,e_shnum=struct.unpack_from('<HH',elf,0x2e)
secs=[]
for i in range(e_shnum):
    sh=struct.unpack_from('<IIIIIIIIII',elf,e_shoff+i*e_shentsize)
    secs.append((sh[3],sh[4],sh[5]))
def off(v):
    for a,o,s in secs:
        if a and a<=v<a+s: return o+v-a
names={}
for line in open('/root/gh/game.dis'):
    m=re.match(r'^([0-9a-f]+) <(.*)>:$',line)
    if m: names[int(m.group(1),16)]=m.group(2)
for i in range(n):
    v=struct.unpack_from('<I',elf,off(addr+4*i))[0]
    print(hex(4*i), hex(v), names.get(v,'?'))
