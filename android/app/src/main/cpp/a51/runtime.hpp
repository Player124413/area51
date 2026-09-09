//==============================================================================
//
//  runtime.hpp
//
//  The Android runtime object: everything the game session owns, in one place.
//
//      input state      - gadgets, filled by touch / gamepad / keyboard
//      touch runtime    - fingers -> gadgets
//      touch layout     - where the buttons are (editable)
//      perf governor    - render scale, quality level, frame pacing
//      mounted archives - the .dfs files the user imported
//      log              - what happened, shown on the HUD
//
//  The class itself does not touch OpenGL or JNI, so it can be instantiated in
//  the host tests as well.
//
//==============================================================================

#ifndef A51_RUNTIME_HPP
#define A51_RUNTIME_HPP

#include "a51_types.hpp"
#include "input_state.hpp"
#include "touch_layout.hpp"
#include "perf_governor.hpp"
#include "dfs_archive.hpp"

namespace a51 {

#define A51_RUNTIME_MAX_ARCHIVES 8
#define A51_RUNTIME_LOG_LINES    64
#define A51_RUNTIME_LOG_WIDTH    160

class Runtime
{
public:
                     Runtime();
                    ~Runtime();

    bool             Init           ( void );
    void             Shutdown       ( void );
    bool             IsReady        ( void ) const { return m_Ready; }

    // Called by the renderer thread.
    void             BeginFrame     ( void );
    void             EndFrame       ( f32 FrameMs, f32 CpuMs );
    f32              SleepMs        ( f32 FrameStartMs, f32 NowMs );

    InputState&      Input          ( void ) { return m_Input; }
    TouchRuntime&    Touch          ( void ) { return m_Touch; }
    TouchLayout&     Layout         ( void ) { return m_Layout; }
    PerfGovernor&    Perf           ( void ) { return m_Perf; }
    FramePacer&      Pacer          ( void ) { return m_Pacer; }

    // Game data.
    bool             MountArchive   ( const char* pPath );
    void             UnmountAll     ( void );
    s32              MountCount     ( void ) const { return m_ArchiveCount; }
    DfsArchive*      GetArchive     ( s32 i );
    bool             FileExists     ( const char* pPath );
    s32              ReadFile       ( const char* pPath, u32 Offset, void* pDst, s32 Length );
    s32              TotalFiles     ( void ) const;

    // Screen geometry, kept here so hit testing works without round trips.
    void             SetScreen      ( s32 WidthPx, s32 HeightPx );
    s32              GetScreenWidth ( void ) const { return m_ScreenWidth; }
    s32              GetScreenHeight( void ) const { return m_ScreenHeight; }
    f32              GetAspect      ( void ) const;

    // Log shown on the HUD.
    void             Log            ( const char* pFmt, ... );
    s32              LogCount       ( void ) const { return m_LogCount; }
    const char*      LogLine        ( s32 i ) const;

private:
                     Runtime( const Runtime& );
    Runtime&         operator = ( const Runtime& );

    bool             m_Ready;

    InputState       m_Input;
    TouchRuntime     m_Touch;
    TouchLayout      m_Layout;
    PerfGovernor     m_Perf;
    FramePacer       m_Pacer;

    DfsArchive       m_Archives[ A51_RUNTIME_MAX_ARCHIVES ];
    s32              m_ArchiveCount;

    s32              m_ScreenWidth;
    s32              m_ScreenHeight;

    char             m_Log[ A51_RUNTIME_LOG_LINES ][ A51_RUNTIME_LOG_WIDTH ];
    s32              m_LogCount;
};

// The one and only session (the app never runs two at a time).
Runtime& runtime();

} // namespace a51

#endif // A51_RUNTIME_HPP
//==============================================================================
