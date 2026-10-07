#!/usr/bin/env python3
"""Turns a Box64 checkout into a shared library (libbox64.so) that the Android app loads
in-process, so the game's SDL2 calls reach the app's own SDL2 (window, touch, audio).

  - the box64 executable target becomes a SHARED library, main() becomes box64_main()
  - box64 finds its own my_/my32_ wrappers through dlopen("libbox64.so") instead of the
    main executable
  - the native SDL2 library is the app's libSDL2.so"""
import re, sys, pathlib

root = pathlib.Path(sys.argv[1])

def patch(rel, old, new, count=1):
    p = root / rel
    s = p.read_text()
    if old not in s:
        sys.exit(f"patch_box64: '{old[:60]}' not found in {rel}")
    s = s.replace(old, new, count)
    p.write_text(s)

patch("CMakeLists.txt", "add_executable(${BOX64})", "add_library(${BOX64} SHARED)")
# everything ends up in a shared library: position-independent code throughout
s = (root / "CMakeLists.txt").read_text()
s = s.replace("project(box64", "set(CMAKE_POSITION_INDEPENDENT_CODE ON)\nproject(box64", 1)
(root / "CMakeLists.txt").write_text(s)
patch("src/main.c", "int main(int argc, const char **argv, char **env) {",
      "__attribute__((visibility(\"default\"))) int box64_main(int argc, const char **argv, char **env) {")
patch("src/box64context.c", "context->box64lib = dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);",
      "context->box64lib = dlopen(\"libbox64.so\", RTLD_NOW|RTLD_GLOBAL);\n"
      "    if(!context->box64lib) context->box64lib = dlopen(NULL, RTLD_NOW|RTLD_GLOBAL);")
patch("src/wrapped/wrappedsdl2.c", 'const char* sdl2Name = "libSDL2-2.0.so.0";', 'const char* sdl2Name = "libSDL2.so";')
print("box64 patched for in-app use")
