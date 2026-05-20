#include "piece_table.h"
#include <algorithm>
#include <stdexcept>
#include <numeric>

namespace sin {

// ── Helpers ──────────────────────────────────────────────────────────────────

size_t PieceTable::count_lines(std::string_view sv) const {
    return static_cast<size_t>(std::count(sv.begin(), sv.end(), '\n'));
}

void PieceTable::push_snapshot() {
    undo_stack_.push_back({pieces_, added_buf_.size(), total_chars_});
    redo_stack_.clear();   // branching history
    dirty_ = true;
}

// Returns {piece_index, byte_offset_within_piece}
std::pair<size_t, size_t> PieceTable::find_piece(size_t char_pos) const {
    size_t acc = 0;
    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (char_pos <= acc + pieces_[i].length)
            return {i, char_pos - acc};
        acc += pieces_[i].length;
    }
    return {pieces_.size(), 0};  // end of document
}

// ── Constructor ───────────────────────────────────────────────────────────────

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
}

// ── Insert ────────────────────────────────────────────────────────────────────

void PieceTable::insert(size_t char_pos, std::string_view text) {
    if (text.empty()) return;
    push_snapshot();

    size_t added_start = added_buf_.size();
    added_buf_.append(text);
    Piece new_piece{BufferType::Added, added_start, text.size(), count_lines(text)};
    total_chars_ += text.size();

    auto [idx, off] = find_piece(char_pos);

    if (idx == pieces_.size()) {
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
        Piece  left  = {p.buffer, p.start,        off,              count_lines({added_buf_.data() + p.start, off})};
        Piece  right = {p.buffer, p.start + off,   p.length - off,   p.lines - left.lines};
        if (p.buffer == BufferType::Original) {
            left.lines  = count_lines({original_buf_.data() + p.start, off});
            right.lines = p.lines - left.lines;
        }
        pieces_.erase(pieces_.begin() + idx);
        pieces_.insert(pieces_.begin() + idx, {right, new_piece, left});  // reversed insert
        // Fix: insert in correct order
        pieces_.erase(pieces_.begin() + idx, pieces_.begin() + idx + 3);
        pieces_.insert(pieces_.begin() + idx, left);
        pieces_.insert(pieces_.begin() + idx + 1, new_piece);
        pieces_.insert(pieces_.begin() + idx + 2, right);
    }
}

// ── Erase ─────────────────────────────────────────────────────────────────────

void PieceTable::erase(size_t char_pos, size_t count) {
    if (count == 0) return;
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
            p.lines  = count_lines(p.buffer == BufferType::Original
                ? std::string_view(original_buf_.data() + p.start, off0)
                : std::string_view(added_buf_.data()   + p.start, off0));
            rebuilt.push_back(p);
        }
        if (i == i1 && off1 < pieces_[i].length) {
            // Keep right portion
            Piece p = pieces_[i];
            p.start  += off1;
            p.length -= off1;
            p.lines   = count_lines(p.buffer == BufferType::Original
                ? std::string_view(original_buf_.data() + p.start, p.length)
                : std::string_view(added_buf_.data()   + p.start, p.length));
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
    auto& snap = undo_stack_.back();
    pieces_      = snap.pieces;
    total_chars_ = snap.total_chars;
    // added_buf_ is append-only; no shrink needed — pieces won't reference future data
    undo_stack_.pop_back();
    dirty_ = !undo_stack_.empty();
}

void PieceTable::redo() {
    if (redo_stack_.empty()) return;
    undo_stack_.push_back({pieces_, added_buf_.size(), total_chars_});
    auto& snap = redo_stack_.back();
    pieces_      = snap.pieces;
    total_chars_ = snap.total_chars;
    redo_stack_.pop_back();
    dirty_ = true;
}

} // namespace sin
