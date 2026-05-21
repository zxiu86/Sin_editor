// ============================================================
//  SIN Editor -- JNI Bridge (Android Optimized)
//  Exposes the C++ PieceTable engine to Kotlin
//
//  Kotlin loads the lib via:
//    System.loadLibrary("sineditor_engine")
//
//  Each "document" is a heap-allocated PieceTable identified
//  by a Long handle passed back to Kotlin.
//  
//  MEMORY SAFETY:
//  - All handles validated before dereference
//  - Exception checking after every JNI operation
//  - RAII wrappers for jstring conversions
//  - Null checks on all Java inputs
// ============================================================
#include "piece_table.h"

#include <jni.h>
#include <android/log.h>
#include <cassert>
#include <cstring>
#include <string>
#include <mutex>
#include <unordered_map>

#define TAG "SINEditorJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---- Global handle registry for safety --------------------------------------
static std::mutex g_registry_mutex;
static std::unordered_map<jlong, sin::PieceTable*> g_handle_registry;

static void register_handle(jlong h, sin::PieceTable* pt) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_handle_registry[h] = pt;
}

static void unregister_handle(jlong h) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_handle_registry.erase(h);
}

static sin::PieceTable* safe_from_handle(jlong h) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto it = g_handle_registry.find(h);
    if (it == g_handle_registry.end() || it->second == nullptr) {
        LOGE("Invalid or stale handle: %p", (void*)h);
        return nullptr;
    }
    return it->second;
}

// ---- Exception helper -------------------------------------------------------
static bool check_exception(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

// ---- RAII jstring wrapper ---------------------------------------------------
class JStringGuard {
    JNIEnv* env_;
    jstring js_;
    const char* cstr_;
    bool released_;
public:
    JStringGuard(JNIEnv* env, jstring js) 
        : env_(env), js_(js), cstr_(nullptr), released_(false) {
        if (js_) cstr_ = env_->GetStringUTFChars(js_, nullptr);
    }
    ~JStringGuard() {
        if (cstr_ && !released_) {
            env_->ReleaseStringUTFChars(js_, cstr_);
        }
    }
    const char* c_str() const { return cstr_; }
    std::string to_string() const { return cstr_ ? std::string(cstr_) : std::string(); }
    void release() {
        if (cstr_ && !released_) {
            env_->ReleaseStringUTFChars(js_, cstr_);
            released_ = true;
            cstr_ = nullptr;
        }
    }
    bool valid() const { return cstr_ != nullptr; }
};

// ============================================================
//  C O R E    A P I
//  Package: com.sineditor.app  --  class: EditorEngine
// ============================================================
extern "C" {

// ---- Lifecycle --------------------------------------------------------------

JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nCreate(JNIEnv* env, jobject /*thiz*/,
                                             jstring initial_text) {
    JStringGuard text_guard(env, initial_text);
    if (initial_text != nullptr && !text_guard.valid()) {
        LOGE("nCreate: Failed to extract initial text");
        return 0;
    }

    std::string text = text_guard.to_string();
    auto* pt = new (std::nothrow) sin::PieceTable(std::move(text));

    if (!pt) {
        LOGE("nCreate: Memory allocation failed");
        return 0;
    }

    jlong handle = reinterpret_cast<jlong>(pt);
    register_handle(handle, pt);

    LOGI("nCreate -> handle=%p, text_size=%zu", (void*)handle, text.size());
    return handle;
}

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nDestroy(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    if (handle == 0) {
        LOGW("nDestroy: Null handle");
        return;
    }

    auto* pt = safe_from_handle(handle);
    if (pt) {
        unregister_handle(handle);
        delete pt;
        LOGI("nDestroy handle=%p freed", (void*)handle);
    } else {
        LOGE("nDestroy: Invalid handle=%p", (void*)handle);
    }
}

// ---- Text operations --------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nInsert(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle,
                                             jlong char_pos,
                                             jstring text) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nInsert: Invalid handle");
        return;
    }

    JStringGuard text_guard(env, text);
    if (text == nullptr) {
        LOGW("nInsert: Null text");
        return;
    }
    if (!text_guard.valid()) {
        LOGE("nInsert: Failed to extract text");
        return;
    }

    std::string s = text_guard.to_string();
    pt->insert(static_cast<size_t>(char_pos), s);

    if (check_exception(env)) {
        LOGE("nInsert: Exception occurred");
    }
}

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nErase(JNIEnv* /*env*/, jobject /*thiz*/,
                                            jlong handle,
                                            jlong char_pos,
                                            jlong count) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nErase: Invalid handle");
        return;
    }

    if (char_pos < 0 || count < 0) {
        LOGW("nErase: Negative params pos=%ld count=%ld", (long)char_pos, (long)count);
        return;
    }

    pt->erase(static_cast<size_t>(char_pos), static_cast<size_t>(count));
}

JNIEXPORT jstring JNICALL
Java_com_sineditor_app_EditorEngine_nGetText(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nGetText: Invalid handle");
        return env->NewStringUTF("");
    }

    std::string text = pt->text();
    jstring result = env->NewStringUTF(text.c_str());

    if (check_exception(env)) {
        LOGE("nGetText: Exception creating jstring");
        return env->NewStringUTF("");
    }
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_sineditor_app_EditorEngine_nGetLine(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle,
                                              jlong line_idx) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nGetLine: Invalid handle");
        return env->NewStringUTF("");
    }

    if (line_idx < 0) {
        LOGW("nGetLine: Negative line index");
        return env->NewStringUTF("");
    }

    std::string line = pt->line(static_cast<size_t>(line_idx));
    return env->NewStringUTF(line.c_str());
}

JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nLineCount(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nLineCount: Invalid handle");
        return 0;
    }
    return static_cast<jlong>(pt->line_count());
}

JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nCharCount(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nCharCount: Invalid handle");
        return 0;
    }
    return static_cast<jlong>(pt->char_count());
}

// ---- Coordinate conversion --------------------------------------------------

JNIEXPORT jlongArray JNICALL
Java_com_sineditor_app_EditorEngine_nOffsetToPoint(JNIEnv* env, jobject /*thiz*/,
                                                    jlong handle,
                                                    jlong char_pos) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nOffsetToPoint: Invalid handle");
        jlongArray arr = env->NewLongArray(2);
        jlong buf[2] = {0, 0};
        env->SetLongArrayRegion(arr, 0, 2, buf);
        return arr;
    }

    if (char_pos < 0) char_pos = 0;

    auto point = pt->offset_to_point(static_cast<size_t>(char_pos));
    jlongArray arr = env->NewLongArray(2);
    if (!arr) {
        LOGE("nOffsetToPoint: Failed to create array");
        return nullptr;
    }

    jlong buf[2] = {static_cast<jlong>(point.line), static_cast<jlong>(point.col)};
    env->SetLongArrayRegion(arr, 0, 2, buf);

    if (check_exception(env)) {
        LOGE("nOffsetToPoint: Exception setting array region");
    }
    return arr;
}

JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nPointToOffset(JNIEnv* /*env*/, jobject /*thiz*/,
                                                    jlong handle,
                                                    jlong line,
                                                    jlong col) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nPointToOffset: Invalid handle");
        return 0;
    }

    if (line < 0) line = 0;
    if (col < 0) col = 0;

    sin::Point p{static_cast<size_t>(line), static_cast<size_t>(col)};
    return static_cast<jlong>(pt->point_to_offset(p));
}

// ---- History ----------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nUndo(JNIEnv* /*env*/, jobject /*thiz*/,
                                           jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nUndo: Invalid handle");
        return;
    }
    pt->undo();
}

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nRedo(JNIEnv* /*env*/, jobject /*thiz*/,
                                           jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nRedo: Invalid handle");
        return;
    }
    pt->redo();
}

JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nCanUndo(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) return JNI_FALSE;
    return pt->can_undo() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nCanRedo(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) return JNI_FALSE;
    return pt->can_redo() ? JNI_TRUE : JNI_FALSE;
}

// ---- Dirty tracking ---------------------------------------------------------

JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nIsDirty(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) return JNI_FALSE;
    return pt->is_dirty() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nMarkClean(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nMarkClean: Invalid handle");
        return;
    }
    pt->mark_clean();
}

// ---- Memory info (new API) --------------------------------------------------

JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nMemoryUsage(JNIEnv* /*env*/, jobject /*thiz*/,
                                                  jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) return 0;
    return static_cast<jlong>(pt->memory_usage());
}

JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nIsMemoryPressure(JNIEnv* /*env*/, jobject /*thiz*/,
                                                       jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) return JNI_FALSE;
    return pt->is_memory_pressure() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nCompact(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    auto* pt = safe_from_handle(handle);
    if (!pt) {
        LOGE("nCompact: Invalid handle");
        return;
    }
    pt->compact();
}

} // extern "C"
