# SIN Editor — Architecture & Memory Management

## File Structure

```
sin_editor/
├── CMakeLists.txt
├── .github/workflows/ci.yml
└── src/
    ├── piece_table.h       ← Core text engine (Piece Table)
    ├── piece_table.cpp
    ├── highlighter.h       ← SINO syntax tokenizer (header-only)
    ├── editor.h            ← Document / Tab model
    └── main.cpp            ← Raylib rendering + input loop
```

---

## Memory Management Strategy

### 1. Piece Table — O(1) Insert/Delete

Traditional editors store text as a flat `std::string`. On a 50MB file,
every keystroke triggers a `memmove` of up to 50MB. The Piece Table solves this:

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

### 2. Append-Only Added Buffer

`added_buf_` only ever grows. This means:
- Undo/Redo stores only `{pieces_snapshot, added_buf_size}` — never text copies
- Restoring undo is `pieces_ = snapshot.pieces` — no memory allocation
- `added_buf_` shrinks are **never needed**: undo simply stops referencing
  data past the old `added_buf_size` boundary

### 3. Per-Line Render Cache

```cpp
struct LineCache {
    std::string          text;    // line content
    std::vector<Token>   tokens;  // syntax tokens
    bool                 dirty;   // re-tokenize flag
};
```

- On every edit, only lines **at and after** the cursor line are marked dirty
- The render loop only re-tokenizes dirty visible lines (not the whole file)
- For a 10,000-line file, a single keystroke re-tokenizes 1–3 lines, not 10,000

### 4. Viewport Clipping (Infinite Scroll Illusion)

The render loop iterates only `visible_lines = editor_height / LINE_H` lines:

```cpp
for (int li = doc->scroll_y; li < scroll_y + visible_lines; ++li)
    draw_line(li);
```

A 500,000-line file renders exactly the same as a 50-line file at 60fps.
The rest never touch the GPU.

### 5. Zero-Copy Text Serialization

`PieceTable::text()` is the only full materialization — called only on Save.
During rendering, individual lines are extracted cheaply via `PieceTable::line(n)`,
which short-circuits as soon as the nth newline is found.

### 6. Stack-Allocated Token Buffer

```cpp
std::vector<Token> tokens;
tokens.reserve(32);  // pre-alloc for typical line
```

Most lines have < 32 tokens. `reserve(32)` avoids heap realloc for the
common case; the vector grows naturally for long lines.

---

## Keyboard Shortcuts

| Shortcut         | Action        |
|------------------|---------------|
| Ctrl+S           | Save          |
| Ctrl+Z           | Undo          |
| Ctrl+Y           | Redo          |
| Ctrl+N           | New Tab       |
| Alt+← / Alt+→   | Scroll tabs   |
| Tab              | 4-space indent|
| Home / End       | Line bounds   |

---

## Build Instructions

### Linux / macOS
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./sin_editor [optional_file.sino]
```

### Windows (MSVC)
```powershell
mkdir build; cd build
cmake ..
cmake --build . --config Release
.\Release\sin_editor.exe
```

Raylib is auto-fetched via CMake FetchContent if not installed.

---

## Roadmap

| Phase | Feature                          |
|-------|----------------------------------|
| v0.2  | File picker dialog (tinyfiledialogs) |
| v0.3  | Multi-cursor editing              |
| v0.4  | SINO LSP-lite (completions)       |
| v0.5  | Terminal panel (embedded output)  |
| v1.0  | Full SINO debugger integration    |
