# Everything the JNI layer touches by name must survive R8.
-keep class com.a51.android.core.NativeBridge { *; }

# Views inflated from XML and activities started by intent.
-keep class com.a51.android.** extends android.app.Activity
-keep class com.a51.android.** extends android.view.View { <init>(...); }
-keepclassmembers class com.a51.android.** extends android.view.View {
    <init>(android.content.Context, android.util.AttributeSet);
    <init>(android.content.Context, android.util.AttributeSet, int);
}

# ViewBinding classes are referenced by name.
-keep class com.a51.android.databinding.** { *; }

# Keep line numbers for readable crash reports, without keeping names.
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile

# The core is C++, nothing to obfuscate there, but keep the native method names.
-keepclasseswithmembernames class * {
    native <methods>;
}
