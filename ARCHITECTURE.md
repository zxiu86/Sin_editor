# SIN Editor — Architecture & Memory Management (Android v0.3)

## Overview

SIN Editor is now **Android-only**. All desktop support (Raylib, Windows, Linux, macOS) has been removed. The project is optimized for mobile constraints with strict memory management.

## File Structure

```
sin_editor/
├── CMakeLists.txt              ← Android-only CMake (shared library .so)
├── settings.gradle.kts         ← Gradle project settings
├── .github/workflows/ci.yml    ← Android-only CI
├── AndroidManifest.xml         ← Android app manifest
└── app/
    └── src/main/
        ├── cpp/
        │   ├── piece_table.h       ← Core text engine (Piece Table)
        │   ├── piece_table.cpp     ← Piece Table implementation
        │   └── jni_bridge.cpp      ← JNI bridge (19 native functions)
        └── java/com/sineditor/app/
            ├── EditorEngine.kt       ← JNI wrapper with memory monitoring
            ├── EditorView.kt         ← Canvas rendering, touch UI
            ├── FindReplaceDialog.kt  ← Search & replace overlay
            ├── MainActivity.kt       ← Main activity & tab management
            └── SyntaxHighlighter.kt  ← SINO language tokenizer
```

---

## Memory Management Strategy

### 1. Piece Table — O(1) Insert/Delete

Traditional editors store text as a flat `std::string`. On a 50MB file, every keystroke triggers a `memmove` of up to 50MB. The Piece Table solves this:

```
original_buf_  = [immutable copy of the file on load]
added_buf_     = [append-only buffer; grows as user types]

pieces_        = [{buf:orig, start:0, len:5},
                  {buf:add,  start:0, len:3},   ← inserted text
                  {buf:orig, start:5, len:100}]
```

- **Insert** → append to `added_buf_`, split piece at cursor: O(1) amortized
- **Delete** → split/trim pieces: O(1) amortized
- **No reallocation** of existing buffers ever during a session

### 2. Mobile Memory Caps (NEW in v0.3)

```cpp
constexpr size_t MAX_ADDED_BUFFER_SIZE = 8 * 1024 * 1024;   // 8 MB cap
constexpr size_t MAX_UNDO_SNAPSHOTS    = 50;                // Max undo levels
constexpr size_t MAX_REDO_SNAPSHOTS    = 25;                // Max redo levels
```

- When `added_buf_` exceeds 8MB, automatic **compaction** occurs: current text is materialized into `original_buf_`, buffers are reset, and history is cleared.
- Undo/redo stacks are trimmed to prevent unbounded growth on mobile devices.
- **Memory pressure API** exposed to Kotlin: `engine.memoryUsage`, `engine.isMemoryPressure`, `engine.compact()`

### 3. Per-Line Render Cache with LRU Eviction

```kotlin
private val lineCache = object : LinkedHashMap<Long, CachedLine>(64, 0.75f, true) {
    override fun removeEldestEntry(eldest: Map.Entry<Long, CachedLine>?): Boolean {
        return size > 200  // Max 200 cached lines
    }
}
```

- On every edit, only lines **at and after** the cursor line are marked dirty
- The render loop only re-tokenizes dirty visible lines (not the whole file)
- LRU eviction prevents memory bloat on large files
- For a 10,000-line file, a single keystroke re-tokenizes 1–3 lines, not 10,000

### 4. Viewport Clipping (Infinite Scroll Illusion)

The render loop iterates only `visible_lines = editor_height / LINE_H` lines:

```kotlin
for (li in scrollY until min(scrollY + vis + 1, lc.toInt())) {
    draw_line(li)
}
```

A 500,000-line file renders exactly the same as a 50-line file at 60fps. The rest never touch the GPU.

### 5. Zero-Copy Text Serialization

`PieceTable::text()` is the only full materialization — called only on Save. During rendering, individual lines are extracted cheaply via `PieceTable::line(n)`, which short-circuits as soon as the nth newline is found.

### 6. JNI Memory Safety

- **Handle registry**: All native `PieceTable` pointers are tracked in a thread-safe `unordered_map`. Invalid handles are detected and rejected.
- **RAII wrappers**: `JStringGuard` class ensures `ReleaseStringUTFChars` is always called.
- **Exception checking**: Every JNI operation checks for pending exceptions.
- **Bounds validation**: All `char_pos` and `count` parameters are clamped to valid ranges.
- **AutoCloseable**: `EditorEngine` implements `AutoCloseable` for try-with-resources patterns.

---

## Touch UI Actions (Replaced Keyboard Shortcuts)

| Desktop Shortcut | Touch UI Replacement | Location |
|------------------|----------------------|----------|
| Ctrl+S           | "Save" button        | Toolbar  |
| Ctrl+Z           | "Undo" button        | Toolbar  |
| Ctrl+Y           | "Redo" button        | Toolbar  |
| Ctrl+N           | "New" button         | Toolbar  |
| Ctrl+W           | "Close Tab" (×)      | Tab bar  |
| Tab              | IME auto-indent      | Keyboard |
| Home / End       | DPAD keys (hardware) | Physical |

---

## Build Instructions

### Android APK

**Requirements:** JDK 17, Android SDK + NDK 25

```bash
# Build native library
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-android --parallel

# Build APK via Gradle
./gradlew assembleDebug

# APK location
ls app/build/outputs/apk/debug/app-debug.apk
```

Or just push to GitHub — the CI generates it automatically.

---

## Roadmap

| Phase | Feature                          |
|-------|----------------------------------|
| v0.3  | Android-only, memory caps, touch UI |
| v0.4  | Multi-cursor editing              |
| v0.5  | SINO LSP-lite (completions)       |
| v0.6  | Terminal panel (embedded output)    |
| v1.0  | Full SINO debugger integration      |
