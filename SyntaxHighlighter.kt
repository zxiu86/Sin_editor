package com.sineditor.app

import android.graphics.Color

/** Token types matching the C++ highlighter */
enum class TokKind { NORMAL, KEYWORD, TYPE, NUMBER, STRING, COMMENT, OPERATOR, BUILTIN }

data class Token(val start: Int, val end: Int, val kind: TokKind)

object SinoHighlighter {

    // Cyberpunk palette -- matches the C++ THEME colours
    val COLOR_NORMAL   = Color.rgb(220, 220, 200)
    val COLOR_KEYWORD  = Color.rgb( 86, 182, 194)
    val COLOR_TYPE     = Color.rgb( 78, 201, 176)
    val COLOR_NUMBER   = Color.rgb(181, 206, 168)
    val COLOR_STRING   = Color.rgb(206, 145, 120)
    val COLOR_COMMENT  = Color.rgb(106, 153,  85)
    val COLOR_OP       = Color.rgb(212, 212, 212)
    val COLOR_BUILTIN  = Color.rgb(220, 120, 220)

    fun colorFor(kind: TokKind): Int = when (kind) {
        TokKind.KEYWORD  -> COLOR_KEYWORD
        TokKind.TYPE     -> COLOR_TYPE
        TokKind.NUMBER   -> COLOR_NUMBER
        TokKind.STRING   -> COLOR_STRING
        TokKind.COMMENT  -> COLOR_COMMENT
        TokKind.OPERATOR -> COLOR_OP
        TokKind.BUILTIN  -> COLOR_BUILTIN
        else             -> COLOR_NORMAL
    }

    private val KEYWORDS  = setOf(
        "fn","let","mut","const","if","else","elif","while","for","in",
        "return","import","export","struct","enum","match","case","break",
        "continue","nil","true","false","and","or","not","is","as",
        "use","pub","priv","self","super"
    )
    private val TYPES = setOf(
        "int","float","str","bool","void","byte","uint","char",
        "int32","int64","float32","float64","list","map","any"
    )
    private val BUILTINS = setOf(
        "print","println","input","len","range","type","open","close",
        "read","write","append","pop","push","keys","values","exit","assert"
    )

    fun tokenize(line: String): List<Token> {
        val toks = mutableListOf<Token>()
        val n = line.length
        var i = 0

        while (i < n) {
            val c = line[i]

            // Whitespace
            if (c == ' ' || c == '\t') { i++; continue }

            // Comment
            if (i + 1 < n && c == '/' && line[i + 1] == '/') {
                toks += Token(i, n, TokKind.COMMENT); break
            }

            // String literal
            if (c == '"' || c == '\'') {
                val delim = c; val s = i++
                while (i < n && line[i] != delim) { if (line[i] == '\\') i++; i++ }
                if (i < n) i++
                toks += Token(s, i, TokKind.STRING); continue
            }

            // Number
            if (c.isDigit() || (c == '-' && i + 1 < n && line[i + 1].isDigit())) {
                val s = i++
                while (i < n && (line[i].isDigit() || line[i] == '.' || line[i] == '_')) i++
                toks += Token(s, i, TokKind.NUMBER); continue
            }

            // Identifier / keyword / type / builtin
            if (c.isLetter() || c == '_') {
                val s = i
                while (i < n && (line[i].isLetterOrDigit() || line[i] == '_')) i++
                val word = line.substring(s, i)
                val kind = when {
                    word in KEYWORDS -> TokKind.KEYWORD
                    word in TYPES    -> TokKind.TYPE
                    word in BUILTINS -> TokKind.BUILTIN
                    else             -> TokKind.NORMAL
                }
                toks += Token(s, i, kind); continue
            }

            // Operator / punctuation
            toks += Token(i, i + 1, TokKind.OPERATOR); i++
        }
        return toks
    }
}
