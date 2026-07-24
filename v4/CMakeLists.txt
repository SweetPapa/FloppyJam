cmake_minimum_required(VERSION 3.16)
project(maglava C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# A plain configure produces the size-focused, floppy-sized executable.
# Opt out with -DMAGLAVA_RELEASE=OFF for warnings/asserts.
option(MAGLAVA_RELEASE "Build the size-focused submission configuration" ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

add_executable(maglava
  src/main.c
  src/sim.c
  src/levels.c
  src/render.c
  src/audio.c
  src/ui.c
  src/save.c)
target_include_directories(maglava PRIVATE src)

# Prefer a static raylib archive so the result is standalone (no shared dep).
find_path(RAYLIB_INCLUDE_DIR raylib.h REQUIRED)
find_library(RAYLIB_STATIC_LIBRARY NAMES libraylib.a raylib_static)
target_include_directories(maglava PRIVATE ${RAYLIB_INCLUDE_DIR})
if(RAYLIB_STATIC_LIBRARY)
  target_link_libraries(maglava PRIVATE ${RAYLIB_STATIC_LIBRARY})
else()
  find_package(raylib REQUIRED)
  target_link_libraries(maglava PRIVATE raylib)
endif()
target_link_libraries(maglava PRIVATE m)

if(APPLE)
  target_link_libraries(maglava PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX)
  target_link_libraries(maglava PRIVATE dl pthread)
endif()

if(MAGLAVA_RELEASE)
  if(MSVC)
    target_compile_options(maglava PRIVATE /O1 /GL)
    target_link_options(maglava PRIVATE /LTCG /OPT:REF /OPT:ICF)
  else()
    target_compile_options(maglava PRIVATE -Os -ffunction-sections -fdata-sections)
    if(APPLE)
      target_link_options(maglava PRIVATE -Wl,-dead_strip)
    else()
      target_link_options(maglava PRIVATE -Wl,--gc-sections -s)
    endif()
  endif()
else()
  if(NOT MSVC)
    target_compile_options(maglava PRIVATE -Wall -Wextra -g)
  endif()
endif()

set(MAGLAVA_PACKAGE_LIMIT 1474560 CACHE STRING "Maximum executable bytes (1.44MB floppy)")
add_custom_command(TARGET maglava POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DEXECUTABLE=$<TARGET_FILE:maglava>
    -DPACKAGE_LIMIT=${MAGLAVA_PACKAGE_LIMIT}
    -DFAIL_ON_OVERSIZE=${MAGLAVA_RELEASE}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SizeReport.cmake
  VERBATIM)

# Headless simulation tests (no raylib).
include(CTest)
if(BUILD_TESTING)
  add_executable(maglava_sim_test EXCLUDE_FROM_ALL tests/test_sim.c src/sim.c src/levels.c)
  target_include_directories(maglava_sim_test PRIVATE src)
  target_link_libraries(maglava_sim_test PRIVATE m)
  add_test(NAME sim_test COMMAND maglava_sim_test)
endif()
