//==============================================================================
//
//  stubs/jni.h
//
//  A *compile check only* stand-in for the Android NDK header.  It exists so
//  that jni_bridge.cpp can be type checked on a desktop (and in CI on a plain
//  ubuntu runner) without downloading the NDK.  The real header is used for the
//  actual APK build - see app/src/main/cpp/CMakeLists.txt.
//
//  Only the members that jni_bridge.cpp really calls are declared here, with the
//  same signatures as the NDK, so a typo in a call still fails to compile.
//
//==============================================================================

#ifndef A51_STUB_JNI_H
#define A51_STUB_JNI_H

#include <cstddef>

typedef unsigned char   jboolean;
typedef signed char     jbyte;
typedef unsigned short  jchar;
typedef short           jshort;
typedef int             jint;
typedef long long       jlong;
typedef float           jfloat;
typedef double          jdouble;
typedef jint            jsize;

struct _jobject;
typedef _jobject*       jobject;
typedef jobject         jclass;
typedef jobject         jstring;
typedef jobject         jarray;
typedef jarray          jbyteArray;
typedef jarray          jfloatArray;
typedef jarray          jintArray;
typedef jarray          jobjectArray;

#define JNI_TRUE   1
#define JNI_FALSE  0

#define JNIEXPORT  __attribute__(( visibility( "default" ) ))
#define JNICALL
#define JNICALL_S

struct JNIEnv
{
    jstring     NewStringUTF        ( const char* pUtf );
    const char* GetStringUTFChars   ( jstring Str, jboolean* pIsCopy );
    void        ReleaseStringUTFChars( jstring Str, const char* pUtf );

    jbyteArray  NewByteArray        ( jsize Length );
    void        SetByteArrayRegion  ( jbyteArray Array, jsize Start, jsize Length, const jbyte* pBuf );
    jbyte*      GetByteArrayElements( jbyteArray Array, jboolean* pIsCopy );
    void        ReleaseByteArrayElements( jbyteArray Array, jbyte* pElems, jint Mode );

    jfloatArray NewFloatArray       ( jsize Length );
    void        SetFloatArrayRegion ( jfloatArray Array, jsize Start, jsize Length, const jfloat* pBuf );

    jintArray   NewIntArray         ( jsize Length );
    void        SetIntArrayRegion   ( jintArray Array, jsize Start, jsize Length, const jint* pBuf );
};

#endif // A51_STUB_JNI_H
//==============================================================================
