//==============================================================================
//
//  a51_types.hpp
//
//  Common types and small helpers shared by every module of the Android
//  runtime.  The naming mirrors x_files (x_types.hpp) so that code ported from
//  the original engine reads the same.
//
//  IMPORTANT: nothing in this header may depend on Android / JNI so the whole
//  core can be compiled and unit-tested on a desktop host.
//
//==============================================================================

#ifndef A51_TYPES_HPP
#define A51_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

//==============================================================================
//  BASIC TYPES
//==============================================================================

typedef std::int8_t     s8;
typedef std::uint8_t    u8;
typedef std::int16_t    s16;
typedef std::uint16_t   u16;
typedef std::int32_t    s32;
typedef std::uint32_t   u32;
typedef std::int64_t    s64;
typedef std::uint64_t   u64;
typedef float           f32;
typedef double          f64;

//==============================================================================
//  SMALL HELPERS
//==============================================================================

inline s32 a51_min( s32 a, s32 b ) { return (a < b) ? a : b; }
inline s32 a51_max( s32 a, s32 b ) { return (a > b) ? a : b; }
inline f32 a51_min( f32 a, f32 b ) { return (a < b) ? a : b; }
inline f32 a51_max( f32 a, f32 b ) { return (a > b) ? a : b; }

inline f32 a51_clamp( f32 v, f32 lo, f32 hi )
{
    if( v < lo ) return lo;
    if( v > hi ) return hi;
    return v;
}

inline s32 a51_clamp( s32 v, s32 lo, s32 hi )
{
    if( v < lo ) return lo;
    if( v > hi ) return hi;
    return v;
}

//------------------------------------------------------------------------------
//  Little endian accessors.
//
//  The Area 51 disc images (.dfs) are always little endian.  The original code
//  used LITTLE_ENDIAN_32() which is a no-op on x86 and a swap on the consoles.
//  On ARM Android the CPU is little endian too, but we read through these
//  helpers anyway so the reader is correct on any host architecture.
//------------------------------------------------------------------------------

inline u16 a51_read_le16( const void* p )
{
    const u8* b = (const u8*)p;
    return (u16)( b[0] | ( b[1] << 8 ) );
}

inline s16 a51_read_le16s( const void* p ) { return (s16)a51_read_le16( p ); }

inline u32 a51_read_le32( const void* p )
{
    const u8* b = (const u8*)p;
    return (u32)b[0] | ( (u32)b[1] << 8 ) | ( (u32)b[2] << 16 ) | ( (u32)b[3] << 24 );
}

inline s32 a51_read_le32s( const void* p ) { return (s32)a51_read_le32( p ); }

inline void a51_write_le16( void* p, u16 v )
{
    u8* b = (u8*)p;
    b[0] = (u8)( v & 0xFF );
    b[1] = (u8)( ( v >> 8 ) & 0xFF );
}

inline void a51_write_le32( void* p, u32 v )
{
    u8* b = (u8*)p;
    b[0] = (u8)( v & 0xFF );
    b[1] = (u8)( ( v >> 8  ) & 0xFF );
    b[2] = (u8)( ( v >> 16 ) & 0xFF );
    b[3] = (u8)( ( v >> 24 ) & 0xFF );
}

//------------------------------------------------------------------------------
//  Case insensitive ASCII comparison used for the virtual file system, because
//  game code requests paths in many different cases.
//------------------------------------------------------------------------------

inline char a51_tolower( char c )
{
    return ( c >= 'A' && c <= 'Z' ) ? (char)( c + 32 ) : c;
}

inline int a51_stricmp( const char* a, const char* b )
{
    if( a == nullptr ) a = "";
    if( b == nullptr ) b = "";
    for( ;; )
    {
        char ca = a51_tolower( *a );
        char cb = a51_tolower( *b );
        if( ca != cb ) return (int)ca - (int)cb;
        if( ca == 0 )  return 0;
        a++; b++;
    }
}

inline bool a51_strieq( const char* a, const char* b ) { return a51_stricmp( a, b ) == 0; }

//------------------------------------------------------------------------------
//  Path helpers (forward slashes only; backslashes are normalised).
//------------------------------------------------------------------------------

inline void a51_normalise_path( char* s )
{
    for( char* p = s; *p; p++ )
        if( *p == '\\' ) *p = '/';
}

//==============================================================================
//  TIMING (host portable, used by the frame governor tests)
//==============================================================================

#ifdef __ANDROID__
    #include <time.h>
    inline f64 a51_now_ms( void )
    {
        struct timespec ts;
        clock_gettime( CLOCK_MONOTONIC, &ts );
        return (f64)ts.tv_sec * 1000.0 + (f64)ts.tv_nsec * 0.000001;
    }
#else
    #include <chrono>
    inline f64 a51_now_ms( void )
    {
        return std::chrono::duration<f64, std::milli>(
                   std::chrono::steady_clock::now().time_since_epoch() ).count();
    }
#endif

//==============================================================================
//  BUILD ASSERT - catches struct layout mistakes at compile time
//==============================================================================

#define A51_STATIC_ASSERT( cond, msg ) static_assert( cond, msg )

#endif // A51_TYPES_HPP
//==============================================================================
