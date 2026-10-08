cmake_minimum_required(VERSION 3.18)
project(DesperadosOriginal C)

# SDL2 from source (the app's window, touch screen and sound)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${SDL2_SOURCE_DIR} SDL2 EXCLUDE_FROM_ALL)

# SDL_main: starts the game process and shows / drives it through the bridge
set(BRIDGE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../bridge)
add_library(main SHARED launcher.c controls.c overlay.c ${BRIDGE}/replay.c)
target_include_directories(main PRIVATE ${BRIDGE})
target_compile_options(main PRIVATE -Wall -Wno-unused-parameter)
set(MAIN_LIBS SDL2)
if(TARGET SDL2main)
    list(PREPEND MAIN_LIBS SDL2main)
endif()
target_link_libraries(main ${MAIN_LIBS} GLESv3 log dl m)
