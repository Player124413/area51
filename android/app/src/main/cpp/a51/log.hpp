//==============================================================================
//
//  log.hpp - tiny logging facade for the runtime core.
//
//  On Android it goes to logcat, everywhere else (host unit tests) to stderr.
//
//==============================================================================

#ifndef A51_LOG_HPP
#define A51_LOG_HPP

#include <cstdio>

#ifdef __ANDROID__
    #include <android/log.h>
    #define A51_LOG_TAG "A51Native"
    #define A51_LOGI( ... ) __android_log_print( ANDROID_LOG_INFO,  A51_LOG_TAG, __VA_ARGS__ )
    #define A51_LOGW( ... ) __android_log_print( ANDROID_LOG_WARN,  A51_LOG_TAG, __VA_ARGS__ )
    #define A51_LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, A51_LOG_TAG, __VA_ARGS__ )
#else
    #define A51_LOGI( ... ) do { std::fprintf( stderr, "[a51] " ); std::fprintf( stderr, __VA_ARGS__ ); std::fprintf( stderr, "\n" ); } while( 0 )
    #define A51_LOGW( ... ) do { std::fprintf( stderr, "[a51][warn] " ); std::fprintf( stderr, __VA_ARGS__ ); std::fprintf( stderr, "\n" ); } while( 0 )
    #define A51_LOGE( ... ) do { std::fprintf( stderr, "[a51][err] " ); std::fprintf( stderr, __VA_ARGS__ ); std::fprintf( stderr, "\n" ); } while( 0 )
#endif

#endif // A51_LOG_HPP
//==============================================================================
