//==============================================================================
//
//  stubs/android/log.h - compile check only, see stubs/jni.h.
//
//==============================================================================

#ifndef A51_STUB_ANDROID_LOG_H
#define A51_STUB_ANDROID_LOG_H

#define ANDROID_LOG_UNKNOWN 0
#define ANDROID_LOG_DEFAULT 1
#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_DEBUG   3
#define ANDROID_LOG_INFO    4
#define ANDROID_LOG_WARN    5
#define ANDROID_LOG_ERROR   6
#define ANDROID_LOG_FATAL   7
#define ANDROID_LOG_SILENT  8

int __android_log_print( int prio, const char* tag, const char* fmt, ... );

#endif // A51_STUB_ANDROID_LOG_H
//==============================================================================
