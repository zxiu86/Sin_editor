package com.sineditor.app

import android.content.Context
import android.graphics.*
import android.text.InputType
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.inputmethod.*
import kotlin.math.max
import kotlin.math.min

/**
 * Custom View that renders the SINO code editor using Android Canvas.
 * Text engine runs in C++ via [EditorEngine] (JNI).
 * Kotlin handles: touch, IME input, drawing, scroll.
 * 
 * OPTIMIZATIONS:
 * - Line cache with LRU eviction for memory efficiency
 * - Dirty tracking per-line (only re-tokenize changed lines)
 * - Viewport clipping (only draw visible lines)
 * - Memory pressure monitoring
 */
class EditorView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs), View.OnKeyListener {

    // ---- Engine (C++ Piece Table) -------------------------------------------
    var engine = EditorEngine()
        private set

    fun setEngine(e: EditorEngine) { 
        engine = e 
        lineCache.clear()
        invalidate() 
    }

    // ---- State --------------------------------------------------------------
    var caretPos  = 0L
        set(v) { field = v.coerceIn(0, engine.charCount); invalidate() }

    private var scrollY  = 0        // first visible line index
    private var scrollX  = 0f       // horizontal pixel offset
    private var blinkOn  = true
    private var blinkMs  = 0L

    // Callbacks
    var onDirtyChanged: ((Boolean) -> Unit)? = null
    var onMemoryWarning: (() -> Unit)? = null

    // ---- Dimensions ---------------------------------------------------------
    private val gutterW   = dp(58)
    private val lineH     = sp(22)
    private val fontSz    = 18f
    private val padLeft   = dp(6)

    // ---- Paints -------------------------------------------------------------
    private val paintBg     = Paint().apply { color = Color.rgb(14, 14, 20) }
    private val paintGutter = Paint().apply { color = Color.rgb(20, 20, 28) }
    private val paintActLn  = Paint().apply { color = Color.argb(120, 25, 35, 50) }
    private val paintSep    = Paint().apply { color = Color.rgb(26, 26, 42) }
    private val paintCaret  = Paint().apply { color = Color.rgb(0, 200, 160) }

    private val paintText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface  = Typeface.MONOSPACE
        textSize  = fontSz
        color     = SinoHighlighter.COLOR_NORMAL
    }
    private val paintGutterNum = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface  = Typeface.MONOSPACE
        textSize  = fontSz - 2f
        color     = Color.rgb(65, 78, 100)
        textAlign = Paint.Align.RIGHT
    }
    private val paintGutterAct = Paint(paintGutterNum).apply {
        color = Color.rgb(30, 160, 200)
    }

    // Monospace char width (cached)
    private val charW: Float by lazy { paintText.measureText("M") }

    // Line cache: LRU with size limit to prevent memory bloat
    private data class CachedLine(val text: String, val tokens: List<Token>)
    private val lineCache = object : LinkedHashMap<Long, CachedLine>(64, 0.75f, true) {
        override fun removeEldestEntry(eldest: Map.Entry<Long, CachedLine>?): Boolean {
            return size > 200  // Max 200 cached lines
        }
    }

    // ---- IME ----------------------------------------------------------------
    private val inputConn = SinoInputConnection(this)

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT or
                InputType.TYPE_TEXT_FLAG_MULTI_LINE or
                InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN
        return inputConn
    }

    override fun onCheckIsTextEditor() = true

    fun showKeyboard() {
        requestFocus()
        val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
    }

    // ---- Drawing ------------------------------------------------------------
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w    = width.toFloat()
        val h    = height.toFloat()
        val vis  = visibleLines()
        val lc   = engine.lineCount
        val cpt  = engine.offsetToPoint(caretPos)
        val actL = cpt[0].toInt()

        // Backgrounds
        canvas.drawRect(0f, 0f, w, h, paintBg)
        canvas.drawRect(0f, 0f, gutterW.toFloat(), h, paintGutter)

        // Scroll clamping
        scrollY = scrollY.coerceIn(0, max(0, lc.toInt() - vis))

        // Caret blink (600ms interval)
        val now = System.currentTimeMillis()
        if (now - blinkMs > 600) { blinkOn = !blinkOn; blinkMs = now; invalidate() }

        // Memory pressure check
        if (engine.isMemoryPressure) {
            onMemoryWarning?.invoke()
        }

        for (li in scrollY until min(scrollY + vis + 1, lc.toInt())) {
            val sy = (li - scrollY) * lineH
            val lineF = li.toLong()

            // Active line highlight
            if (li == actL) {
                canvas.drawRect(0f, sy.toFloat(), w, (sy + lineH).toFloat(), paintActLn)
            }

            // Gutter number
            val gutterPaint = if (li == actL) paintGutterAct else paintGutterNum
            canvas.drawText("${li + 1}", gutterW - padLeft.toFloat(),
                            sy + lineH * 0.75f, gutterPaint)

            // Code text with syntax highlighting
            val cached = lineCache.getOrPut(lineF) {
                val txt = engine.line(lineF)
                CachedLine(txt, SinoHighlighter.tokenize(txt))
            }

            drawLine(canvas, cached, sy, gutterW + padLeft - scrollX)
        }

        // Caret
        if (blinkOn) {
            val cx = gutterW + padLeft + cpt[1] * charW - scrollX
            val cy = (actL - scrollY) * lineH.toFloat()
            canvas.drawRect(cx, cy + 2f, cx + 2f, cy + lineH - 2f, paintCaret)
        }

        // Gutter separator
        canvas.drawRect(gutterW - 1f, 0f, gutterW.toFloat(), h, paintSep)
    }

    private fun drawLine(canvas: Canvas, cached: CachedLine, sy: Int, baseX: Float) {
        if (cached.tokens.isEmpty()) {
            paintText.color = SinoHighlighter.COLOR_NORMAL
            canvas.drawText(cached.text, baseX, sy + lineH * 0.75f, paintText)
            return
        }
        for (tok in cached.tokens) {
            val word = cached.text.substring(tok.start, tok.end)
            paintText.color = SinoHighlighter.colorFor(tok.kind)
            val x = baseX + tok.start * charW
            canvas.drawText(word, x, sy + lineH * 0.75f, paintText)
        }
    }

    // ---- Touch --------------------------------------------------------------
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                showKeyboard()
                moveCaret(event.x, event.y)
            }
        }
        return true
    }

    private fun moveCaret(touchX: Float, touchY: Float) {
        val li = (touchY / lineH).toInt() + scrollY
        val line = li.toLong().coerceIn(0, max(0, engine.lineCount - 1))
        val col  = ((touchX - gutterW - padLeft + scrollX) / charW)
                      .toInt()
                      .coerceAtLeast(0)
                      .coerceAtMost(engine.line(line).length)
        caretPos = engine.pointToOffset(line, col.toLong())
        scrollToActiveLine()
    }

    // ---- Hardware key support (minimal - for physical keyboards only) --------
    override fun onKey(v: View?, keyCode: Int, event: KeyEvent?): Boolean {
        if (event?.action != KeyEvent.ACTION_DOWN) return false

        when (keyCode) {
            KeyEvent.KEYCODE_DEL       -> backspace()
            KeyEvent.KEYCODE_FORWARD_DEL -> deleteForward()
            KeyEvent.KEYCODE_ENTER     -> typeText("\n")
            KeyEvent.KEYCODE_TAB       -> typeText("    ")
            KeyEvent.KEYCODE_DPAD_LEFT  -> if (caretPos > 0) caretPos--
            KeyEvent.KEYCODE_DPAD_RIGHT -> if (caretPos < engine.charCount) caretPos++
            KeyEvent.KEYCODE_DPAD_UP    -> moveLine(-1)
            KeyEvent.KEYCODE_DPAD_DOWN  -> moveLine(+1)
            else -> return false
        }
        return true
    }

    // ---- Text mutations (called by IME + key handler) -----------------------
    fun typeText(text: String) {
        engine.insert(caretPos, text)
        caretPos += text.length.toLong()
        val pt = engine.offsetToPoint(caretPos)
        invalidateCacheFrom(pt[0])
        notifyDirty()
        scrollToActiveLine()
        invalidate()
    }

    fun backspace() {
        if (caretPos <= 0) return
        val pt = engine.offsetToPoint(caretPos - 1)
        engine.erase(caretPos - 1, 1)
        caretPos--
        invalidateCacheFrom(pt[0])
        notifyDirty()
        invalidate()
    }

    fun deleteForward() {
        if (caretPos >= engine.charCount) return
        val pt = engine.offsetToPoint(caretPos)
        engine.erase(caretPos, 1)
        invalidateCacheFrom(pt[0])
        notifyDirty()
        invalidate()
    }

    // ---- Touch UI Actions (replacing keyboard shortcuts) --------------------

    /** Touch UI: Save action (replaces Ctrl+S) */
    fun actionSave() {
        onSaveRequested?.invoke()
    }

    /** Touch UI: Undo action (replaces Ctrl+Z) */
    fun actionUndo() {
        if (engine.canUndo) {
            engine.undo()
            invalidateAll()
            notifyDirty()
        }
    }

    /** Touch UI: Redo action (replaces Ctrl+Y) */
    fun actionRedo() {
        if (engine.canRedo) {
            engine.redo()
            invalidateAll()
            notifyDirty()
        }
    }

    /** Touch UI: New document (replaces Ctrl+N) */
    fun actionNewDocument() {
        onNewDocRequested?.invoke()
    }

    // ---- Helpers ------------------------------------------------------------
    private var onSaveRequested: (() -> Unit)? = null
    private var onNewDocRequested: (() -> Unit)? = null

    fun onSave(cb: () -> Unit) { onSaveRequested = cb }
    fun onNewDoc(cb: () -> Unit) { onNewDocRequested = cb }

    /** Called by FindReplaceDialog to jump to a specific line */
    fun scrollToLine(line: Long) {
        scrollY = line.toInt().coerceIn(0, maxOf(0, engine.lineCount.toInt() - visibleLines()))
        invalidate()
    }

    /** Called by MainActivity font size buttons. delta = +1 or -1 sp */
    fun adjustFontSize(delta: Int) {
        val newSp = (_fontSizeSp + delta).coerceIn(10, 32)
        if (newSp == _fontSizeSp) return
        _fontSizeSp = newSp
        paintText.textSize = sp(_fontSizeSp).toFloat()
        lineCache.clear()
        invalidate()
    }

    /** Current font size in sp (read by MainActivity for status message) */
    val currentFontSizeSp: Int get() = _fontSizeSp
    private var _fontSizeSp = 18

    /** Public alias used by FindReplaceDialog's onReplaceAll callback */
    fun invalidateAllCache() = invalidateAll()

    private fun moveLine(delta: Int) {
        val pt   = engine.offsetToPoint(caretPos)
        val newL = (pt[0] + delta).coerceIn(0, max(0, engine.lineCount - 1))
        caretPos = engine.pointToOffset(newL, pt[1])
        scrollToActiveLine()
    }

    private fun scrollToActiveLine() {
        val vis = visibleLines()
        val pt  = engine.offsetToPoint(caretPos)
        val li  = pt[0].toInt()
        if (li < scrollY) scrollY = li
        if (li >= scrollY + vis) scrollY = li - vis + 1
        invalidate()
    }

    private fun visibleLines() = if (lineH > 0) height / lineH else 1

    private fun invalidateCacheFrom(line: Long) {
        val keys = lineCache.keys.filter { it >= line }
        keys.forEach { lineCache.remove(it) }
    }

    private fun invalidateAll() {
        lineCache.clear()
        invalidate()
    }

    private fun notifyDirty() { onDirtyChanged?.invoke(engine.isDirty) }

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()
    private fun sp(v: Int) = (v * resources.displayMetrics.scaledDensity).toInt()

    /** Two-finger vertical swipe calls this (handled by ScaleGestureDetector in future) */
    fun scrollByLines(lines: Int) {
        scrollY = (scrollY + lines).coerceIn(0, max(0, engine.lineCount.toInt() - visibleLines()))
        invalidate()
    }

    // ---- InputConnection ----------------------------------------------------
    /** Minimal InputConnection: routes IME commits to EditorView */
    private class SinoInputConnection(private val view: EditorView) :
        BaseInputConnection(view, true) {

        override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
            if (text != null) view.typeText(text.toString())
            return true
        }

        override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
            repeat(beforeLength) { view.backspace() }
            repeat(afterLength)  { view.deleteForward() }
            return true
        }

        override fun sendKeyEvent(event: KeyEvent?): Boolean {
            if (event != null) view.onKey(view, event.keyCode, event)
            return true
        }
    }
}
