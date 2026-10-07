#!/usr/bin/env python3
"""The original game asks SDL for a desktop OpenGL 3.x context; on Android only OpenGL ES
exists. Make SDL create an OpenGL ES 3.0 context whatever the game requests (the graphics
calls are translated by libGLdesp.so)."""
import sys, pathlib
p = pathlib.Path(sys.argv[1]) / "src/video/SDL_video.c"
s = p.read_text()
old = "int SDL_GL_SetAttribute(SDL_GLattr attr, int value)\n{\n"
if old not in s:
    sys.exit("patch_sdl: SDL_GL_SetAttribute not found")
new = old + """#ifdef __ANDROID__
    switch (attr) {  /* Desperados (original): always OpenGL ES 3.0 */
    case SDL_GL_CONTEXT_PROFILE_MASK: value = SDL_GL_CONTEXT_PROFILE_ES; break;
    case SDL_GL_CONTEXT_MAJOR_VERSION: value = 3; break;
    case SDL_GL_CONTEXT_MINOR_VERSION: value = 0; break;
    case SDL_GL_CONTEXT_FLAGS: value = 0; break;
    default: break;
    }
#endif
"""
s = s.replace(old, new, 1)
p.write_text(s)
print("SDL patched: OpenGL ES 3 contexts")
