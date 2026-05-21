package com.sineditor.app

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.text.Editable
import android.text.TextWatcher
import android.view.*
import android.widget.*

/**
 * Floating Find / Replace dialog for SIN Editor.
 * Reports match positions back to the caller via callbacks.
 */
class FindReplaceDialog(
    context: Context,
    private val engine: EditorEngine,
    private val onNavigate: (caretPos: Long, scrollToLine: Long) -> Unit,
    private val onReplaceAll: (count: Int) -> Unit
) {

    private val dialog  = Dialog(context, android.R.style.Theme_Black_NoTitleBar)
    private var matches = listOf<Long>()   // list of char offsets where query starts
    private var matchIdx = 0

    private lateinit var etFind:      EditText
    private lateinit var etReplace:   EditText
    private lateinit var tvCount:     TextView
    private lateinit var btnPrev:     ImageButton
    private lateinit var btnNext:     ImageButton
    private lateinit var btnReplace:  Button
    private lateinit var btnReplAll:  Button

    init {
        val root = buildLayout(context)
        dialog.setContentView(root)
        dialog.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            setGravity(Gravity.TOP)
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)
            decorView.setPadding(0, 0, 0, 0)
        }
    }

    fun show() {
        dialog.show()
        etFind.requestFocus()
    }

    fun dismiss() = dialog.dismiss()

    // ---- Layout -------------------------------------------------------------
    private fun buildLayout(ctx: Context): View {
        val root = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.rgb(20, 20, 32))
            setPadding(dp(ctx, 12), dp(ctx, 10), dp(ctx, 12), dp(ctx, 10))
        }

        // Row 1: Find field + nav buttons
        val row1 = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity     = Gravity.CENTER_VERTICAL
        }

        etFind = EditText(ctx).apply {
            hint       = "Find..."
            setHintTextColor(Color.rgb(100, 100, 120))
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(30, 30, 48))
            typeface   = Typeface.MONOSPACE
            textSize   = 14f
            setPadding(dp(ctx, 10), dp(ctx, 8), dp(ctx, 10), dp(ctx, 8))
            layoutParams = LinearLayout.LayoutParams(0, dp(ctx, 40), 1f)
            addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(s: Editable?) = search()
                override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            })
        }
        row1.addView(etFind)

        tvCount = TextView(ctx).apply {
            text      = "0/0"
            setTextColor(Color.rgb(0, 200, 160))
            textSize  = 12f
            setPadding(dp(ctx, 8), 0, dp(ctx, 8), 0)
            minWidth  = dp(ctx, 46)
            gravity   = Gravity.CENTER
        }
        row1.addView(tvCount)

        btnPrev = ImageButton(ctx).apply {
            setImageResource(android.R.drawable.ic_media_previous)
            setBackgroundColor(Color.TRANSPARENT)
            setColorFilter(Color.rgb(0, 200, 160))
            setOnClickListener { navigatePrev() }
        }
        row1.addView(btnPrev)

        btnNext = ImageButton(ctx).apply {
            setImageResource(android.R.drawable.ic_media_next)
            setBackgroundColor(Color.TRANSPARENT)
            setColorFilter(Color.rgb(0, 200, 160))
            setOnClickListener { navigateNext() }
        }
        row1.addView(btnNext)

        val btnClose = ImageButton(ctx).apply {
            setImageResource(android.R.drawable.ic_menu_close_clear_cancel)
            setBackgroundColor(Color.TRANSPARENT)
            setColorFilter(Color.rgb(200, 80, 80))
            setOnClickListener { dismiss() }
        }
        row1.addView(btnClose)

        root.addView(row1)

        // Divider
        root.addView(View(ctx).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 1)
            setBackgroundColor(Color.rgb(40, 40, 60))
        })

        // Row 2: Replace field + buttons
        val row2 = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity     = Gravity.CENTER_VERTICAL
            setPadding(0, dp(ctx, 6), 0, 0)
        }

        etReplace = EditText(ctx).apply {
            hint       = "Replace with..."
            setHintTextColor(Color.rgb(100, 100, 120))
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(30, 30, 48))
            typeface   = Typeface.MONOSPACE
            textSize   = 14f
            setPadding(dp(ctx, 10), dp(ctx, 8), dp(ctx, 10), dp(ctx, 8))
            layoutParams = LinearLayout.LayoutParams(0, dp(ctx, 40), 1f)
        }
        row2.addView(etReplace)

        btnReplace = Button(ctx).apply {
            text     = "Replace"
            textSize = 12f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(40, 80, 160))
            layoutParams = LinearLayout.LayoutParams(dp(ctx, 90), dp(ctx, 38)).apply {
                marginStart = dp(ctx, 6)
            }
            setOnClickListener { replaceCurrent() }
        }
        row2.addView(btnReplace)

        btnReplAll = Button(ctx).apply {
            text     = "All"
            textSize = 12f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(80, 40, 140))
            layoutParams = LinearLayout.LayoutParams(dp(ctx, 56), dp(ctx, 38)).apply {
                marginStart = dp(ctx, 4)
            }
            setOnClickListener { replaceAll() }
        }
        row2.addView(btnReplAll)

        root.addView(row2)
        return root
    }

    // ---- Search logic -------------------------------------------------------
    private fun search() {
        val query = etFind.text.toString()
        matches   = emptyList()
        matchIdx  = 0

        if (query.isEmpty()) { tvCount.text = "0/0"; return }

        val fullText = engine.text
        val found    = mutableListOf<Long>()
        var idx      = fullText.indexOf(query)
        while (idx >= 0) {
            found += idx.toLong()
            idx    = fullText.indexOf(query, idx + 1)
        }
        matches   = found
        tvCount.text = if (matches.isEmpty()) "0/0" else "${matchIdx + 1}/${matches.size}"
        if (matches.isNotEmpty()) jumpTo(0)
    }

    private fun navigateNext() {
        if (matches.isEmpty()) return
        matchIdx = (matchIdx + 1) % matches.size
        jumpTo(matchIdx)
    }

    private fun navigatePrev() {
        if (matches.isEmpty()) return
        matchIdx = (matchIdx - 1 + matches.size) % matches.size
        jumpTo(matchIdx)
    }

    private fun jumpTo(idx: Int) {
        matchIdx = idx
        val pos = matches[idx]
        val pt  = engine.offsetToPoint(pos)
        tvCount.text = "${matchIdx + 1}/${matches.size}"
        onNavigate(pos, pt[0])
    }

    private fun replaceCurrent() {
        if (matches.isEmpty()) return
        val query = etFind.text.toString()
        val repl  = etReplace.text.toString()
        val pos   = matches[matchIdx]
        engine.erase(pos, query.length.toLong())
        engine.insert(pos, repl)
        search()  // re-scan after mutation
    }

    private fun replaceAll() {
        val query = etFind.text.toString()
        val repl  = etReplace.text.toString()
        if (query.isEmpty()) return
        var count  = 0
        var offset = 0L
        val full   = engine.text
        var idx    = full.indexOf(query)
        while (idx >= 0) {
            val adjustedPos = idx.toLong() + offset
            engine.erase(adjustedPos, query.length.toLong())
            engine.insert(adjustedPos, repl)
            offset += (repl.length - query.length).toLong()
            count++
            idx = full.indexOf(query, idx + 1)
        }
        onReplaceAll(count)
        search()
    }

    private fun dp(ctx: Context, v: Int) =
        (v * ctx.resources.displayMetrics.density).toInt()
}
