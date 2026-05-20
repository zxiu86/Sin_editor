#pragma once
#include "piece_table.h"
#include "highlighter.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>

// ============================================================
//  SIN Editor — Editor State & Document Model
// ============================================================

namespace sin {

namespace fs = std::filesystem;

struct Document {
    std::string         title;
    fs::path            path;          // empty = untitled
    PieceTable          buffer;
    bool                is_new = true;

    // Cursor
    size_t              caret_pos  = 0;
    size_t              sel_start  = 0;
    size_t              sel_end    = 0;
    bool                has_sel    = false;

    // Viewport
    int                 scroll_y   = 0;   // first visible line
    int                 scroll_x   = 0;   // horizontal pixel offset

    // Per-line highlight cache (dirty bit per line)
    struct LineCache {
        std::string             text;
        std::vector<Token>      tokens;
        bool                    dirty = true;
    };
    std::vector<LineCache>  line_cache;

    void invalidate_cache_from(size_t line) {
        if (line < line_cache.size())
            for (size_t i = line; i < line_cache.size(); ++i)
                line_cache[i].dirty = true;
    }

    explicit Document(std::string t = "Untitled")
        : title(std::move(t)), buffer("") {}
};

// ── Tab Bar ──────────────────────────────────────────────────────────────────

class TabManager {
public:
    std::vector<std::unique_ptr<Document>> docs;
    int active = 0;

    Document* current() {
        if (docs.empty()) return nullptr;
        return docs[active].get();
    }

    int add(std::string title) {
        docs.push_back(std::make_unique<Document>(std::move(title)));
        active = static_cast<int>(docs.size()) - 1;
        return active;
    }

    void close(int idx) {
        if (docs.empty()) return;
        docs.erase(docs.begin() + idx);
        if (active >= static_cast<int>(docs.size()))
            active = static_cast<int>(docs.size()) - 1;
        if (active < 0) active = 0;
    }
};

// ── Process Bridge (SINO runner) ─────────────────────────────────────────────

struct RunResult {
    int         exit_code = 0;
    std::string stdout_data;
    std::string stderr_data;
};

RunResult run_sino(const fs::path& script_path);

} // namespace sin
