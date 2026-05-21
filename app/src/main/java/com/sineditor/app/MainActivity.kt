package com.sineditor.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.*
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter

class MainActivity : AppCompatActivity() {

    // ---- Views --------------------------------------------------------------
    private lateinit var editorView : EditorView
    private lateinit var tabLayout  : LinearLayout
    private lateinit var tabScroll  : HorizontalScrollView
    private lateinit var statusBar  : TextView
    private lateinit var headerRow  : LinearLayout
    private lateinit var toolbarRow : LinearLayout  // Touch toolbar for actions

    // ---- Documents ----------------------------------------------------------
    private data class Doc(
        var title  : String,
        var uri    : Uri?,
        val engine : EditorEngine
    )

    private val docs     = mutableListOf<Doc>()
    private var activeIdx = -1

    // ---- Find/Replace -------------------------------------------------------
    private var findDialog: FindReplaceDialog? = null

    // ---- Request codes ------------------------------------------------------
    companion object {
        private const val REQ_OPEN = 1001
        private const val REQ_SAVE = 1002
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Edge-to-edge dark chrome
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.statusBarColor     = Color.rgb(12, 12, 18)
        window.navigationBarColor = Color.rgb( 9,  9, 15)

        setContentView(buildRootLayout())

        newDoc()
        setStatus("SIN Editor  —  Ready")
    }

    override fun onDestroy() {
        super.onDestroy()
        docs.forEach { it.engine.destroy() }
    }

    // =========================================================================
    // Layout (100 % programmatic -- no XML inflation overhead)
    // =========================================================================
    private fun buildRootLayout(): View {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.rgb(14, 14, 20))
        }

        root.addView(buildHeader())
        root.addView(buildToolbar())  // Touch action toolbar
        root.addView(buildTabBar())

        editorView = EditorView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
            isFocusable = true
            isFocusableInTouchMode = true
            onSave { saveDoc() }
            onNewDoc { newDoc() }
            onDirtyChanged = { refreshTabs() }
            onMemoryWarning = { showMemoryWarning() }
        }
        root.addView(editorView)
        root.addView(buildStatusBar())

        return root
    }

    private fun buildHeader(): View {
        headerRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(54))
            setBackgroundColor(Color.rgb(12, 12, 18))
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(6), 0, dp(6), 0)
        }

        // Hamburger menu
        headerRow.addView(iconBtn("☰", Color.rgb(0, 200, 160)) { showMenu() })

        // Title (flexible space)
        headerRow.addView(TextView(this).apply {
            text      = "SIN EDITOR"
            textSize  = 13f
            typeface  = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
            setTextColor(Color.rgb(0, 200, 160))
            gravity   = Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
        })

        // Find button
        headerRow.addView(iconBtn("🔍", Color.rgb(160, 160, 200)) {
            toggleFind()
        })

        // Font size buttons
        headerRow.addView(iconBtn("A⁻", Color.rgb(160, 160, 160)) { changeFontSize(-1) })
        headerRow.addView(iconBtn("A⁺", Color.rgb(160, 160, 160)) { changeFontSize(+1) })

        return headerRow
    }

    /** Touch Toolbar - replaces keyboard shortcuts with visible buttons */
    private fun buildToolbar(): View {
        toolbarRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(42))
            setBackgroundColor(Color.rgb(18, 18, 28))
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(4), 0, dp(4), 0)
        }

        // New document button
        toolbarRow.addView(touchBtn("📄 New", Color.rgb(0, 200, 160)) {
            newDoc()
        })

        // Save button
        toolbarRow.addView(touchBtn("💾 Save", Color.rgb(0, 200, 160)) {
            saveDoc()
        })

        // Undo button
        toolbarRow.addView(touchBtn("↶ Undo", Color.rgb(160, 160, 160)) {
            editorView.actionUndo()
        })

        // Redo button
        toolbarRow.addView(touchBtn("↷ Redo", Color.rgb(160, 160, 160)) {
            editorView.actionRedo()
        })

        // Run button
        toolbarRow.addView(touchBtn("▶ Run", Color.rgb(0, 210, 80)) {
            runScript()
        })

        return toolbarRow
    }

    private fun buildTabBar(): View {
        tabScroll = HorizontalScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(38))
            setBackgroundColor(Color.rgb(18, 18, 28))
            isHorizontalScrollBarEnabled = false
        }
        tabLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.MATCH_PARENT)
        }
        tabScroll.addView(tabLayout)
        return tabScroll
    }

    private fun buildStatusBar(): View {
        statusBar = TextView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(22))
            setTextColor(Color.rgb(105, 172, 105))
            setBackgroundColor(Color.rgb(9, 9, 15))
            textSize = 11f
            setPadding(dp(8), dp(3), dp(8), 0)
            typeface = Typeface.MONOSPACE
            maxLines = 1
            ellipsize = android.text.TextUtils.TruncateAt.END
        }
        return statusBar
    }

    // Helper: small square icon button
    private fun iconBtn(label: String, tint: Int, action: () -> Unit): View =
        TextView(this).apply {
            text     = label
            textSize = 16f
            setTextColor(tint)
            gravity  = Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(dp(38), dp(38))
            setOnClickListener { action() }
        }

    // Helper: touch toolbar button with label
    private fun touchBtn(label: String, tint: Int, action: () -> Unit): View =
        TextView(this).apply {
            text     = label
            textSize = 11f
            setTextColor(tint)
            gravity  = Gravity.CENTER
            setPadding(dp(8), 0, dp(8), 0)
            layoutParams = LinearLayout.LayoutParams(0, dp(36), 1f).apply {
                marginStart = dp(2)
                marginEnd = dp(2)
            }
            setOnClickListener { action() }
        }

    // =========================================================================
    // Hamburger Menu
    // =========================================================================
    private fun showMenu() {
        val items = arrayOf(
            "📄  New File",
            "📂  Open File",
            "💾  Save",
            "💾  Save As…",
            "✖  Close Tab",
            "📊  Memory Info",
            "ℹ  About"
        )
        AlertDialog.Builder(this)
            .setTitle(null)
            .setItems(items) { _, i ->
                when (i) {
                    0 -> newDoc()
                    1 -> openPicker()
                    2 -> saveDoc()
                    3 -> saveAsPicker()
                    4 -> closeDoc()
                    5 -> showMemoryInfo()
                    6 -> showAbout()
                }
            }
            .show()
    }

    private fun showAbout() {
        AlertDialog.Builder(this)
            .setTitle("SIN Editor  v0.3 (Android Only)")
            .setMessage(
                "SINO Language IDE

" +
                "Architecture:
" +
                "  • Kotlin  → UI, lifecycle, file I/O
" +
                "  • C++20  → Piece Table engine (via JNI)

" +
                "Engine: O(1) insert / delete
" +
                "Memory: Capped undo/redo (8MB limit)
" +
                "Rendering: Android Canvas
" +
                "Syntax: SINO language

" +
                "Desktop support removed. Android optimized."
            )
            .setPositiveButton("OK", null)
            .show()
    }

    // =========================================================================
    // Memory Info
    // =========================================================================
    private fun showMemoryInfo() {
        val doc = docs.getOrNull(activeIdx) ?: return
        val mem = doc.engine.memoryUsage
        val pressure = doc.engine.isMemoryPressure

        AlertDialog.Builder(this)
            .setTitle("Memory Usage")
            .setMessage(
                "Document: ${doc.title}
" +
                "Memory: ${mem / 1024} KB
" +
                "Status: ${if (pressure) "⚠ HIGH PRESSURE" else "✓ Normal"}

" +
                "Limits:
" +
                "  Max buffer: 8 MB
" +
                "  Max undo: 50 levels
" +
                "  Max redo: 25 levels"
            )
            .setPositiveButton("Compact") { _, _ ->
                doc.engine.compact()
                setStatus("Memory compacted")
            }
            .setNegativeButton("OK", null)
            .show()
    }

    private fun showMemoryWarning() {
        Toast.makeText(this, 
            "⚠ Memory pressure high! Consider saving and compacting.",
            Toast.LENGTH_LONG).show()
    }

    // =========================================================================
    // Find / Replace
    // =========================================================================
    private fun toggleFind() {
        val doc = docs.getOrNull(activeIdx) ?: return
        if (findDialog == null) {
            findDialog = FindReplaceDialog(
                context     = this,
                engine      = doc.engine,
                onNavigate  = { pos, line ->
                    editorView.caretPos = pos
                    editorView.scrollToLine(line)
                },
                onReplaceAll = { count ->
                    setStatus("Replaced $count occurrence(s)")
                    editorView.invalidateAllCache()
                }
            )
        }
        findDialog?.show()
    }

    // =========================================================================
    // Font size
    // =========================================================================
    private fun changeFontSize(delta: Int) {
        editorView.adjustFontSize(delta)
        setStatus("Font size: ${editorView.currentFontSizeSp}sp")
    }

    // =========================================================================
    // Documents
    // =========================================================================
    private fun newDoc() {
        val engine = EditorEngine(
            "// SIN Editor  —  SINO Language

fn main() {
    println("Hello SINO!")
}
"
        )
        engine.markClean()
        docs += Doc("untitled-${docs.size + 1}", null, engine)
        activateDoc(docs.lastIndex)
    }

    private fun activateDoc(idx: Int) {
        activeIdx = idx
        val doc   = docs[idx]
        editorView.setEngine(doc.engine)
        editorView.caretPos = 0
        findDialog?.dismiss()
        findDialog = null     // reset find dialog for new doc
        refreshTabs()
        setStatus("Editing: ${doc.title}")
    }

    private fun closeDoc() {
        if (docs.isEmpty()) return
        docs[activeIdx].engine.destroy()
        docs.removeAt(activeIdx)
        if (docs.isEmpty()) { newDoc(); return }
        activateDoc(activeIdx.coerceAtMost(docs.lastIndex))
    }

    // =========================================================================
    // Tab bar
    // =========================================================================
    private fun refreshTabs() {
        tabLayout.removeAllViews()
        docs.forEachIndexed { i, doc ->
            val label  = (if (doc.engine.isDirty) "* " else "") + doc.title
            val active = (i == activeIdx)

            val tabView = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity     = Gravity.CENTER_VERTICAL
                setBackgroundColor(
                    if (active) Color.rgb(30, 30, 45) else Color.TRANSPARENT
                )
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.MATCH_PARENT)
                setPadding(dp(12), 0, dp(6), 0)
                setOnClickListener { activateDoc(i) }
            }

            // Label
            tabView.addView(TextView(this).apply {
                text      = label
                textSize  = 12f
                typeface  = Typeface.MONOSPACE
                setTextColor(if (active) Color.WHITE else Color.rgb(140, 140, 140))
                maxWidth  = dp(130)
                maxLines  = 1
                ellipsize = android.text.TextUtils.TruncateAt.END
            })

            // Active line indicator
            if (active) {
                val frame = android.widget.FrameLayout(this).apply {
                    layoutParams = LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.MATCH_PARENT)
                    addView(tabView)
                }
                tabLayout.addView(frame)
                continue
            }

            tabLayout.addView(tabView)

            // Close button (×)
            tabView.addView(TextView(this).apply {
                text      = "×"
                textSize  = 14f
                setTextColor(Color.rgb(120, 120, 120))
                setPadding(dp(6), 0, dp(4), 0)
                setOnClickListener { closeDocAt(i) }
            })
        }
    }

    private fun closeDocAt(idx: Int) {
        docs[idx].engine.destroy()
        docs.removeAt(idx)
        if (docs.isEmpty()) { newDoc(); return }
        activateDoc(activeIdx.coerceAtMost(docs.lastIndex))
    }

    // =========================================================================
    // File I/O
    // =========================================================================
    private fun openPicker() {
        startActivityForResult(
            Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            }, REQ_OPEN)
    }

    private fun saveAsPicker() {
        val doc = docs.getOrNull(activeIdx) ?: return
        startActivityForResult(
            Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "text/plain"
                putExtra(Intent.EXTRA_TITLE, "${doc.title}.sino")
            }, REQ_SAVE)
    }

    private fun saveDoc() {
        val doc = docs.getOrNull(activeIdx) ?: return
        if (doc.uri != null) {
            writeUri(doc.uri!!, doc.engine.text)
            doc.engine.markClean()
            refreshTabs()
            setStatus("Saved: ${doc.title}")
        } else {
            saveAsPicker()
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != Activity.RESULT_OK) return
        val uri = data?.data ?: return

        when (requestCode) {
            REQ_OPEN -> {
                val text   = readUri(uri)
                val name   = uriName(uri)
                val engine = EditorEngine(text)
                engine.markClean()
                docs += Doc(name, uri, engine)
                activateDoc(docs.lastIndex)
            }
            REQ_SAVE -> {
                val doc  = docs.getOrNull(activeIdx) ?: return
                writeUri(uri, doc.engine.text)
                doc.engine.markClean()
                val name = uriName(uri)
                docs[activeIdx] = doc.copy(title = name, uri = uri)
                refreshTabs()
                setStatus("Saved: $name")
            }
        }
    }

    private fun readUri(uri: Uri): String {
        val sb = StringBuilder()
        contentResolver.openInputStream(uri)?.use { s ->
            BufferedReader(InputStreamReader(s)).forEachLine { sb.append(it).append('
') }
        }
        return sb.toString()
    }

    private fun writeUri(uri: Uri, text: String) {
        contentResolver.openOutputStream(uri, "wt")?.use { s ->
            OutputStreamWriter(s).use { it.write(text) }
        }
    }

    private fun uriName(uri: Uri): String {
        var result = uri.lastPathSegment ?: "untitled.sino"
        contentResolver.query(uri, null, null, null, null)?.use { c ->
            if (c.moveToFirst()) {
                val idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (idx >= 0) result = c.getString(idx)
            }
        }
        return result
    }

    // =========================================================================
    // Run
    // =========================================================================
    private fun runScript() {
        saveDoc()
        setStatus("Run: transfer the .sino file to a device with the SINO runtime installed.")
        Toast.makeText(this,
            "SINO runtime not bundled. Save the file and run externally.",
            Toast.LENGTH_LONG).show()
    }

    // =========================================================================
    // Status
    // =========================================================================
    fun setStatus(msg: String) {
        statusBar.text = msg
    }

    private fun dp(v: Int) =
        (v * resources.displayMetrics.density).toInt()
}
