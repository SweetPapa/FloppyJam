cmake_minimum_required(VERSION 3.20)
project(polybloom C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# A plain configure must produce the contest-sized executable. Developers can
# explicitly opt out with -DPOLYBLOOM_RELEASE=OFF for assertions/overlays.
option(POLYBLOOM_RELEASE "Build the size-focused submission configuration" ON)
option(POLYBLOOM_INCLUDE_LEVEL3 "Compile Aurora Foundry into the game" ON)
option(POLYBLOOM_INCLUDE_LEVEL4 "Compile Sovereign Crown and its boss into the game" ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()
message(STATUS "POLYBLOOM optional content: Level 3=${POLYBLOOM_INCLUDE_LEVEL3}, Level 4/Boss=${POLYBLOOM_INCLUDE_LEVEL4}")

add_executable(polybloom
  src/main.c
  src/platform_raylib.c
  src/meshgen.c
  src/particles.c
  src/render.c
  src/collision.c
  src/player.c
  src/camera.c
  src/levels.c
  src/gameplay.c
  src/save.c
  src/audio.c
  src/ui.c)
target_include_directories(polybloom PRIVATE src)
if(POLYBLOOM_INCLUDE_LEVEL3)
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_INCLUDE_LEVEL3=1)
else()
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_INCLUDE_LEVEL3=0)
endif()
if(POLYBLOOM_INCLUDE_LEVEL4)
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_INCLUDE_LEVEL4=1)
else()
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_INCLUDE_LEVEL4=0)
endif()

# Homebrew exposes both shared and static raylib libraries but its package target
# selects the shared one. The contest package must be standalone, so prefer the
# archive explicitly when it is available.
find_path(RAYLIB_INCLUDE_DIR raylib.h REQUIRED)
find_library(RAYLIB_STATIC_LIBRARY NAMES libraylib.a raylib_static)
if(RAYLIB_STATIC_LIBRARY)
  target_include_directories(polybloom PRIVATE ${RAYLIB_INCLUDE_DIR})
  target_link_libraries(polybloom PRIVATE ${RAYLIB_STATIC_LIBRARY})
else()
  find_package(raylib 5.0 REQUIRED)
  target_link_libraries(polybloom PRIVATE raylib)
endif()

if(APPLE)
  target_link_libraries(polybloom PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

if(POLYBLOOM_RELEASE)
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_RELEASE=1)
  if(MSVC)
    target_compile_options(polybloom PRIVATE /O1 /GL)
    target_link_options(polybloom PRIVATE /LTCG /OPT:REF /OPT:ICF)
  else()
    target_compile_options(polybloom PRIVATE -Os -ffunction-sections -fdata-sections)
    target_link_options(polybloom PRIVATE -Wl,-dead_strip)
  endif()
else()
  target_compile_definitions(polybloom PRIVATE POLYBLOOM_DEBUG=1)
  if(MSVC)
    target_compile_options(polybloom PRIVATE /W4)
  else()
    target_compile_options(polybloom PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

set(POLYBLOOM_PACKAGE_LIMIT 1474560 CACHE STRING "Maximum extracted submission bytes")
add_custom_command(TARGET polybloom POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DEXECUTABLE=$<TARGET_FILE:polybloom>
    -DPACKAGE_LIMIT=${POLYBLOOM_PACKAGE_LIMIT}
    -DFAIL_ON_OVERSIZE=${POLYBLOOM_RELEASE}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SizeReport.cmake
  VERBATIM)

include(CTest)
if(BUILD_TESTING)
  add_executable(polybloom_controller_test EXCLUDE_FROM_ALL
    tests/controller_test.c src/collision.c src/player.c src/camera.c
    src/levels.c src/render.c src/meshgen.c src/particles.c src/gameplay.c)
  target_include_directories(polybloom_controller_test PRIVATE src ${RAYLIB_INCLUDE_DIR})
  target_link_libraries(polybloom_controller_test PRIVATE
    $<IF:$<BOOL:${RAYLIB_STATIC_LIBRARY}>,${RAYLIB_STATIC_LIBRARY},raylib>)
  if(APPLE)
    target_link_libraries(polybloom_controller_test PRIVATE
      "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
  add_test(NAME controller_grounding COMMAND polybloom_controller_test)
endif()
