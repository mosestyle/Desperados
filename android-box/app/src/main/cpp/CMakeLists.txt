cmake_minimum_required(VERSION 3.18)
project(DesperadosOriginal C)

# SDL2 from source (patched in CI so it always creates OpenGL ES 3 contexts)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${SDL2_SOURCE_DIR} SDL2 EXCLUDE_FROM_ALL)

# desktop OpenGL -> OpenGL ES 3 for the game
add_library(GLdesp SHARED glshim.c)
target_compile_options(GLdesp PRIVATE -fvisibility=hidden -O2)
target_link_libraries(GLdesp GLESv3 EGL log dl)

# SDL_main: prepares the files and starts the game through Box64
add_library(main SHARED launcher.c)
set(MAIN_LIBS SDL2)
if(TARGET SDL2main)
    list(PREPEND MAIN_LIBS SDL2main)
endif()
target_link_libraries(main ${MAIN_LIBS} log dl)
