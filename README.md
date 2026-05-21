# SIN Editor v0.3 (Android Only)

High-performance IDE for the SINO programming language. **Desktop support removed.**

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  KOTLIN  (UI Shell)                                  │
│  MainActivity  ──  tab management, file I/O, menu   │
│  EditorView    ──  Canvas rendering, IME, touch       │
│  FindReplaceDialog  ──  search & replace overlay      │
│  SyntaxHighlighter  ──  SINO tokeniser (Kotlin)       │
├─────────────────────────────────────────────────────┤
│  JNI Bridge  (jni_bridge.cpp)                        │
│  19 native functions: insert, erase, undo, redo,     │
│  memory monitoring, compaction ...                    │
├─────────────────────────────────────────────────────┤
│  C++20 ENGINE  (piece_table.h / .cpp)               │
│  O(1) insert / delete via Piece Table               │
│  Append-only buffers  ──  snapshot undo/redo          │
│  Memory caps: 8MB buffer, 50 undo, 25 redo levels     │
└─────────────────────────────────────────────────────┘
```

## Project Layout

```
sin_editor/
├── CMakeLists.txt              ← Android-only CMake
├── settings.gradle.kts         ← Gradle settings
├── AndroidManifest.xml         ← App manifest
├── .github/workflows/ci.yml    ← Android CI
└── app/src/main/
    ├── cpp/
    │   ├── CMakeLists.txt      ← Android NDK CMake
    │   ├── jni_bridge.cpp      ← JNI bridge (19 functions)
    │   ├── piece_table.h       ← Piece Table header
    │   └── piece_table.cpp     ← Piece Table implementation
    └── java/com/sineditor/app/
        ├── EditorEngine.kt       ← JNI wrapper + memory API
        ├── EditorView.kt         ← Canvas editor + touch UI
        ├── FindReplaceDialog.kt  ← Find/replace overlay
        ├── MainActivity.kt       ← Main activity
        └── SyntaxHighlighter.kt  ← SINO tokenizer
```

## Build

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

## Memory Management

| Limit | Value | Action When Exceeded |
|-------|-------|---------------------|
| Added buffer | 8 MB | Auto-compaction |
| Undo stack | 50 levels | Oldest snapshots discarded |
| Redo stack | 25 levels | Oldest snapshots discarded |
| Line cache | 200 lines | LRU eviction |

Access memory info via:
- `EditorEngine.memoryUsage` — bytes used
- `EditorEngine.isMemoryPressure` — warning flag
- `EditorEngine.compact()` — force compaction

## Touch UI

All actions are accessible via touch buttons:
- **Toolbar**: New, Save, Undo, Redo, Run
- **Tab bar**: Switch tabs, close tabs (×)
- **Menu (☰)**: Open, Save As, Memory Info, About
- **Find (🔍)**: Search & replace dialog

## Keyboard Shortcuts (Hardware keyboards only)

| Shortcut | Action |
|----------|--------|
| DEL | Backspace |
| Forward DEL | Delete forward |
| Enter | New line |
| Tab | 4-space indent |
| DPAD | Move caret |

## Icon

Place your icon at `res/mipmap-hdpi/ic_launcher.png` (square PNG). The CI copies it automatically.
