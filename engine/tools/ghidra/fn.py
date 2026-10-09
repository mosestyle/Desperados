#!/usr/bin/env python3
# fn.py REGEX [all.c]: print every decompiled function whose name matches REGEX
import re, sys
pat = re.compile(sys.argv[1])
src = sys.argv[2] if len(sys.argv) > 2 else '/root/gh/out3/all.c'
out = False
for line in open(src, errors='replace'):
    if line.startswith('// ==== '):
        out = bool(pat.search(line[8:]))
    if out:
        sys.stdout.write(line)
