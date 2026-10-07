#!/usr/bin/env python3
"""SDL2 for the game process (Linux/glibc, ARM64), built with only the offscreen video driver and
the disk audio driver. Two small changes:
  - the offscreen "desktop" has the size given in SDL_OFFSCREEN_WIDTH / SDL_OFFSCREEN_HEIGHT
    (the phone's aspect ratio), instead of a fixed 1024x768
  - the disk audio driver starts its stream with a 16-byte header describing the format, so the
    app knows how to play what arrives through the audio pipe"""
import sys, pathlib
root = pathlib.Path(sys.argv[1])

def patch(rel, old, new):
    p = root / rel
    s = p.read_text()
    if old not in s:
        sys.exit(f"patch_child_sdl: '{old[:50]}' not found in {rel}")
    p.write_text(s.replace(old, new, 1))

patch("src/video/offscreen/SDL_offscreenvideo.c", "    mode.w = 1024;\n    mode.h = 768;",
      "    mode.w = SDL_getenv(\"SDL_OFFSCREEN_WIDTH\") ? SDL_atoi(SDL_getenv(\"SDL_OFFSCREEN_WIDTH\")) : 1024;\n"
      "    mode.h = SDL_getenv(\"SDL_OFFSCREEN_HEIGHT\") ? SDL_atoi(SDL_getenv(\"SDL_OFFSCREEN_HEIGHT\")) : 768;")

patch("src/audio/disk/SDL_diskaudio.c", """    /* Allocate mixing buffer */
    if (!iscapture) {""", """    if (!iscapture) {  /* Desperados bridge: format header for the app */
        Uint8 hdr[16] = {'D', 'S', 'A', 'U'};
        Uint32 freq = (Uint32)_this->spec.freq;
        Uint16 fmt = _this->spec.format;
        SDL_memcpy(hdr + 4, &freq, 4);
        SDL_memcpy(hdr + 8, &fmt, 2);
        hdr[10] = _this->spec.channels;
        SDL_memcpy(hdr + 12, &_this->spec.samples, 2);
        SDL_RWwrite(_this->hidden->io, hdr, 1, sizeof hdr);
    }

    /* Allocate mixing buffer */
    if (!iscapture) {""")
print("child SDL patched")
