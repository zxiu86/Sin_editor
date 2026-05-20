#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// ============================================================
//  SIN Editor — Piece Table Text Buffer
//  O(1) insert/delete via append-only buffers + piece list
// ============================================================

namespace sin {

enum class BufferType : uint8_t { Original, Added };

struct Piece {
    BufferType  buffer;
    size_t      start;   // byte offset in buffer
    size_t      length;  // byte count
    size_t      lines;   // newline count (cached for O(log n) line lookup)
};

struct Point { size_t line, col; };

class PieceTable {
public:
    explicit PieceTable(std::string original = "");

    // ── Core editing ─────────────────────────────────────────
    void insert(size_t char_pos, std::string_view text);
    void erase (size_t char_pos, size_t count);

    // ── Reading ──────────────────────────────────────────────
    std::string text()                              const;
    std::string line(size_t line_idx)               const;
    size_t      line_count()                        const;
    size_t      char_count()                        const;
    Point       offset_to_point(size_t char_pos)    const;
    size_t      point_to_offset(Point p)            const;

    // ── History ──────────────────────────────────────────────
    void undo();
    void redo();
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    bool is_dirty() const { return dirty_; }
    void mark_clean()     { dirty_ = false; }

private:
    // Two append-only buffers — never freed during session
    std::string original_buf_;
    std::string added_buf_;

    std::vector<Piece>  pieces_;
    size_t              total_chars_ = 0;

    // Snapshot-based undo (lightweight: only piece-list diff)
    struct Snapshot {
        std::vector<Piece> pieces;
        size_t             added_buf_size;  // added_buf_ is append-only
        size_t             total_chars;
    };
    std::vector<Snapshot> undo_stack_;
    std::vector<Snapshot> redo_stack_;
    bool dirty_ = false;

    void push_snapshot();
    std::pair<size_t, size_t> find_piece(size_t char_pos) const;  // {piece_idx, offset_within}
    size_t count_lines(std::string_view sv) const;
};

} // namespace sin
