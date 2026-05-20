// ============================================================
//  SIN Editor -- JNI Bridge
//  Exposes the C++ PieceTable engine to Kotlin
//
//  Kotlin loads the lib via:
//    System.loadLibrary("sineditor_engine")
//
//  Each "document" is a heap-allocated PieceTable identified
//  by a Long handle passed back to Kotlin.
// ============================================================
#include "piece_table.h"

#include <jni.h>
#include <android/log.h>
#include <cassert>
#include <cstring>
#include <string>

#define TAG "SINEditorJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---- Handle helpers ---------------------------------------------------------
static sin::PieceTable* from_handle(jlong h) {
    assert(h != 0);
    return reinterpret_cast<sin::PieceTable*>(h);
}

// ---- JNI helpers: jstring <-> std::string -----------------------------------
static std::string j2s(JNIEnv* env, jstring js) {
    if (!js) return {};
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string s(c);
    env->ReleaseStringUTFChars(js, c);
    return s;
}

static jstring s2j(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

// ============================================================
//  C O R E    A P I
//  Package: com.sineditor.app  --  class: EditorEngine
// ============================================================
extern "C" {

// Create a new document and return its handle (Long)
JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nCreate(JNIEnv* env, jobject /*thiz*/,
                                             jstring initial_text) {
    std::string text = j2s(env, initial_text);
    auto* pt = new sin::PieceTable(std::move(text));
    LOGI("nCreate -> handle=%p", (void*)pt);
    return reinterpret_cast<jlong>(pt);
}

// Destroy document
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nDestroy(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    delete from_handle(handle);
    LOGI("nDestroy handle freed");
}

// Insert text at char position
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nInsert(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle,
                                             jlong char_pos,
                                             jstring text) {
    std::string s = j2s(env, text);
    from_handle(handle)->insert(static_cast<size_t>(char_pos), s);
}

// Erase `count` chars starting at char_pos
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nErase(JNIEnv* /*env*/, jobject /*thiz*/,
                                            jlong handle,
                                            jlong char_pos,
                                            jlong count) {
    from_handle(handle)->erase(
        static_cast<size_t>(char_pos),
        static_cast<size_t>(count));
}

// Full document text
JNIEXPORT jstring JNICALL
Java_com_sineditor_app_EditorEngine_nGetText(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle) {
    return s2j(env, from_handle(handle)->text());
}

// Single line text (by 0-based index)
JNIEXPORT jstring JNICALL
Java_com_sineditor_app_EditorEngine_nGetLine(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle,
                                              jlong line_idx) {
    return s2j(env, from_handle(handle)->line(static_cast<size_t>(line_idx)));
}

// Total line count
JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nLineCount(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    return static_cast<jlong>(from_handle(handle)->line_count());
}

// Total char count
JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nCharCount(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    return static_cast<jlong>(from_handle(handle)->char_count());
}

// Offset -> line/col  (returns long[] {line, col})
JNIEXPORT jlongArray JNICALL
Java_com_sineditor_app_EditorEngine_nOffsetToPoint(JNIEnv* env, jobject /*thiz*/,
                                                    jlong handle,
                                                    jlong char_pos) {
    auto pt  = from_handle(handle)->offset_to_point(static_cast<size_t>(char_pos));
    jlongArray arr = env->NewLongArray(2);
    jlong buf[2]   = {static_cast<jlong>(pt.line), static_cast<jlong>(pt.col)};
    env->SetLongArrayRegion(arr, 0, 2, buf);
    return arr;
}

// line/col -> offset
JNIEXPORT jlong JNICALL
Java_com_sineditor_app_EditorEngine_nPointToOffset(JNIEnv* /*env*/, jobject /*thiz*/,
                                                    jlong handle,
                                                    jlong line,
                                                    jlong col) {
    sin::Point p{static_cast<size_t>(line), static_cast<size_t>(col)};
    return static_cast<jlong>(from_handle(handle)->point_to_offset(p));
}

// Undo
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nUndo(JNIEnv* /*env*/, jobject /*thiz*/,
                                           jlong handle) {
    from_handle(handle)->undo();
}

// Redo
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nRedo(JNIEnv* /*env*/, jobject /*thiz*/,
                                           jlong handle) {
    from_handle(handle)->redo();
}

// isDirty
JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nIsDirty(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    return from_handle(handle)->is_dirty() ? JNI_TRUE : JNI_FALSE;
}

// markClean
JNIEXPORT void JNICALL
Java_com_sineditor_app_EditorEngine_nMarkClean(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jlong handle) {
    from_handle(handle)->mark_clean();
}

// canUndo
JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nCanUndo(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    return from_handle(handle)->can_undo() ? JNI_TRUE : JNI_FALSE;
}

// canRedo
JNIEXPORT jboolean JNICALL
Java_com_sineditor_app_EditorEngine_nCanRedo(JNIEnv* /*env*/, jobject /*thiz*/,
                                              jlong handle) {
    return from_handle(handle)->can_redo() ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
