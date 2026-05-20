#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <raylib.h>

// ============================================================
//  SIN Editor — SINO Language Syntax Highlighter
//  Token-based, single-pass, cache-invalidated per-line
// ============================================================

namespace sin {

enum class TokenType : uint8_t {
    Normal,
    Keyword,
    TypeName,
    Number,
    String,
    Comment,
    Operator,
    Identifier,
    BuiltIn,
};

struct Token {
    size_t      start;   // char offset within line
    size_t      length;
    TokenType   type;
};

// Color palette — cyberpunk dark
struct SyntaxColors {
    Color normal     = {220, 220, 200, 255};   // warm white
    Color keyword    = {86,  182, 194, 255};   // cyan
    Color type_name  = {78,  201, 176, 255};   // teal
    Color number     = {181, 206, 168, 255};   // soft green
    Color string_col = {206, 145, 120, 255};   // warm orange
    Color comment    = {106, 153,  85, 255};   // green comment
    Color op_col     = {212, 212, 212, 255};   // light gray
    Color builtin    = {220, 120, 220, 255};   // purple
    Color active_ln  = {30,  160, 200, 255};   // light blue gutter highlight
    Color gutter_bg  = {20,   20,  28, 255};
    Color editor_bg  = {14,   14,  20, 255};
    Color active_bg  = {25,   35,  50, 120};   // active line tint
    Color tab_active = {30,   30,  45, 255};
    Color tab_bg     = {18,   18,  28, 255};
    Color header_bg  = {12,   12,  18, 255};
    Color accent     = {0,   200, 160, 255};   // SINO green
};

inline const SyntaxColors COLORS;

// SINO language keywords
inline constexpr std::string_view KEYWORDS[] = {
    "fn", "let", "mut", "const", "if", "else", "elif", "while",
    "for", "in", "return", "import", "export", "struct", "enum",
    "match", "case", "break", "continue", "nil", "true", "false",
    "and", "or", "not", "is", "as", "use", "pub", "priv",
};
inline constexpr std::string_view TYPES[] = {
    "int", "float", "str", "bool", "void", "byte", "uint",
    "int32", "int64", "float32", "float64", "char", "list", "map",
};
inline constexpr std::string_view BUILTINS[] = {
    "print", "println", "input", "len", "range", "type",
    "str", "int", "float", "open", "close", "read", "write",
    "append", "pop", "push", "keys", "values", "exit",
};

class Highlighter {
public:
    // Tokenize a single line — called lazily, result cached by editor
    std::vector<Token> tokenize(std::string_view line) const;

    Color color_for(TokenType t) const;
};

// ── Inline implementations (header-only, no separate .cpp needed) ─────────────

inline std::vector<Token> Highlighter::tokenize(std::string_view line) const {
    std::vector<Token> tokens;
    tokens.reserve(32);

    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        // Skip whitespace
        if (line[i] == ' ' || line[i] == '\t') { ++i; continue; }

        // Comment
        if (i + 1 < n && line[i] == '/' && line[i+1] == '/') {
            tokens.push_back({i, n - i, TokenType::Comment});
            break;
        }

        // String literal
        if (line[i] == '"' || line[i] == '\'') {
            char delim = line[i];
            size_t start = i++;
            while (i < n && line[i] != delim) {
                if (line[i] == '\\') ++i;  // escape
                ++i;
            }
            if (i < n) ++i;  // closing quote
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Number
        if (std::isdigit(static_cast<unsigned char>(line[i])) ||
            (line[i] == '-' && i + 1 < n && std::isdigit(static_cast<unsigned char>(line[i+1])))) {
            size_t start = i++;
            while (i < n && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == '_'))
                ++i;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Operator
        if (!std::isalnum(static_cast<unsigned char>(line[i])) && line[i] != '_') {
            tokens.push_back({i, 1, TokenType::Operator});
            ++i;
            continue;
        }

        // Identifier / keyword / type / builtin
        size_t start = i;
        while (i < n && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_'))
            ++i;
        std::string_view word = line.substr(start, i - start);

        TokenType tt = TokenType::Identifier;
        for (auto kw : KEYWORDS)   if (kw == word) { tt = TokenType::Keyword;  break; }
        for (auto ty : TYPES)      if (ty == word) { tt = TokenType::TypeName;  break; }
        for (auto bi : BUILTINS)   if (bi == word) { tt = TokenType::BuiltIn;   break; }

        tokens.push_back({start, i - start, tt});
    }

    return tokens;
}

inline Color Highlighter::color_for(TokenType t) const {
    switch (t) {
        case TokenType::Keyword:    return COLORS.keyword;
        case TokenType::TypeName:   return COLORS.type_name;
        case TokenType::Number:     return COLORS.number;
        case TokenType::String:     return COLORS.string_col;
        case TokenType::Comment:    return COLORS.comment;
        case TokenType::Operator:   return COLORS.op_col;
        case TokenType::BuiltIn:    return COLORS.builtin;
        default:                    return COLORS.normal;
    }
}

} // namespace sin
