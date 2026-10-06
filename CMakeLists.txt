cmake_minimum_required(VERSION 3.16)
project(OpenDesperados C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- bzip2 (vendored, the game compresses most images with it) -------------
add_library(bz2_static STATIC
    third_party/bzip2/blocksort.c third_party/bzip2/huffman.c third_party/bzip2/crctable.c
    third_party/bzip2/randtable.c third_party/bzip2/compress.c third_party/bzip2/decompress.c
    third_party/bzip2/bzlib.c)
target_include_directories(bz2_static PUBLIC third_party/bzip2)
target_compile_definitions(bz2_static PRIVATE BZ_NO_STDIO)
set_target_properties(bz2_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(ZLIB REQUIRED)

# --- SDL2 -------------------------------------------------------------------
if(ANDROID)
    # The Android build passes the SDL source folder (downloaded by the CI workflow).
    if(NOT SDL2_SOURCE_DIR)
        set(SDL2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android/SDL)
    endif()
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${SDL2_SOURCE_DIR} SDL2_build)
    set(SDL_LIBS SDL2)
    if(TARGET SDL2main)
        list(PREPEND SDL_LIBS SDL2main)
    endif()
else()
    find_package(SDL2 REQUIRED)
    if(TARGET SDL2::SDL2main)
        set(SDL_LIBS SDL2::SDL2main SDL2::SDL2)
    else()
        set(SDL_LIBS ${SDL2_LIBRARIES})
        include_directories(${SDL2_INCLUDE_DIRS})
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/App.cpp
    src/core/FileSystem.cpp
    src/formats/Decompress.cpp
    src/formats/Image16.cpp
    src/formats/LevelFile.cpp
    src/formats/LevelElements.cpp
    src/formats/Masks.cpp
    src/formats/SpriteFile.cpp
    src/game/Level.cpp
    src/render/SpriteAtlas.cpp
    src/input/TouchInput.cpp
    src/render/Textures.cpp
    src/screens/LevelSelectScreen.cpp
    src/screens/MapViewScreen.cpp
    src/screens/WaitingForDataScreen.cpp
)

if(ANDROID)
    add_library(main SHARED ${SOURCES})   # SDL's Java activity loads libmain.so
    set(APP_TARGET main)
else()
    add_executable(desperados ${SOURCES})
    set(APP_TARGET desperados)
endif()

target_link_libraries(${APP_TARGET} PRIVATE bz2_static ZLIB::ZLIB ${SDL_LIBS})
if(ANDROID)
    target_link_libraries(${APP_TARGET} PRIVATE log android)
endif()
if(NOT MSVC)
    target_compile_options(${APP_TARGET} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
