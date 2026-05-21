# ProGuard rules for SIN Editor
# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep JNI bridge class
-keep class com.sineditor.app.EditorEngine {
    private native *** n*(...);
}
