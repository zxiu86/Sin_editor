package com.sineditor.app

/**
 * Kotlin wrapper around the C++ PieceTable JNI engine.
 * Each EditorEngine holds one document (one handle to a native PieceTable).
 * Must call [destroy] when done (use in try-with-resources or remember to call it).
 */
class EditorEngine(initialText: String = "") : AutoCloseable {

    private var handle: Long = nCreate(initialText)

    // ---- Text operations ----------------------------------------------------

    fun insert(charPos: Long, text: String) = nInsert(handle, charPos, text)
    fun erase(charPos: Long, count: Long)   = nErase(handle, charPos, count)

    val text:       String  get() = nGetText(handle)
    fun line(idx: Long): String  = nGetLine(handle, idx)
    val lineCount:  Long    get() = nLineCount(handle)
    val charCount:  Long    get() = nCharCount(handle)

    // ---- Caret helpers ------------------------------------------------------

    /** Returns [line, col] for a given char offset */
    fun offsetToPoint(charPos: Long): LongArray = nOffsetToPoint(handle, charPos)

    /** Returns char offset for a given [line, col] */
    fun pointToOffset(line: Long, col: Long): Long = nPointToOffset(handle, line, col)

    // ---- History ------------------------------------------------------------

    fun undo()           = nUndo(handle)
    fun redo()           = nRedo(handle)
    val canUndo: Boolean get() = nCanUndo(handle)
    val canRedo: Boolean get() = nCanRedo(handle)

    // ---- Dirty tracking -----------------------------------------------------

    val isDirty: Boolean get() = nIsDirty(handle)
    fun markClean()            = nMarkClean(handle)

    // ---- Lifecycle ----------------------------------------------------------

    fun destroy() {
        if (handle != 0L) {
            nDestroy(handle)
            handle = 0L
        }
    }

    override fun close() = destroy()

    // ---- JNI declarations ---------------------------------------------------

    private external fun nCreate(text: String): Long
    private external fun nDestroy(h: Long)
    private external fun nInsert(h: Long, pos: Long, text: String)
    private external fun nErase(h: Long, pos: Long, count: Long)
    private external fun nGetText(h: Long): String
    private external fun nGetLine(h: Long, lineIdx: Long): String
    private external fun nLineCount(h: Long): Long
    private external fun nCharCount(h: Long): Long
    private external fun nOffsetToPoint(h: Long, pos: Long): LongArray
    private external fun nPointToOffset(h: Long, line: Long, col: Long): Long
    private external fun nUndo(h: Long)
    private external fun nRedo(h: Long)
    private external fun nIsDirty(h: Long): Boolean
    private external fun nMarkClean(h: Long)
    private external fun nCanUndo(h: Long): Boolean
    private external fun nCanRedo(h: Long): Boolean

    companion object {
        init {
            System.loadLibrary("sineditor_engine")
        }
    }
}
