#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>

// ============================================================
//  SIN Editor — Piece Table Text Buffer (Android Optimized)
//  O(1) insert/delete via append-only buffers + piece list
//  Memory-capped undo/redo for mobile constraints
// ============================================================

namespace sino {

// Memory limits for mobile (configurable)
constexpr size_t MAX_ADDED_BUFFER_SIZE = 8 * 1024 * 1024;   // 8 MB cap
constexpr size_t MAX_UNDO_SNAPSHOTS    = 50;                // Max undo levels
constexpr size_t MAX_REDO_SNAPSHOTS    = 25;                // Max redo levels
constexpr size_t MEMORY_PRESSURE_THRESHOLD = 6 * 1024 * 1024; // 6 MB warning

enum class BufferType : uint8_t { Original, Added };

struct Piece {
    BufferType  buffer;
    size_t      start;   // byte offset in buffer
    size_t      length;  // byte count
    size_t      lines;   // newline count (cached for O(log n) line lookup)
};

struct Point { size_t line, col; };

// Forward declarations for memory tracking
class MemoryTracker;

class PieceTable {
public:
    explicit PieceTable(std::string original = "");
    ~PieceTable();

    // Disable copy to prevent accidental expensive copies
    PieceTable(const PieceTable&) = delete;
    PieceTable& operator=(const PieceTable&) = delete;

    // Enable move for efficient transfers
    PieceTable(PieceTable&&) noexcept;
    PieceTable& operator=(PieceTable&&) noexcept;

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

    // ── Memory management ────────────────────────────────────
    size_t memory_usage() const;
    bool is_memory_pressure() const { return added_buf_.size() > MEMORY_PRESSURE_THRESHOLD; }
    void compact();  // Force compaction when memory pressure is high

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

    // Memory management helpers
    void enforce_memory_limits();
    void trim_undo_stack();
    void trim_redo_stack();
};

} // namespace sino
