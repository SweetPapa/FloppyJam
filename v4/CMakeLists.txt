cmake_minimum_required(VERSION 3.16)
project(maglava C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Desktop builds prioritize smooth rendering; there is no package size cap.
option(MAGLAVA_RELEASE "Enable optimized desktop build" ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(maglava
  src/main.c
  src/sim.c
  src/levels.c
  src/render.c
  src/audio.c
  src/music.c
  src/ui.c
  src/save.c)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB LEVEL_JSON CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/levels/*.json)
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/levels_gen.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_levels.py
  DEPENDS tools/gen_levels.py ${LEVEL_JSON}
  VERBATIM)
add_custom_target(maglava_levels DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/levels_gen.h)
add_dependencies(maglava maglava_levels)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts_gen.h
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_fonts.py
    ${CMAKE_CURRENT_BINARY_DIR}/fonts_gen.h
  DEPENDS tools/embed_fonts.py assets/fonts/Barlow-Medium.ttf assets/fonts/Barlow-SemiBold.ttf
  VERBATIM)
target_sources(maglava PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/fonts_gen.h)
target_include_directories(maglava PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS maglava RUNTIME DESTINATION .)
install(FILES assets/fonts/OFL.txt DESTINATION licenses)

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
if(NOT MSVC)
  target_link_libraries(maglava PRIVATE m)
endif()

if(APPLE)
  target_link_libraries(maglava PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX)
  target_link_libraries(maglava PRIVATE dl pthread)
endif()

if(MAGLAVA_RELEASE)
  if(MSVC)
    target_compile_options(maglava PRIVATE /O2 /GL)
    target_link_options(maglava PRIVATE /LTCG /OPT:REF /OPT:ICF)
  else()
    target_compile_options(maglava PRIVATE -O2 -ffunction-sections -fdata-sections)
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

# Headless simulation tests (no raylib).
include(CTest)
if(BUILD_TESTING)
  add_test(NAME level_data COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_levels.py --check)
  add_executable(maglava_sim_test tests/test_sim.c src/sim.c src/levels.c)
  target_include_directories(maglava_sim_test PRIVATE src)
  if(NOT MSVC)
    target_link_libraries(maglava_sim_test PRIVATE m)
  endif()
  add_test(NAME sim_test COMMAND maglava_sim_test)
  add_executable(maglava_campaign_audit tools/campaign_audit.c src/sim.c src/levels.c)
  target_include_directories(maglava_campaign_audit PRIVATE src)
  if(NOT MSVC)
    target_link_libraries(maglava_campaign_audit PRIVATE m)
  endif()
  add_test(NAME campaign_audit COMMAND maglava_campaign_audit --strict)
  add_test(NAME new_level_comfort COMMAND maglava_campaign_audit --strict --new-relaxed)
  add_test(NAME subspace_comfort COMMAND maglava_campaign_audit --strict --deathless --new-relaxed --level-key level-6b)
  add_executable(maglava_save_test tests/test_save.c src/save.c src/levels.c src/sim.c)
  target_include_directories(maglava_save_test PRIVATE src)
  if(NOT MSVC)
    target_link_libraries(maglava_save_test PRIVATE m)
  endif()
  add_test(NAME save_test COMMAND maglava_save_test ${CMAKE_CURRENT_BINARY_DIR}/save-test.dat)
  add_dependencies(maglava_sim_test maglava_levels)
  add_dependencies(maglava_campaign_audit maglava_levels)
  add_dependencies(maglava_save_test maglava_levels)
  # Release builds still execute assertions in the headless regression suites.
  foreach(test_target maglava_sim_test maglava_campaign_audit maglava_save_test)
    if(MSVC)
      target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
