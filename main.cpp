//  SIN Editor -- main.cpp
//  Supports: Desktop (Linux/Windows/macOS) + Android (via android_main)
//  Raylib 5.0 | C++20
//
//  FIXES vs previous version:
//    [1] DrawRectangleRoundedLinesEx  ->  DrawRectangleRoundedLines
//        (DrawRectangleRoundedLinesEx does not exist in Raylib 5.0)
//    [2] COLORS ambiguity resolved via explicit  using sin::COLORS
//        (avoids ADL lookup failure even with  using namespace sin)
//    [3] save_file renamed to save_doc and declared before all callers
//        (eliminates "no known conversion from Document* to int*" error)

#include "editor.h"
#include "highlighter.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ---- Android-specific includes ----------------------------------------------
#if defined(PLATFORM_ANDROID)
  #include <android_native_app_glue.h>
  #include <android/asset_manager.h>
  #include <android/log.h>
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "SINEditor", __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "SINEditor", __VA_ARGS__)
#else
  #define LOGI(...) (void)0
  #define LOGE(...) (void)0
#endif

// ---- Namespace imports ------------------------------------------------------
namespace fs = std::filesystem;

using namespace sin;       // Document, TabManager, PieceTable, Point, Highlighter ...
using sin::COLORS;         // FIX [2]: explicit import resolves COLORS in all scopes

// ---- Layout -----------------------------------------------------------------
struct Layout {
    int   win_w       = 1280;
    int   win_h       = 800;
    int   header_h    = 48;
    int   tab_bar_h   = 38;
    int   gutter_w    = 60;
    int   font_sz     = 16;
    int   line_h      = 22;
    int   status_h    = 24;
    float tab_w       = 150.0f;
    int   menu_w      = 210;
    int   menu_item_h = 40;

    int editor_y()      const { return header_h + tab_bar_h; }
    int editor_h()      const { return win_h - editor_y() - status_h; }
    int editor_x()      const { return gutter_w; }
    int editor_w()      const { return win_w - gutter_w; }
    int visible_lines() const { return (editor_h() > 0) ? editor_h() / line_h : 1; }
};

// ---- Application state ------------------------------------------------------
struct AppState {
    TabManager  tabs;
    Highlighter hl;
    Layout      lay;

    bool    menu_open     = false;
    float   tab_scroll    = 0.0f;
    float   run_flash_t   = 0.0f;
    bool    run_flashing  = false;
    float   caret_blink   = 0.0f;

    char    status[256]   = "SIN Editor  --  Ready";

    // Touch state (Android)
    bool    touch_down    = false;
    Vector2 touch_pos     = {0.0f, 0.0f};
    double  touch_down_t  = 0.0;
    bool    sim_click     = false;
};

static AppState* g_app = nullptr;

// ---- Helpers ----------------------------------------------------------------
static Color blend_color(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return Color{
        (unsigned char)(a.r + (int)((b.r - a.r) * t)),
        (unsigned char)(a.g + (int)((b.g - a.g) * t)),
        (unsigned char)(a.b + (int)((b.b - a.b) * t)),
        (unsigned char)(a.a + (int)((b.a - a.a) * t)),
    };
}

static Vector2 input_pos() {
#if defined(PLATFORM_ANDROID)
    if (GetTouchPointCount() > 0) return GetTouchPosition(0);
    return {-1.0f, -1.0f};
#else
    return GetMousePosition();
#endif
}

static bool input_clicked(Rectangle r) {
    if (!CheckCollisionPointRec(input_pos(), r)) return false;
#if defined(PLATFORM_ANDROID)
    return g_app->sim_click;
#else
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
#endif
}

// ---- Touch-to-click emulation -----------------------------------------------
static void update_touch() {
#if defined(PLATFORM_ANDROID)
    AppState& app  = *g_app;
    app.sim_click  = false;
    int tc         = GetTouchPointCount();

    if (tc > 0) {
        if (!app.touch_down) {
            app.touch_down   = true;
            app.touch_pos    = GetTouchPosition(0);
            app.touch_down_t = GetTime();
        }
    } else if (app.touch_down) {
        double  dt   = GetTime() - app.touch_down_t;
        float   dist = Vector2Distance(app.touch_pos, GetTouchPosition(0));
        if (dt < 0.3 && dist < 20.0f) app.sim_click = true;
        app.touch_down = false;
    }
#endif
}

// ---- File I/O  (FIX [3]: declared BEFORE any caller) -----------------------
static bool save_doc(Document* doc) {
    if (!doc) return false;
    if (doc->path.empty())
        doc->path = fs::current_path() / (doc->title + ".sino");

    std::ofstream f(doc->path, std::ios::binary | std::ios::trunc);
    if (!f) {
        LOGE("save_doc: cannot open %s", doc->path.string().c_str());
        return false;
    }
    auto txt = doc->buffer.text();
    f.write(txt.data(), static_cast<std::streamsize>(txt.size()));
    doc->buffer.mark_clean();
    doc->is_new = false;
    snprintf(g_app->status, sizeof(g_app->status),
             "Saved: %s", doc->path.string().c_str());
    return true;
}

static bool load_doc(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();

    auto& tabs = g_app->tabs;
    int   idx  = tabs.add(p.filename().string());
    tabs.docs[idx]->path   = p;
    tabs.docs[idx]->buffer = PieceTable(ss.str());
    tabs.docs[idx]->is_new = false;
    snprintf(g_app->status, sizeof(g_app->status), "Loaded: %s", p.string().c_str());
    return true;
}

static void new_doc() {
    static int ctr = 1;
    g_app->tabs.add("untitled-" + std::to_string(ctr++));
}

static void run_current() {
    AppState& app = *g_app;
    Document* doc = app.tabs.current();
    if (!doc) return;
    save_doc(doc);
    if (doc->path.empty()) return;

#if !defined(PLATFORM_ANDROID)
    std::string cmd = "sino \"" + doc->path.string() + "\" 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        snprintf(app.status, sizeof(app.status), "Error: SINO not found in PATH");
        return;
    }
    char buf[512]; std::string out;
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    int ec = pclose(pipe);
    snprintf(app.status, sizeof(app.status), "Exit %d | %s",
             ec, out.empty() ? "(no output)" : out.substr(0, 100).c_str());
#else
    snprintf(app.status, sizeof(app.status), "Run: not supported in mobile build");
#endif

    app.run_flashing = true;
    app.run_flash_t  = 0.0f;
}

// ---- Keyboard ---------------------------------------------------------------
static void process_keyboard() {
    AppState& app = *g_app;
    Document* doc = app.tabs.current();
    if (!doc) return;

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (ctrl && IsKeyPressed(KEY_S)) { save_doc(doc); return; }
    if (ctrl && IsKeyPressed(KEY_Z)) { doc->buffer.undo(); doc->invalidate_cache_from(0); return; }
    if (ctrl && IsKeyPressed(KEY_Y)) { doc->buffer.redo(); doc->invalidate_cache_from(0); return; }
    if (ctrl && IsKeyPressed(KEY_N)) { new_doc(); return; }
    if (ctrl && IsKeyPressed(KEY_W)) { app.tabs.close(app.tabs.active); return; }

    auto get_pt = [&]() { return doc->buffer.offset_to_point(doc->caret_pos); };
    auto set_pt = [&](Point p) { doc->caret_pos = doc->buffer.point_to_offset(p); };

    if (IsKeyPressed(KEY_LEFT)  && doc->caret_pos > 0)                           --doc->caret_pos;
    if (IsKeyPressed(KEY_RIGHT) && doc->caret_pos < doc->buffer.char_count())     ++doc->caret_pos;
    if (IsKeyPressed(KEY_UP))   { auto p = get_pt(); if (p.line > 0) { p.line--; set_pt(p); } }
    if (IsKeyPressed(KEY_DOWN)) {
        auto p = get_pt(); p.line++;
        doc->caret_pos = std::min(doc->buffer.point_to_offset(p), doc->buffer.char_count());
    }
    if (IsKeyPressed(KEY_HOME)) { auto p = get_pt(); p.col = 0; set_pt(p); }
    if (IsKeyPressed(KEY_END))  {
        auto p = get_pt(); p.col = doc->buffer.line(p.line).size(); set_pt(p);
    }
    if (IsKeyPressed(KEY_PAGE_UP))   doc->scroll_y = std::max(0, doc->scroll_y - app.lay.visible_lines());
    if (IsKeyPressed(KEY_PAGE_DOWN)) doc->scroll_y += app.lay.visible_lines();

    if (IsKeyPressed(KEY_BACKSPACE) && doc->caret_pos > 0) {
        --doc->caret_pos;
        doc->buffer.erase(doc->caret_pos, 1);
        doc->invalidate_cache_from(doc->buffer.offset_to_point(doc->caret_pos).line);
    }
    if (IsKeyPressed(KEY_DELETE) && doc->caret_pos < doc->buffer.char_count()) {
        doc->buffer.erase(doc->caret_pos, 1);
        doc->invalidate_cache_from(doc->buffer.offset_to_point(doc->caret_pos).line);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        doc->buffer.insert(doc->caret_pos, "\n");
        ++doc->caret_pos;
        auto p = doc->buffer.offset_to_point(doc->caret_pos);
        doc->invalidate_cache_from(p.line > 0 ? p.line - 1 : 0);
    }
    if (IsKeyPressed(KEY_TAB)) {
        doc->buffer.insert(doc->caret_pos, "    ");
        doc->caret_pos += 4;
        doc->invalidate_cache_from(doc->buffer.offset_to_point(doc->caret_pos).line);
    }

    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (ch >= 32) {
            char buf[2] = {static_cast<char>(ch), '\0'};
            doc->buffer.insert(doc->caret_pos, std::string_view(buf, 1));
            ++doc->caret_pos;
            doc->invalidate_cache_from(doc->buffer.offset_to_point(doc->caret_pos).line);
        }
    }

    // Scroll to keep caret visible
    auto pt = doc->buffer.offset_to_point(doc->caret_pos);
    int  vl = app.lay.visible_lines();
    if ((int)pt.line < doc->scroll_y)       doc->scroll_y = (int)pt.line;
    if ((int)pt.line >= doc->scroll_y + vl) doc->scroll_y = (int)pt.line - vl + 1;
}

// ---- Draw: Header -----------------------------------------------------------
static void draw_header() {
    AppState& app = *g_app;
    Layout&   lay = app.lay;
    Vector2   mp  = input_pos();

    DrawRectangle(0, 0, lay.win_w, lay.header_h, COLORS.header_bg);
    DrawRectangle(0, lay.header_h - 1, lay.win_w, 1, {0, 200, 160, 50});

    // Hamburger
    Rectangle ham = {10.0f, 8.0f, 36.0f, 32.0f};
    bool hov_ham  = CheckCollisionPointRec(mp, ham);
    if (hov_ham) DrawRectangleRounded(ham, 0.3f, 4, {255, 255, 255, 18});
    for (int i = 0; i < 3; ++i)
        DrawRectangleRec({20.0f, 15.0f + (float)i * 7.0f, 20.0f, 2.0f},
                         hov_ham ? COLORS.accent : Color{170, 170, 170, 255});
    if (input_clicked(ham)) app.menu_open = !app.menu_open;

    // Title
    const char* title = "SIN EDITOR";
    DrawText(title, (lay.win_w - MeasureText(title, 15)) / 2, 16, 15, COLORS.accent);

    // Run button
    Rectangle run  = {(float)(lay.win_w - 95), 11.0f, 80.0f, 26.0f};
    bool hov_run   = CheckCollisionPointRec(mp, run);

    if (app.run_flashing) {
        app.run_flash_t += GetFrameTime() * 3.0f;
        if (app.run_flash_t >= 1.0f) { app.run_flashing = false; app.run_flash_t = 0.0f; }
    }
    Color rc = {0, 210, 80, 255};
    if (app.run_flashing) rc = blend_color(rc, {255, 240, 60, 255}, app.run_flash_t);
    if (hov_run)          rc = blend_color(rc, WHITE, 0.18f);

    DrawRectangleRounded(run, 0.35f, 6, rc);

    // FIX [1]: DrawRectangleRoundedLines  (not DrawRectangleRoundedLinesEx)
    // Raylib 5.0 signature: (Rectangle, roundness, segments, lineThick, Color)
    DrawRectangleRoundedLines(
        {run.x - 1.0f, run.y - 1.0f, run.width + 2.0f, run.height + 2.0f},
        0.35f, 6, 1.5f, Color{0, 255, 100, 35});

    int rw = MeasureText("RUN", 13);
    DrawText("RUN", (int)(run.x + (run.width - (float)rw) / 2.0f), (int)(run.y + 6), 13, BLACK);

    if (input_clicked(run)) run_current();
}

// ---- Draw: Hamburger Menu ---------------------------------------------------
struct MenuItem { const char* label; };
static const MenuItem MENU_ITEMS[] = {
    {"New File"},
    {"Save"},
    {"Close Tab"},
    {"Rename"},
    {"Delete Tab"},
};
static constexpr int MENU_COUNT = 5;

static void draw_menu() {
    AppState& app = *g_app;
    if (!app.menu_open) return;

    Layout& lay = app.lay;
    DrawRectangle(0, 0, lay.win_w, lay.win_h, {0, 0, 0, 90});

    float     mh = (float)(MENU_COUNT * lay.menu_item_h + 14);
    Rectangle bg = {8.0f, (float)(lay.header_h + 4), (float)lay.menu_w, mh};

    DrawRectangleRounded(bg, 0.07f, 6, {20, 20, 32, 245});
    // FIX [1]: DrawRectangleRoundedLines (not Ex)
    DrawRectangleRoundedLines(bg, 0.07f, 6, 1.0f, Color{0, 200, 160, 55});

    Vector2 mp = input_pos();
    for (int i = 0; i < MENU_COUNT; ++i) {
        Rectangle item = {
            bg.x + 5.0f,
            bg.y + 7.0f + (float)(i * lay.menu_item_h),
            bg.width - 10.0f,
            (float)lay.menu_item_h
        };
        bool hov = CheckCollisionPointRec(mp, item);
        if (hov) DrawRectangleRounded(item, 0.2f, 4, {255, 255, 255, 16});

        // FIX [2]: COLORS.accent now resolved correctly via  using sin::COLORS
        DrawText(MENU_ITEMS[i].label,
                 (int)(item.x + 16), (int)(item.y + 11), 14,
                 hov ? COLORS.accent : Color{208, 208, 208, 255});

        if (input_clicked(item)) {
            app.menu_open = false;
            switch (i) {
                case 0: new_doc(); break;
                // FIX [3]: save_doc(Document*) -- correct type, no int* conflict
                case 1: save_doc(app.tabs.current()); break;
                case 2: app.tabs.close(app.tabs.active); break;
                case 3: { if (auto* d = app.tabs.current()) d->title += "_r"; break; }
                case 4: app.tabs.close(app.tabs.active); break;
            }
        }
    }

    // Close on outside click
    if (input_clicked({0.0f, 0.0f, (float)lay.win_w, (float)lay.win_h}) &&
        !CheckCollisionPointRec(mp, bg))
        app.menu_open = false;
}

// ---- Draw: Tab Bar ----------------------------------------------------------
static void draw_tabs() {
    AppState& app = *g_app;
    Layout&   lay = app.lay;
    int       y   = lay.header_h;

    DrawRectangle(0, y, lay.win_w, lay.tab_bar_h, COLORS.tab_bg);
    DrawRectangle(0, y + lay.tab_bar_h - 1, lay.win_w, 1, {28, 28, 46, 255});

    if (app.tabs.docs.empty()) return;

    Vector2 mp = input_pos();

    // Horizontal scroll on tab bar via mouse wheel
    if (CheckCollisionPointRec(mp, {0.0f, (float)y, (float)lay.win_w, (float)lay.tab_bar_h})) {
        float w = GetMouseWheelMove();
        if (w != 0.0f) app.tab_scroll = std::max(0.0f, app.tab_scroll - w * lay.tab_w);
    }
    float total_w = (float)app.tabs.docs.size() * lay.tab_w;
    app.tab_scroll = Clamp(app.tab_scroll, 0.0f, std::max(0.0f, total_w - (float)lay.win_w));

    BeginScissorMode(0, y, lay.win_w, lay.tab_bar_h);

    float x = 6.0f - app.tab_scroll;
    for (int i = 0; i < (int)app.tabs.docs.size(); ++i) {
        auto& doc    = app.tabs.docs[i];
        bool  active = (i == app.tabs.active);
        bool  dirty  = doc->buffer.is_dirty();

        Rectangle tr = {x, (float)(y + 4), lay.tab_w - 4.0f, (float)(lay.tab_bar_h - 8)};
        DrawRectangleRounded(tr, 0.22f, 4, active ? COLORS.tab_active : COLORS.tab_bg);
        if (active)
            DrawRectangle((int)(tr.x + 4), y + lay.tab_bar_h - 3,
                          (int)(tr.width - 8), 2, COLORS.accent);

        std::string label = (dirty ? "* " : "") + doc->title;
        int tw2 = MeasureText(label.c_str(), 13);
        while (tw2 > (int)(tr.width - 32) && label.size() > 2) {
            label.pop_back();
            tw2 = MeasureText(label.c_str(), 13);
        }
        DrawText(label.c_str(), (int)(tr.x + 8), (int)(tr.y + 9), 13,
                 active ? WHITE : Color{155, 155, 155, 255});

        // Close button
        Rectangle cr = {tr.x + tr.width - 20.0f, tr.y + 8.0f, 14.0f, 14.0f};
        bool chov = CheckCollisionPointRec(mp, cr);
        if (chov) DrawRectangleRounded(cr, 0.4f, 4, {220, 50, 50, 180});
        DrawText("x", (int)(cr.x + 3), (int)(cr.y + 1), 12,
                 chov ? WHITE : Color{140, 140, 140, 200});

        if      (input_clicked(cr)) { app.tabs.close(i); break; }
        else if (input_clicked(tr))   app.tabs.active = i;

        x += lay.tab_w;
    }

    EndScissorMode();
}

// ---- Draw: Editor -----------------------------------------------------------
static void draw_editor() {
    AppState& app = *g_app;
    Layout&   lay = app.lay;

    int ey = lay.editor_y();
    int eh = lay.editor_h();
    int ex = lay.editor_x();

    DrawRectangle(0, ey, lay.win_w, eh, COLORS.editor_bg);
    DrawRectangle(0, ey, lay.gutter_w, eh, COLORS.gutter_bg);
    DrawRectangle(lay.gutter_w - 1, ey, 1, eh, {28, 28, 44, 255});

    Document* doc = app.tabs.current();
    if (!doc) {
        const char* msg = "No file open -- use the menu to create one";
        DrawText(msg, (lay.win_w - MeasureText(msg, 15)) / 2,
                 ey + eh / 2, 15, {75, 75, 100, 255});
        return;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) doc->scroll_y = std::max(0, doc->scroll_y - (int)(wheel * 3));

    int line_count = (int)doc->buffer.line_count();
    int vis        = lay.visible_lines();
    doc->scroll_y  = Clamp(doc->scroll_y, 0, std::max(0, line_count - vis));

    doc->line_cache.resize(line_count);

    Point caret_pt  = doc->buffer.offset_to_point(doc->caret_pos);
    int   active_ln = (int)caret_pt.line;

    BeginScissorMode(0, ey, lay.win_w, eh);

    int last_line = std::min(doc->scroll_y + vis + 1, line_count);
    for (int li = doc->scroll_y; li < last_line; ++li) {
        int sy = ey + (li - doc->scroll_y) * lay.line_h;

        if (li == active_ln) {
            DrawRectangle(0, sy, lay.win_w, lay.line_h, COLORS.active_bg);
            DrawRectangle(ex, sy + lay.line_h - 1, lay.editor_w(), 1, {28, 110, 175, 35});
        }

        char lnum[12];
        snprintf(lnum, sizeof(lnum), "%4d", li + 1);
        DrawText(lnum, 6, sy + 3, lay.font_sz - 2,
                 (li == active_ln) ? COLORS.active_ln : Color{65, 78, 100, 255});

        auto& lc = doc->line_cache[li];
        if (lc.dirty) {
            lc.text   = doc->buffer.line(li);
            lc.tokens = app.hl.tokenize(lc.text);
            lc.dirty  = false;
        }

        const int glyph_w = lay.font_sz / 2 + 2;
        int       base_x  = ex + 6 - doc->scroll_x;

        if (lc.tokens.empty()) {
            DrawText(lc.text.c_str(), base_x, sy + 3, lay.font_sz, COLORS.normal);
        } else {
            for (const auto& tok : lc.tokens) {
                std::string word(lc.text.data() + tok.start, tok.length);
                DrawText(word.c_str(),
                         base_x + (int)(tok.start) * glyph_w,
                         sy + 3, lay.font_sz, app.hl.color_for(tok.type));
            }
        }
    }

    // Caret blink
    app.caret_blink += GetFrameTime() * 2.0f;
    if ((int)app.caret_blink % 2 == 0) {
        int glyph_w = lay.font_sz / 2 + 2;
        int cx = ex + 6 + (int)(caret_pt.col) * glyph_w - doc->scroll_x;
        int cy = ey + (active_ln - doc->scroll_y) * lay.line_h;
        DrawRectangle(cx, cy + 2, 2, lay.line_h - 4, COLORS.accent);
    }

    EndScissorMode();

    // Status bar
    DrawRectangle(0, lay.win_h - lay.status_h, lay.win_w, lay.status_h, {10, 10, 16, 255});
    DrawText(app.status, 10, lay.win_h - lay.status_h + 5, 12, {110, 175, 110, 200});

    char pos_str[40];
    snprintf(pos_str, sizeof(pos_str), "Ln %zu  Col %zu",
             caret_pt.line + 1, caret_pt.col + 1);
    int pw = MeasureText(pos_str, 12);
    DrawText(pos_str, lay.win_w - pw - 12, lay.win_h - lay.status_h + 5, 12, {100, 140, 180, 200});
}

// ---- Frame tick -------------------------------------------------------------
static void app_tick() {
    AppState& app = *g_app;

    app.lay.win_w = GetScreenWidth();
    app.lay.win_h = GetScreenHeight();

#if defined(PLATFORM_ANDROID)
    app.lay.header_h    = 60;
    app.lay.tab_bar_h   = 44;
    app.lay.menu_item_h = 52;
    app.lay.gutter_w    = 50;
    app.lay.font_sz     = 18;
    app.lay.line_h      = 26;
    app.lay.tab_w       = 170.0f;
    app.lay.menu_w      = 230;
    app.lay.status_h    = 28;
#endif

    update_touch();
    process_keyboard();

    BeginDrawing();
    ClearBackground(COLORS.editor_bg);

    draw_editor();
    draw_tabs();
    draw_header();
    draw_menu();

    EndDrawing();
}

// ---- Bootstrap / Shutdown ---------------------------------------------------
static void app_init(const char* initial_file) {
    g_app = new AppState();
    if (initial_file && initial_file[0] != '\0') {
        load_doc(initial_file);
    } else {
        new_doc();
        if (auto* d = g_app->tabs.current()) {
            const char* welcome =
                "// SIN Editor -- SINO Language IDE\n"
                "// Version 0.2\n\n"
                "fn greet(name: str) -> str {\n"
                "    return \"Hello, \" + name + \"!\"\n"
                "}\n\n"
                "fn main() {\n"
                "    let msg = greet(\"SINO\")\n"
                "    println(msg)\n"
                "}\n";
            d->buffer.insert(0, welcome);
            d->buffer.mark_clean();
        }
    }
    LOGI("SIN Editor ready. Screen: %d x %d", GetScreenWidth(), GetScreenHeight());
}

static void app_shutdown() {
    delete g_app;
    g_app = nullptr;
}

// =============================================================================
//  Entry: Desktop
// =============================================================================
#if !defined(PLATFORM_ANDROID)

int main(int argc, char* argv[]) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "SIN Editor -- SINO IDE");
    SetTargetFPS(60);

    app_init(argc > 1 ? argv[1] : nullptr);

    while (!WindowShouldClose()) {
        app_tick();
    }

    app_shutdown();
    CloseWindow();
    return 0;
}

// =============================================================================
//  Entry: Android
// =============================================================================
#else

void android_main(struct android_app* state) {
    (void)state;
    InitWindow(0, 0, "SIN Editor");
    SetTargetFPS(60);
    app_init(nullptr);
    while (!WindowShouldClose()) {
        app_tick();
    }
    app_shutdown();
    CloseWindow();
}

#endif // PLATFORM_ANDROID
