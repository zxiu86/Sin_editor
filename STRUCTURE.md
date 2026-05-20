# SIN Editor — Flat Root Structure

All source files are placed in the repository root for simplicity.

## Files in Root

### C++ Core (Desktop + Android JNI)
| File | Purpose |
|------|---------|
| `piece_table.h` | Piece Table text buffer engine |
| `piece_table.cpp` | Piece Table implementation |
| `editor.h` | Document model & Tab manager |
| `highlighter.h` | SINO syntax tokenizer (header-only) |
| `main.cpp` | Raylib desktop renderer + input loop |
| `jni_bridge.cpp` | JNI bridge for Android (namespace: sin) |

### Build Files
| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Desktop build (Linux/Windows/macOS) |
| `CMakeLists-android.txt` | Android JNI library build |
| `settings.gradle.kts` | Gradle project settings |

### Android UI (Kotlin)
| File | Purpose |
|------|---------|
| `EditorEngine.kt` | JNI wrapper for PieceTable |
| `EditorView.kt` | Custom Canvas editor view |
| `FindReplaceDialog.kt` | Search & replace overlay |
| `MainActivity.kt` | Main activity & tab management |
| `SyntaxHighlighter.kt` | SINO language tokenizer |
| `AndroidManifest.xml` | Android app manifest |

### CI/CD
| File | Purpose |
|------|---------|
| `.github/workflows/ci.yml` | GitHub Actions: Linux + Windows + Android |

### Documentation
| File | Purpose |
|------|---------|
| `ARCHITECTURE.md` | Architecture & memory management |
| `README.md` | Project overview & build instructions |

## Build Commands

### Desktop
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./sin_editor
```

### Android
```bash
# Build native library
cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/cmake .. -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a

# Build APK via Gradle
./gradlew assembleDebug
```
