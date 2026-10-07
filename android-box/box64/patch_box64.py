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

# ---- Box32 on bionic: the few glibc-only pieces (all guarded by #ifdef ANDROID) ----
def prepend_after(rel, anchor, text):
    p = root / rel
    s = p.read_text()
    i = s.find(anchor)
    if i < 0:
        sys.exit(f"patch_box64: anchor '{anchor}' not found in {rel}")
    i = s.find("\n", i) + 1
    p.write_text(s[:i] + text + s[i:])

# signal32.c: bionic's <signal.h> turns si_tid into a member path, and has no __SI_SIGFAULT_ADDL
prepend_after("src/libtools/signal32.c", "#include <pthread.h>",
    "#ifdef ANDROID\n#undef si_tid\n#ifndef __SI_SIGFAULT_ADDL\n#define __SI_SIGFAULT_ADDL\n#endif\n#endif\n")

# threads32.c: default thread attributes, mutex type from the attributes
patch("src/libtools/threads32.c", "pthread_getattr_default_np(&attr);",
      "\n#ifdef ANDROID\n            pthread_attr_init(&attr);\n#else\n            pthread_getattr_default_np(&attr);\n#endif\n")
patch("src/libtools/threads32.c",
      "fake->i386__kind = ((struct __pthread_mutex_s*)from_ptrv(fake->real_mutex))->__kind;",
      "\n#ifdef ANDROID\n    { int type = PTHREAD_MUTEX_NORMAL; if(att) pthread_mutexattr_gettype(att, &type); fake->i386__kind = type; }\n"
      "#else\n    fake->i386__kind = ((struct __pthread_mutex_s*)from_ptrv(fake->real_mutex))->__kind;\n#endif\n")

# libc_net32.c: reentrant service / protocol lookups and the resolver state
prepend_after("src/libtools/libc_net32.c", '#include "debug.h"', r"""
#ifdef ANDROID
#include <netdb.h>
static int getservbyname_r(const char* n, const char* p, struct servent* r, char* b, size_t l, struct servent** res)
{ (void)b; (void)l; struct servent* s = getservbyname(n, p); if(!s) { *res = NULL; return ENOENT; } *r = *s; *res = r; return 0; }
static int getprotobyname_r(const char* n, struct protoent* r, char* b, size_t l, struct protoent** res)
{ (void)b; (void)l; struct protoent* s = getprotobyname(n); if(!s) { *res = NULL; return ENOENT; } *r = *s; *res = r; return 0; }
static int getprotobynumber_r(int n, struct protoent* r, char* b, size_t l, struct protoent** res)
{ (void)b; (void)l; struct protoent* s = getprotobynumber(n); if(!s) { *res = NULL; return ENOENT; } *r = *s; *res = r; return 0; }
#endif
""")
patch("src/libtools/libc_net32.c", "typedef struct my_res_state_32_s {", "#ifndef ANDROID\ntypedef struct my_res_state_32_s {")
patch("src/libtools/libc_net32.c", "void convert_ns_msg_to_32(void* d, void* s)", r"""#else
EXPORT void* my32___res_state(x64emu_t* emu) { static char dummy[512]; (void)emu; return dummy; }
EXPORT int my32_res_query(x64emu_t* emu, void* dname, int class, int type, void* answer, int anslen)
{ (void)dname; (void)class; (void)type; (void)answer; (void)anslen; emu->libc_herr = 1; return -1; }
EXPORT int my32_res_search(x64emu_t* emu, void* dname, int class, int type, void* answer, int anslen)
{ (void)dname; (void)class; (void)type; (void)answer; (void)anslen; emu->libc_herr = 1; return -1; }
#endif
void convert_ns_msg_to_32(void* d, void* s)""")

# wrappedlibc.c (32-bit): glob64 is glob on bionic; posix_spawn_file_actions_t is a pointer there
prepend_after("src/wrapped32/wrappedlibc.c", "#include <stdlib.h>",
    "#ifdef ANDROID\n#define glob64_t glob_t\n#define glob64 glob\n#define globfree64 globfree\n#endif\n")
patch("src/wrapped32/wrappedlibc.c", """    posix_spawn_file_actions_t* src = s;
    dst->__allocated = src->__allocated;
    dst->__used = src->__used;
    dst->__actions = to_ptrv(src->__actions);""", """#ifdef ANDROID
    memset(dst, 0, sizeof(*dst));
    memcpy(dst->__pad, s, sizeof(posix_spawn_file_actions_t));
#else
    posix_spawn_file_actions_t* src = s;
    dst->__allocated = src->__allocated;
    dst->__used = src->__used;
    dst->__actions = to_ptrv(src->__actions);
#endif""")
patch("src/wrapped32/wrappedlibc.c", """    posix_spawn_file_actions_t* dst = d;
    posix_spawn_file_actions_32_t* src = s;
    dst->__actions = from_ptrv(src->__actions);
    dst->__used = src->__used;
    dst->__allocated = src->__allocated;""", """#ifdef ANDROID
    posix_spawn_file_actions_32_t* src = s;
    memcpy(d, src->__pad, sizeof(posix_spawn_file_actions_t));
#else
    posix_spawn_file_actions_t* dst = d;
    posix_spawn_file_actions_32_t* src = s;
    dst->__actions = from_ptrv(src->__actions);
    dst->__used = src->__used;
    dst->__allocated = src->__allocated;
#endif""")

# bionic lacks some glibc headers Box32 needs for structure layouts
import shutil
compat = pathlib.Path(__file__).parent / "compat"
for h in compat.glob("*.h"):
    shutil.copy(h, root / "src" / "include" / h.name)
print("box64 patched for in-app use")
