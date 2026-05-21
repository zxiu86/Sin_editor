cmake_minimum_required(VERSION 3.20)
project(SINEditor VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Android-only build ──────────────────────────────────────────────────────
if(NOT ANDROID)
  message(FATAL_ERROR "This project is Android-only. Desktop support has been removed.")
endif()

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── Compiler flags (Android) ────────────────────────────────────────────────
add_compile_options(
  -Wall -Wextra -Wpedantic
  -O3
  -fvisibility=hidden
  -ffunction-sections
  -fdata-sections
)

# ── Android NDK configuration ───────────────────────────────────────────────
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PLATFORM Android CACHE STRING "" FORCE)
set(GRAPHICS GRAPHICS_API_OPENGL_ES2 CACHE STRING "" FORCE)
set(USE_AUDIO OFF CACHE BOOL "" FORCE)

# native_app_glue for Android
set(NATIVE_APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${NATIVE_APP_GLUE_DIR})
add_library(native_app_glue STATIC ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c)

# ── Sources ─────────────────────────────────────────────────────────────────
set(SOURCES
  piece_table.cpp
  jni_bridge.cpp
)

# ── Target: Android shared library (.so) ────────────────────────────────────
add_library(sineditor_engine SHARED ${SOURCES})

target_include_directories(sineditor_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sineditor_engine PRIVATE
  native_app_glue
  android
  log
  EGL
  GLESv2
)

# Strip symbols in Release to reduce .so size
set_target_properties(sineditor_engine PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

message(STATUS "SIN Editor Android v0.3 — Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ABI: ${ANDROID_ABI}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
