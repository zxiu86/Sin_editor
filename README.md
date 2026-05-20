# SIN Editor

High-performance IDE for the SINO programming language.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  KOTLIN  (UI Shell)                                  │
│  MainActivity  ──  tab management, file I/O, menu   │
│  EditorView    ──  Canvas rendering, IME, touch      │
│  FindReplaceDialog  ──  search & replace overlay    │
│  SyntaxHighlighter  ──  SINO tokeniser (Kotlin)     │
├─────────────────────────────────────────────────────┤
│  JNI Bridge  (jni_bridge.cpp)                        │
│  16 native functions: insert, erase, undo, redo …   │
├─────────────────────────────────────────────────────┤
│  C++20 ENGINE  (piece_table.h / .cpp)               │
│  O(1) insert / delete via Piece Table               │
│  Append-only buffers  ──  snapshot undo/redo        │
└─────────────────────────────────────────────────────┘
```

## Project Layout

```
sineditor-android/          Android app (Kotlin + C++ JNI)
├── app/
│   ├── build.gradle.kts
│   ├── proguard-rules.pro
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── cpp/
│       │   ├── CMakeLists.txt      (Android-only CMake)
│       │   ├── jni_bridge.cpp
│       │   ├── piece_table.h
│       │   └── piece_table.cpp
│       ├── kotlin/com/sineditor/app/
│       │   ├── EditorEngine.kt     (JNI wrapper)
│       │   ├── EditorView.kt       (custom Canvas View)
│       │   ├── FindReplaceDialog.kt
│       │   ├── MainActivity.kt
│       │   └── SyntaxHighlighter.kt
│       └── res/
│           ├── values/strings.xml
│           └── values/themes.xml
├── build.gradle.kts
├── settings.gradle.kts
├── gradlew / gradlew.bat
└── .github/workflows/ci.yml

CMakeLists.txt              Desktop build (Linux/Windows via Raylib)
src/                        Desktop source (main.cpp, piece_table, etc.)
res/
└── sinoicon.png            App icon (used by both Desktop and Android)
```

## Build

### Android APK

**Requirements:** JDK 17, Android SDK + NDK 25

```bash
# First-time: generate the Gradle wrapper jar
gradle wrapper --gradle-version 8.7

# Build debug APK
./gradlew assembleDebug

# APK location
ls app/build/outputs/apk/debug/app-debug.apk
```

Or just push to GitHub — the CI generates it automatically.

### Desktop (Linux / macOS)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./sin_editor
```

### Desktop (Windows)

```powershell
mkdir build; cd build
cmake ..
cmake --build . --config Release
.\Release\sin_editor.exe
```

## Icon

Place your icon at `res/sinoicon.png` (any size, square PNG).  
The CI copies it into all Android mipmap density folders automatically.  
For Desktop, it is loaded via `SetWindowIcon()` at startup.

## Keyboard Shortcuts (Desktop + Hardware keyboard)

| Shortcut    | Action            |
|-------------|-------------------|
| Ctrl+S      | Save              |
| Ctrl+Z      | Undo              |
| Ctrl+Y      | Redo              |
| Ctrl+N      | New tab           |
| Ctrl+W      | Close tab         |
| Home / End  | Line start / end  |
| Page Up/Dn  | Scroll page       |
| Tab         | Indent 4 spaces   |
