# Tauri — garder toutes les classes utilisées par les fonctions JNI Rust
-keep class com.openbingo.app.** { *; }
-keep class app.tauri.** { *; }
-dontwarn app.tauri.**

# WebView JS interface
-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}

# Empêcher le renommage des classes référencées depuis le code natif (.so)
-keepclasseswithmembernames class * {
    native <methods>;
}

-keepattributes SourceFile,LineNumberTable
