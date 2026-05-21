package com.sineditor.app

/**
 * Kotlin wrapper around the C++ PieceTable JNI engine.
 * Each EditorEngine holds one document (one handle to a native PieceTable).
 * 
 * MEMORY SAFETY:
 * - AutoCloseable for try-with-resources
 * - Handle validation before every native call
 * - Memory pressure monitoring
 * - Explicit destroy() required
 */
class EditorEngine(initialText: String = "") : AutoCloseable {

    private var handle: Long = nCreate(initialText)

    init {
        if (handle == 0L) {
            throw OutOfMemoryError("Failed to create native PieceTable. Memory exhausted.")
        }
    }

    // ---- Text operations ----------------------------------------------------

    fun insert(charPos: Long, text: String) {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nInsert(handle, charPos.coerceAtLeast(0), text)
    }

    fun erase(charPos: Long, count: Long) {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nErase(handle, charPos.coerceAtLeast(0), count.coerceAtLeast(0))
    }

    val text: String
        get() {
            if (handle == 0L) throw IllegalStateException("Engine already destroyed")
            return nGetText(handle)
        }

    fun line(idx: Long): String {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        return nGetLine(handle, idx.coerceAtLeast(0))
    }

    val lineCount: Long
        get() {
            if (handle == 0L) throw IllegalStateException("Engine already destroyed")
            return nLineCount(handle)
        }

    val charCount: Long
        get() {
            if (handle == 0L) throw IllegalStateException("Engine already destroyed")
            return nCharCount(handle)
        }

    // ---- Caret helpers ------------------------------------------------------

    /** Returns [line, col] for a given char offset */
    fun offsetToPoint(charPos: Long): LongArray {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        return nOffsetToPoint(handle, charPos.coerceAtLeast(0))
    }

    /** Returns char offset for a given [line, col] */
    fun pointToOffset(line: Long, col: Long): Long {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        return nPointToOffset(handle, line.coerceAtLeast(0), col.coerceAtLeast(0))
    }

    // ---- History ------------------------------------------------------------

    fun undo() {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nUndo(handle)
    }

    fun redo() {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nRedo(handle)
    }

    val canUndo: Boolean
        get() {
            if (handle == 0L) return false
            return nCanUndo(handle)
        }

    val canRedo: Boolean
        get() {
            if (handle == 0L) return false
            return nCanRedo(handle)
        }

    // ---- Dirty tracking -----------------------------------------------------

    val isDirty: Boolean
        get() {
            if (handle == 0L) return false
            return nIsDirty(handle)
        }

    fun markClean() {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nMarkClean(handle)
    }

    // ---- Memory monitoring (new) -------------------------------------------

    /** Returns total native memory usage in bytes */
    val memoryUsage: Long
        get() {
            if (handle == 0L) return 0
            return nMemoryUsage(handle)
        }

    /** Returns true if memory pressure is high (approaching cap) */
    val isMemoryPressure: Boolean
        get() {
            if (handle == 0L) return false
            return nIsMemoryPressure(handle)
        }

    /** Force compaction when memory pressure is high */
    fun compact() {
        if (handle == 0L) throw IllegalStateException("Engine already destroyed")
        nCompact(handle)
    }

    // ---- Lifecycle ----------------------------------------------------------

    fun destroy() {
        if (handle != 0L) {
            nDestroy(handle)
            handle = 0L
        }
    }

    override fun close() = destroy()

    protected fun finalize() {
        // Safety net: destroy if forgotten
        if (handle != 0L) {
            android.util.Log.w("EditorEngine", "finalize() called with active handle - memory leak!")
            destroy()
        }
    }

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

    // New memory monitoring functions
    private external fun nMemoryUsage(h: Long): Long
    private external fun nIsMemoryPressure(h: Long): Boolean
    private external fun nCompact(h: Long)

    companion object {
        init {
            System.loadLibrary("sineditor_engine")
        }
    }
}
