#!/usr/bin/env python3
"""Make glibc (ARM64) usable inside an Android app process.

Android puts every app process under a seccomp filter. glibc calls set_robust_list(2) while
the loader starts and again for every new thread; Android's filter doesn't allow it, so the
kernel kills the process with SIGSYS before anything runs (the app saw the game process end
with status 0x1f = signal 31).

glibc already copes with kernels that don't have set_robust_list (it then just doesn't use
robust mutexes), so each such system call is replaced by "x0 = -ENOSYS":
    mov x8, #99 (set_robust_list) ... svc #0   ->   mov x8, #99 ... mov x0, #-38

Usage: patch_glibc.py file...   (patches in place, fails if a file has nothing to patch and
is one that should: the loader and libc)
"""
import struct
import sys

MOV_X8_SET_ROBUST_LIST = 0xD2800C68  # movz x8, #99
SVC_0 = 0xD4000001                   # svc #0
MOV_X0_ENOSYS = 0x928004A0           # movn x0, #37  (x0 = -38 = -ENOSYS)


def patch(data: bytearray) -> int:
    count = 0
    for off in range(0, len(data) - 4, 4):
        if struct.unpack_from("<I", data, off)[0] != MOV_X8_SET_ROBUST_LIST:
            continue
        # the svc follows within a few instructions (the arguments are set up in between)
        for k in range(1, 8):
            o = off + 4 * k
            if o + 4 > len(data):
                break
            insn = struct.unpack_from("<I", data, o)[0]
            if insn == SVC_0:
                struct.pack_into("<I", data, o, MOV_X0_ENOSYS)
                count += 1
                break
            # stop if x8 is written again before the svc
            if (insn & 0x1F) == 8 and (insn >> 24) in (0xD2, 0x92, 0xF2, 0xAA, 0x91, 0xF9):
                break
    return count


def main() -> int:
    ok = True
    for name in sys.argv[1:]:
        with open(name, "rb") as f:
            data = bytearray(f.read())
        n = patch(data)
        with open(name, "wb") as f:
            f.write(data)
        print(f"{name}: {n} set_robust_list call(s) patched")
        base = name.rsplit("/", 1)[-1]
        if n == 0 and ("ld" in base or base.endswith("_c.so") or base.startswith("libc.")):
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
