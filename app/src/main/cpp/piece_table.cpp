#include "piece_table.h"
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <android/log.h>

namespace sino {

#define TAG "SINEditorNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ── Helpers ──────────────────────────────────────────────────────────────────

size_t PieceTable::count_lines(std::string_view sv) const {
    return static_cast<size_t>(std::count(sv.begin(), sv.end(), '\n'));
}

void PieceTable::push_snapshot() {
    // Trim stacks before pushing to maintain memory limits
    trim_undo_stack();
    trim_redo_stack();

    undo_stack_.push_back({pieces_, added_buf_.size(), total_chars_});
    redo_stack_.clear();   // branching history
    dirty_ = true;

    LOGI("Snapshot pushed. Undo: %zu, Added buf: %zu bytes", 
         undo_stack_.size(), added_buf_.size());
}

void PieceTable::trim_undo_stack() {
    while (undo_stack_.size() >= MAX_UNDO_SNAPSHOTS) {
        undo_stack_.erase(undo_stack_.begin());
        LOGW("Undo stack trimmed (memory pressure)");
    }
}

void PieceTable::trim_redo_stack() {
    while (redo_stack_.size() >= MAX_REDO_SNAPSHOTS) {
        redo_stack_.erase(redo_stack_.begin());
    }
}

void PieceTable::enforce_memory_limits() {
    // If added buffer exceeds cap, force compaction by clearing history
    if (added_buf_.size() > MAX_ADDED_BUFFER_SIZE) {
        LOGW("Memory limit reached (%zu > %zu). Compacting...", 
             added_buf_.size(), MAX_ADDED_BUFFER_SIZE);

        // Materialize current text and reset
        std::string current = text();
        original_buf_ = std::move(current);
        added_buf_.clear();
        added_buf_.shrink_to_fit();

        pieces_.clear();
        if (!original_buf_.empty()) {
            pieces_.push_back({
                BufferType::Original, 0,
                original_buf_.size(),
                count_lines(original_buf_)
            });
        }

        // Clear history since buffers changed
        undo_stack_.clear();
        redo_stack_.clear();
        total_chars_ = original_buf_.size();

        LOGI("Compaction complete. New size: %zu bytes", total_chars_);
    }
}

// Returns {piece_index, byte_offset_within_piece}
std::pair<size_t, size_t> PieceTable::find_piece(size_t char_pos) const {
    if (pieces_.empty()) return {0, 0};

    size_t acc = 0;
    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (char_pos < acc + pieces_[i].length)
            return {i, char_pos - acc};
        acc += pieces_[i].length;
    }
    return {pieces_.size(), 0};  // end of document
}

// ── Constructor / Destructor ───────────────────────────────────────────────────

PieceTable::PieceTable(std::string original)
    : original_buf_(std::move(original))
{
    if (!original_buf_.empty()) {
        pieces_.push_back({
            BufferType::Original, 0,
            original_buf_.size(),
            count_lines(original_buf_)
        });
        total_chars_ = original_buf_.size();
    }
    LOGI("PieceTable created. Size: %zu bytes, Lines: %zu", 
         total_chars_, line_count());
}

PieceTable::~PieceTable() {
    LOGI("PieceTable destroyed. Final size: %zu bytes", total_chars_);
}

// Move constructor
PieceTable::PieceTable(PieceTable&& other) noexcept
    : original_buf_(std::move(other.original_buf_))
    , added_buf_(std::move(other.added_buf_))
    , pieces_(std::move(other.pieces_))
    , total_chars_(other.total_chars_)
    , undo_stack_(std::move(other.undo_stack_))
    , redo_stack_(std::move(other.redo_stack_))
    , dirty_(other.dirty_)
{
    other.total_chars_ = 0;
    other.dirty_ = false;
}

PieceTable& PieceTable::operator=(PieceTable&& other) noexcept {
    if (this != &other) {
        original_buf_ = std::move(other.original_buf_);
        added_buf_ = std::move(other.added_buf_);
        pieces_ = std::move(other.pieces_);
        total_chars_ = other.total_chars_;
        undo_stack_ = std::move(other.undo_stack_);
        redo_stack_ = std::move(other.redo_stack_);
        dirty_ = other.dirty_;
        other.total_chars_ = 0;
        other.dirty_ = false;
    }
    return *this;
}

// ── Insert ────────────────────────────────────────────────────────────────────

void PieceTable::insert(size_t char_pos, std::string_view text) {
    if (text.empty()) return;

    // Bounds check
    if (char_pos > total_chars_) {
        LOGE("Insert out of bounds: pos=%zu, total=%zu", char_pos, total_chars_);
        char_pos = total_chars_;  // Clamp to end
    }

    push_snapshot();

    size_t added_start = added_buf_.size();
    added_buf_.append(text);

    // Check memory limits after append
    enforce_memory_limits();

    Piece new_piece{BufferType::Added, added_start, text.size(), count_lines(text)};
    total_chars_ += text.size();

    auto [idx, off] = find_piece(char_pos);

    if (idx >= pieces_.size()) {
        pieces_.push_back(new_piece);
        return;
    }

    if (off == 0) {
        // Insert before piece[idx]
        pieces_.insert(pieces_.begin() + idx, new_piece);
    } else if (off == pieces_[idx].length) {
        // Insert after piece[idx]
        pieces_.insert(pieces_.begin() + idx + 1, new_piece);
    } else {
        // Split piece[idx] at off
        Piece& p = pieces_[idx];

        // Calculate line counts for split pieces
        size_t left_lines = 0, right_lines = 0;
        const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;

        for (size_t i = p.start; i < p.start + off; ++i) {
            if (buf[i] == '\n') ++left_lines;
        }
        right_lines = p.lines - left_lines;

        Piece left  = {p.buffer, p.start,        off,              left_lines};
        Piece right = {p.buffer, p.start + off,   p.length - off,   right_lines};

        pieces_.erase(pieces_.begin() + idx);
        pieces_.insert(pieces_.begin() + idx, left);
        pieces_.insert(pieces_.begin() + idx + 1, new_piece);
        pieces_.insert(pieces_.begin() + idx + 2, right);
    }
}

// ── Erase ─────────────────────────────────────────────────────────────────────

void PieceTable::erase(size_t char_pos, size_t count) {
    if (count == 0) return;

    // Bounds check
    if (char_pos >= total_chars_) {
        LOGE("Erase out of bounds: pos=%zu, total=%zu", char_pos, total_chars_);
        return;
    }
    if (char_pos + count > total_chars_) {
        count = total_chars_ - char_pos;
    }

    push_snapshot();
    total_chars_ -= count;

    size_t end = char_pos + count;
    auto [i0, off0] = find_piece(char_pos);
    auto [i1, off1] = find_piece(end);

    // Clamp i1 to valid piece
    if (i1 > pieces_.size()) i1 = pieces_.size();

    std::vector<Piece> rebuilt;
    rebuilt.reserve(pieces_.size());

    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (i < i0 || i > i1) {
            rebuilt.push_back(pieces_[i]);
            continue;
        }
        if (i == i0 && off0 > 0) {
            // Keep left portion
            Piece p = pieces_[i];
            p.length = off0;

            const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
            p.lines = 0;
            for (size_t j = p.start; j < p.start + off0; ++j) {
                if (buf[j] == '\n') ++p.lines;
            }
            rebuilt.push_back(p);
        }
        if (i == i1 && i1 < pieces_.size() && off1 < pieces_[i].length) {
            // Keep right portion
            Piece p = pieces_[i];
            p.start  += off1;
            p.length -= off1;

            const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
            p.lines = 0;
            for (size_t j = p.start; j < p.start + p.length; ++j) {
                if (buf[j] == '\n') ++p.lines;
            }
            rebuilt.push_back(p);
        }
    }
    pieces_ = std::move(rebuilt);
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::string PieceTable::text() const {
    std::string out;
    out.reserve(total_chars_);
    for (const auto& p : pieces_) {
        const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
        out.append(buf, p.start, p.length);
    }
    return out;
}

std::string PieceTable::line(size_t line_idx) const {
    // Reconstruct only up to needed line — avoids full text() call
    std::string result;
    size_t current_line = 0;
    bool   capturing    = false;

    for (const auto& p : pieces_) {
        const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
        for (size_t i = p.start; i < p.start + p.length; ++i) {
            if (current_line == line_idx) {
                capturing = true;
                if (buf[i] == '\n') return result;
                result += buf[i];
            } else if (buf[i] == '\n') {
                ++current_line;
                if (current_line > line_idx) return result;
            }
        }
    }
    return result;
}

size_t PieceTable::line_count() const {
    size_t n = 0;
    for (const auto& p : pieces_) n += p.lines;
    return n + 1;  // last line has no trailing \n
}

size_t PieceTable::char_count() const { return total_chars_; }

Point PieceTable::offset_to_point(size_t char_pos) const {
    Point pt{0, 0};
    if (char_pos > total_chars_) char_pos = total_chars_;

    size_t acc = 0;
    for (const auto& p : pieces_) {
        const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
        for (size_t i = p.start; i < p.start + p.length && acc < char_pos; ++i, ++acc) {
            if (buf[i] == '\n') { ++pt.line; pt.col = 0; }
            else                  ++pt.col;
        }
        if (acc >= char_pos) break;
    }
    return pt;
}

size_t PieceTable::point_to_offset(Point p_) const {
    size_t offset = 0, line = 0, col = 0;
    for (const auto& p : pieces_) {
        const std::string& buf = (p.buffer == BufferType::Original) ? original_buf_ : added_buf_;
        for (size_t i = p.start; i < p.start + p.length; ++i) {
            if (line == p_.line && col == p_.col) return offset;
            if (buf[i] == '\n') { ++line; col = 0; } else ++col;
            ++offset;
        }
    }
    return offset;
}

// ── History ───────────────────────────────────────────────────────────────────

void PieceTable::undo() {
    if (undo_stack_.empty()) return;
    redo_stack_.push_back({pieces_, added_buf_.size(), total_chars_});
    trim_redo_stack();

    auto& snap = undo_stack_.back();
    pieces_      = snap.pieces;
    total_chars_ = snap.total_chars;
    // added_buf_ is append-only; no shrink needed — pieces won't reference future data
    undo_stack_.pop_back();
    dirty_ = !undo_stack_.empty();

    LOGI("Undo performed. Remaining undo: %zu", undo_stack_.size());
}

void PieceTable::redo() {
    if (redo_stack_.empty()) return;
    undo_stack_.push_back({pieces_, added_buf_.size(), total_chars_});
    trim_undo_stack();

    auto& snap = redo_stack_.back();
    pieces_      = snap.pieces;
    total_chars_ = snap.total_chars;
    redo_stack_.pop_back();
    dirty_ = true;

    LOGI("Redo performed. Remaining redo: %zu", redo_stack_.size());
}

// ── Memory Management ─────────────────────────────────────────────────────────

size_t PieceTable::memory_usage() const {
    size_t total = original_buf_.capacity() + added_buf_.capacity();
    total += pieces_.capacity() * sizeof(Piece);
    total += undo_stack_.capacity() * sizeof(Snapshot);
    total += redo_stack_.capacity() * sizeof(Snapshot);
    for (const auto& snap : undo_stack_) {
        total += snap.pieces.capacity() * sizeof(Piece);
    }
    for (const auto& snap : redo_stack_) {
        total += snap.pieces.capacity() * sizeof(Piece);
    }
    return total;
}

void PieceTable::compact() {
    LOGI("Manual compaction requested. Current: %zu bytes", memory_usage());
    enforce_memory_limits();
}

} // namespace sino
