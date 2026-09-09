//==============================================================================
//
//  runtime.cpp
//
//==============================================================================

#include "runtime.hpp"
#include "log.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace a51 {

//==============================================================================

Runtime::Runtime()
{
    m_Ready        = false;
    m_ArchiveCount = 0;
    m_ScreenWidth  = 1280;
    m_ScreenHeight = 720;
    m_LogCount     = 0;

    for( s32 i = 0; i < A51_RUNTIME_LOG_LINES; i++ )
        m_Log[i][0] = 0;
}

//------------------------------------------------------------------------------

Runtime::~Runtime()
{
    Shutdown();
}

//------------------------------------------------------------------------------

bool Runtime::Init( void )
{
    if( m_Ready ) return true;

    m_Touch.SetLayout( &m_Layout );
    m_Touch.Reset();

    m_Input.Clear();

    m_LogCount = 0;
    Log( "Area 51 Android runtime starting" );

    m_Ready = true;
    return true;
}

//------------------------------------------------------------------------------

void Runtime::Shutdown( void )
{
    if( !m_Ready ) return;

    UnmountAll();
    m_Ready = false;
}

//==============================================================================
//  Frame
//==============================================================================

void Runtime::BeginFrame( void )
{
    m_Input.BeginFrame();
    m_Touch.Update( m_Input );
}

//------------------------------------------------------------------------------

void Runtime::EndFrame( f32 FrameMs, f32 CpuMs )
{
    m_Perf.PushFrame( FrameMs, CpuMs );
    m_Input.EndFrame();
}

//------------------------------------------------------------------------------

f32 Runtime::SleepMs( f32 FrameStartMs, f32 NowMs )
{
    return m_Pacer.SleepMs( FrameStartMs, NowMs );
}

//==============================================================================
//  Game data
//==============================================================================

bool Runtime::MountArchive( const char* pPath )
{
    if( !pPath || !pPath[0] ) return false;
    if( m_ArchiveCount >= A51_RUNTIME_MAX_ARCHIVES )
    {
        Log( "too many archives mounted" );
        return false;
    }

    // Do not mount the same file twice.
    for( s32 i = 0; i < m_ArchiveCount; i++ )
        if( strcmp( m_Archives[i].GetPath(), pPath ) == 0 )
            return true;

    DfsArchive& Archive = m_Archives[ m_ArchiveCount ];
    if( !Archive.Open( pPath ) )
    {
        Log( "MOUNT FAILED %s : %s", pPath, Archive.GetError() );
        return false;
    }

    m_ArchiveCount++;
    Log( "MOUNTED %s : %d files, %d splits", pPath, Archive.GetFileCount(), Archive.GetSubFileCount() );
    return true;
}

//------------------------------------------------------------------------------

void Runtime::UnmountAll( void )
{
    for( s32 i = 0; i < m_ArchiveCount; i++ )
        m_Archives[i].Close();
    m_ArchiveCount = 0;
}

//------------------------------------------------------------------------------

DfsArchive* Runtime::GetArchive( s32 i )
{
    if( i < 0 || i >= m_ArchiveCount ) return nullptr;
    return &m_Archives[i];
}

//------------------------------------------------------------------------------

s32 Runtime::TotalFiles( void ) const
{
    s32 Total = 0;
    for( s32 i = 0; i < m_ArchiveCount; i++ )
        Total += m_Archives[i].GetFileCount();
    return Total;
}

//------------------------------------------------------------------------------

bool Runtime::FileExists( const char* pPath )
{
    // Later mounts win, exactly like the engine's file system search order.
    for( s32 i = m_ArchiveCount - 1; i >= 0; i-- )
        if( m_Archives[i].FindFile( pPath ) >= 0 )
            return true;
    return false;
}

//------------------------------------------------------------------------------

s32 Runtime::ReadFile( const char* pPath, u32 Offset, void* pDst, s32 Length )
{
    for( s32 i = m_ArchiveCount - 1; i >= 0; i-- )
    {
        s32 Index = m_Archives[i].FindFile( pPath );
        if( Index >= 0 )
            return m_Archives[i].Read( Index, Offset, pDst, Length );
    }
    return 0;
}

//==============================================================================
//  Screen
//==============================================================================

void Runtime::SetScreen( s32 WidthPx, s32 HeightPx )
{
    if( WidthPx  < 1 ) WidthPx  = 1;
    if( HeightPx < 1 ) HeightPx = 1;

    if( WidthPx == m_ScreenWidth && HeightPx == m_ScreenHeight ) return;

    m_ScreenWidth  = WidthPx;
    m_ScreenHeight = HeightPx;

    m_Layout.ClampToScreen( GetAspect() );
}

//------------------------------------------------------------------------------

f32 Runtime::GetAspect( void ) const
{
    if( m_ScreenHeight <= 0 ) return 1.0f;
    return (f32)m_ScreenWidth / (f32)m_ScreenHeight;
}

//==============================================================================
//  Log
//==============================================================================

void Runtime::Log( const char* pFmt, ... )
{
    char Line[ A51_RUNTIME_LOG_WIDTH ];

    va_list Args;
    va_start( Args, pFmt );
    vsnprintf( Line, sizeof( Line ), pFmt, Args );
    va_end( Args );

    if( m_LogCount < A51_RUNTIME_LOG_LINES )
    {
        snprintf( m_Log[ m_LogCount ], A51_RUNTIME_LOG_WIDTH, "%s", Line );
        m_LogCount++;
    }
    else
    {
        // Scroll one line up.
        for( s32 i = 1; i < A51_RUNTIME_LOG_LINES; i++ )
            snprintf( m_Log[i-1], A51_RUNTIME_LOG_WIDTH, "%s", m_Log[i] );
        snprintf( m_Log[ A51_RUNTIME_LOG_LINES - 1 ], A51_RUNTIME_LOG_WIDTH, "%s", Line );
    }

    A51_LOGI( "%s", Line );
}

//------------------------------------------------------------------------------

const char* Runtime::LogLine( s32 i ) const
{
    if( i < 0 || i >= m_LogCount ) return "";
    return m_Log[i];
}

//==============================================================================

Runtime& runtime()
{
    static Runtime s_Runtime;
    return s_Runtime;
}

} // namespace a51
//==============================================================================
