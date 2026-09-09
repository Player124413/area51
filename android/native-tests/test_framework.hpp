//==============================================================================
//
//  test_framework.hpp - a 60 line test harness.
//
//  No external dependencies: the Android NDK build and the host build both
//  compile the very same core sources, so the tests can run anywhere there is
//  a C++17 compiler (that is what .github/workflows/android-build.yml does on
//  every push).
//
//==============================================================================

#ifndef A51_TEST_FRAMEWORK_HPP
#define A51_TEST_FRAMEWORK_HPP

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

namespace test {

struct Failure
{
    std::string File;
    int         Line;
    std::string Message;
};

inline std::vector< Failure >& failures()
{
    static std::vector< Failure > s_Failures;
    return s_Failures;
}

inline int& checks()
{
    static int s_Checks = 0;
    return s_Checks;
}

inline void fail( const char* pFile, int Line, const std::string& Msg )
{
    Failure F;
    F.File    = pFile;
    F.Line    = Line;
    F.Message = Msg;
    failures().push_back( F );
    std::printf( "    FAIL %s:%d  %s\n", pFile, Line, Msg.c_str() );
}

typedef void (*TestFn)( void );

struct TestCase
{
    const char* Name;
    TestFn      Fn;
};

inline std::vector< TestCase >& cases()
{
    static std::vector< TestCase > s_Cases;
    return s_Cases;
}

struct Registrar
{
    Registrar( const char* pName, TestFn Fn )
    {
        TestCase C;
        C.Name = pName;
        C.Fn   = Fn;
        cases().push_back( C );
    }
};

inline int run_all( void )
{
    int Passed = 0;

    for( size_t i = 0; i < cases().size(); i++ )
    {
        size_t Before = failures().size();
        std::printf( "  %s\n", cases()[i].Name );
        cases()[i].Fn();
        if( failures().size() == Before ) Passed++;
    }

    std::printf( "\n" );
    if( failures().empty() )
    {
        std::printf( "ALL TESTS PASSED  (%d cases, %d checks)\n", (int)cases().size(), checks() );
        return 0;
    }

    std::printf( "%d FAILURE(S) in %d of %d cases\n",
                 (int)failures().size(),
                 (int)cases().size() - Passed,
                 (int)cases().size() );
    return 1;
}

} // namespace test

#define TEST( name )                                                          \
    static void test_##name( void );                                          \
    static ::test::Registrar reg_##name( #name, test_##name );                \
    static void test_##name( void )

#define CHECK( cond )                                                         \
    do {                                                                      \
        ::test::checks()++;                                                   \
        if( !( cond ) ) ::test::fail( __FILE__, __LINE__, "CHECK(" #cond ")" ); \
    } while( 0 )

#define CHECK_EQ_INT( a, b )                                                  \
    do {                                                                      \
        ::test::checks()++;                                                   \
        long long _a = (long long)( a );                                      \
        long long _b = (long long)( b );                                      \
        if( _a != _b )                                                        \
        {                                                                     \
            char _msg[256];                                                   \
            snprintf( _msg, sizeof( _msg ), "%s == %s  (%lld vs %lld)",       \
                      #a, #b, _a, _b );                                       \
            ::test::fail( __FILE__, __LINE__, _msg );                         \
        }                                                                     \
    } while( 0 )

#define CHECK_NEAR( a, b, tol )                                               \
    do {                                                                      \
        ::test::checks()++;                                                   \
        double _a = (double)( a );                                            \
        double _b = (double)( b );                                            \
        if( fabs( _a - _b ) > ( tol ) )                                       \
        {                                                                     \
            char _msg[256];                                                   \
            snprintf( _msg, sizeof( _msg ), "%s ~= %s  (%.6f vs %.6f)",       \
                      #a, #b, _a, _b );                                       \
            ::test::fail( __FILE__, __LINE__, _msg );                         \
        }                                                                     \
    } while( 0 )

#define CHECK_STR( a, b )                                                     \
    do {                                                                      \
        ::test::checks()++;                                                   \
        if( strcmp( ( a ), ( b ) ) != 0 )                                     \
        {                                                                     \
            char _msg[512];                                                   \
            snprintf( _msg, sizeof( _msg ), "%s == %s  (\"%s\" vs \"%s\")",   \
                      #a, #b, ( a ), ( b ) );                                 \
            ::test::fail( __FILE__, __LINE__, _msg );                         \
        }                                                                     \
    } while( 0 )

#endif // A51_TEST_FRAMEWORK_HPP
//==============================================================================
