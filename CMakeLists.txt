cmake_minimum_required(VERSION 3.20)
project(SINEditorRoot VERSION 0.3.0 LANGUAGES NONE)

# ── Android-only guard ──────────────────────────────────────────────────────
if(NOT ANDROID)
  message(FATAL_ERROR "SIN Editor v0.3 is Android-only. Desktop support removed.")
endif()

# ── Delegate to the real Android NDK CMake ───────────────────────────────────
add_subdirectory(app/src/main/cpp)
