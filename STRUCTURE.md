# SIN Editor — Android-Only Structure (v0.3)

All source files are organized for Android-only development.

## Files

### C++ Core (Android JNI)
| File | Purpose |
|------|---------|
| `piece_table.h` | Piece Table text buffer engine (with memory caps) |
| `piece_table.cpp` | Piece Table implementation (auto-compaction) |
| `jni_bridge.cpp` | JNI bridge for Android (19 functions, handle registry) |

### Build Files
| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Android-only CMake (shared library .so) |
| `settings.gradle.kts` | Gradle project settings |

### Android UI (Kotlin)
| File | Purpose |
|------|---------|
| `EditorEngine.kt` | JNI wrapper for PieceTable + memory monitoring |
| `EditorView.kt` | Custom Canvas editor view (touch UI, LRU cache) |
| `FindReplaceDialog.kt` | Search & replace overlay |
| `MainActivity.kt` | Main activity & tab management (touch toolbar) |
| `SyntaxHighlighter.kt` | SINO language tokenizer |
| `AndroidManifest.xml` | Android app manifest |

### CI/CD
| File | Purpose |
|------|---------|
| `.github/workflows/ci.yml` | GitHub Actions: Android APK only |

### Documentation
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architecture & memory management |
| `README.md` | Project overview & build instructions |

## Build Commands

### Android
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
```
