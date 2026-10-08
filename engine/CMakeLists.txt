cmake_minimum_required(VERSION 3.18)
project(DesperadosEngine C CXX)

# The new Desperados engine. Every part of src/sb (the original's "SB" base library) and src/dv
# (the original's "DV" game classes) is translated from the function of the same name in the
# original Linux build (desperados32), so the game behaves like the original. src/app is the
# phone side: window, touch, camera feel.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(dv_bz2 STATIC
    ${REPO}/third_party/bzip2/blocksort.c ${REPO}/third_party/bzip2/huffman.c
    ${REPO}/third_party/bzip2/crctable.c ${REPO}/third_party/bzip2/randtable.c
    ${REPO}/third_party/bzip2/compress.c ${REPO}/third_party/bzip2/decompress.c
    ${REPO}/third_party/bzip2/bzlib.c)
target_include_directories(dv_bz2 PUBLIC ${REPO}/third_party/bzip2)
target_compile_definitions(dv_bz2 PRIVATE BZ_NO_STDIO)
set_target_properties(dv_bz2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sb/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dv/*.cpp)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp)

# the engine itself, without window or GL: also used by the desktop test tools
add_library(dvengine STATIC ${ENGINE_SOURCES})
target_include_directories(dvengine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(dvengine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(dvengine PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare)
if(ANDROID)
    target_link_libraries(dvengine PUBLIC dv_bz2 z log)
else()
    find_package(ZLIB REQUIRED)
    target_link_libraries(dvengine PUBLIC dv_bz2 ZLIB::ZLIB)
endif()

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${SDL2_SOURCE_DIR} SDL2 EXCLUDE_FROM_ALL)
    add_library(main SHARED ${APP_SOURCES})
    set(MAIN_LIBS SDL2)
    if(TARGET SDL2main)
        list(PREPEND MAIN_LIBS SDL2main)
    endif()
    target_link_libraries(main dvengine ${MAIN_LIBS} GLESv3 log m)
    target_compile_options(main PRIVATE -Wall -Wno-unused-parameter)
else()
    find_package(SDL2 QUIET)
    if(SDL2_FOUND AND APP_SOURCES)
        add_executable(dvgame ${APP_SOURCES})
        target_include_directories(dvgame PRIVATE ${SDL2_INCLUDE_DIRS})
        target_compile_definitions(dvgame PRIVATE DV_DESKTOP=1)
        target_link_libraries(dvgame dvengine ${SDL2_LIBRARIES} GLESv2 m)
        target_compile_options(dvgame PRIVATE -Wall -Wno-unused-parameter)
    endif()

    # desktop checks: load every level, write pictures of what the engine draws
    file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp)
    foreach(t ${TOOL_SOURCES})
        get_filename_component(n ${t} NAME_WE)
        add_executable(${n} ${t})
        target_link_libraries(${n} dvengine EGL GLESv2)
    endforeach()
endif()
