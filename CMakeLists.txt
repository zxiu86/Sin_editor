cmake_minimum_required(VERSION 3.20)
project(SINEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(
  -Wall -Wextra -Wpedantic
  $<$<CONFIG:Release>:-O3 -march=native>
  $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)
add_link_options(
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# ── Raylib (find via pkg-config or FetchContent) ──────────────────────────────
find_package(raylib 5.0 QUIET)

if(NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
  )
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
  main.cpp
  piece_table.cpp
)

add_executable(sin_editor ${SOURCES})

# All sources in root, no include dir needed

target_link_libraries(sin_editor PRIVATE raylib)

# Platform-specific
if(UNIX AND NOT APPLE)
  target_link_libraries(sin_editor PRIVATE GL m pthread dl rt X11)
endif()
if(APPLE)
  target_link_libraries(sin_editor PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit")
endif()
if(WIN32)
  target_link_libraries(sin_editor PRIVATE opengl32 gdi32 winmm)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS sin_editor RUNTIME DESTINATION bin)

message(STATUS "SIN Editor — Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
