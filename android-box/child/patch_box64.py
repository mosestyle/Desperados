#!/usr/bin/env python3
"""Fill gaps in Box32's SDL2 wrapper that Desperados needs.

The game imports three SDL2 functions Box32 doesn't provide (checked against all of the game's
imports): SDL_RWFromConstMem and SDL_LoadWAV_RW (sound effects loaded from memory, the first
time a menu sound plays) and SDL_GetAudioDeviceName. Without them Box32 stops the game with
"PltResolver32: Symbol SDL_RWFromConstMem ... not found".

Usage: patch_box64.py <box64 source dir>
"""
import sys

root = sys.argv[1]
priv = f"{root}/src/wrapped32/wrappedsdl2_private.h"
impl = f"{root}/src/wrapped32/wrappedsdl2.c"


def replace_once(path, old, new):
    s = open(path).read()
    if old not in s and new in s:
        return  # already patched
    if old not in s:
        sys.exit(f"patch_box64: expected text not found in {path}: {old!r}")
    open(path, "w").write(s.replace(old, new, 1))


replace_once(priv, "//GOM(SDL_RWFromConstMem, pFEpi)", "GOM(SDL_RWFromConstMem, pFEpi)")
replace_once(priv, "//GO(SDL_GetAudioDeviceName, pFii)", "GO(SDL_GetAudioDeviceName, pFii)")
# 5 arguments, all 4 bytes on the 32-bit side; the last three are pointers (converted below)
replace_once(priv, "//GOM(SDL_LoadWAV_RW, pFEpippp)", "GOM(SDL_LoadWAV_RW, pFEpiiuu)")

code = r'''
// --- added for Desperados (android-box/child/patch_box64.py) ---
EXPORT void* my32_2_SDL_RWFromConstMem(x64emu_t* emu, void* a, int b)
{
    static void* (*f)(const void*, int) = NULL;
    if (!f) f = dlsym(my_lib->w.lib, "SDL_RWFromConstMem");
    if (!f) return NULL;
    SDL2_RWops_t* r = (SDL2_RWops_t*)f(a, b);
    void* ret = AddNativeRW2(emu, r);
    inplace_SDL2_RWops_to_32(ret);
    return ret;
}

EXPORT void* my32_2_SDL_LoadWAV_RW(x64emu_t* emu, void* a, int b, int spec_, uint32_t buf_, uint32_t len_)
{
    static void* (*f)(void*, int, SDL2_AudioSpec*, uint8_t**, uint32_t*) = NULL;
    if (!f) f = dlsym(my_lib->w.lib, "SDL_LoadWAV_RW");
    if (!f) return NULL;
    SDL2_AudioSpec32* spec = (SDL2_AudioSpec32*)from_ptrv((uint32_t)spec_);
    ptr_t* buf = (ptr_t*)from_ptrv(buf_);
    uint32_t* len = (uint32_t*)from_ptrv(len_);
    inplace_SDL2_RWops_to_64(a);
    SDL2_RWops_t* rw = RWNativeStart2(emu, (SDL2_RWops_t*)a);
    SDL2_AudioSpec native = { 0 };
    uint8_t* data = NULL;
    uint32_t size = 0;
    void* r = f(rw, b, &native, &data, &size);
    if (b == 0) {
        RWNativeEnd2(rw);
        inplace_SDL2_RWops_to_32(a);
    }
    if (!r) return NULL;
    spec->freq = native.freq;
    spec->format = native.format;
    spec->channels = native.channels;
    spec->silence = native.silence;
    spec->samples = native.samples;
    spec->padding = native.padding;
    spec->size = native.size;
    spec->callback = 0;
    spec->userdata = 0;
    if (buf) *buf = to_ptrv(data);
    if (len) *len = size;
    return spec;
}
'''
s = open(impl).read()
if "my32_2_SDL_RWFromConstMem" not in s:
    anchor = "EXPORT void *my32_2_SDL_LoadBMP_RW("
    if anchor not in s:
        sys.exit("patch_box64: LoadBMP_RW anchor not found")
    s = s.replace(anchor, code + "\n" + anchor, 1)
    open(impl, "w").write(s)
print("box64 patched")
