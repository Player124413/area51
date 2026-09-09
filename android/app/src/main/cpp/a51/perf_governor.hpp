//==============================================================================
//
//  perf_governor.hpp
//
//  The performance heart of the Android runtime.
//
//  Goal: a *stable* frame rate on any phone, from a 1 GB RAM budget device to a
//  flagship.  It watches real frame times and reacts in two independent ways:
//
//    1. render scale  - the scene is drawn into an offscreen buffer that can be
//                       as small as 35% of the screen and is then upscaled.
//                       This is by far the cheapest way to buy frame time on a
//                       mobile GPU, because fill rate is what usually hurts.
//    2. quality level - a small integer the renderer uses to switch off the
//                       expensive things (shadows, extra draw distance,
//                       particle counts, post effects).
//
//  On top of that it reacts to the platform:
//    * thermal status  (PowerManager.THERMAL_STATUS_*) - back off *before* the
//      kernel starts throttling the CPU/GPU, which is what causes the classic
//      "smooth for 2 minutes then stutter forever" behaviour;
//    * memory pressure - shrinks the asset cache instead of getting killed by
//      the low memory killer.
//
//  Everything here is deterministic, allocation free and free of Android
//  dependencies so it can be unit tested on a desktop.
//
//==============================================================================

#ifndef A51_PERF_GOVERNOR_HPP
#define A51_PERF_GOVERNOR_HPP

#include "a51_types.hpp"

namespace a51 {

//==============================================================================
//  Configuration
//==============================================================================

struct PerfConfig
{
    f32     TargetFps;          // what we aim for (30 / 60 / 90 / 120)
    f32     MinScale;           // lowest render scale we are willing to use
    f32     MaxScale;           // 1.0 = native resolution
    f32     ScaleStep;          // how much the scale moves per adjustment
    s32     DownFrames;         // bad frames in a row before backing off
    s32     UpFrames;           // good frames in a row before raising quality
    s32     CooldownFrames;     // frames to wait after any adjustment
    s32     QualityLevels;      // number of quality steps (0 .. N-1)
    s32     TextureBudgetMb;    // cache size for a device with no pressure

                 PerfConfig();  // sane defaults: 60 fps, 0.40 .. 1.00
};

//==============================================================================
//  Governor
//==============================================================================

#define A51_PERF_HISTORY 64

class PerfGovernor
{
public:
                     PerfGovernor();

    void             Configure        ( const PerfConfig& Config );
    const PerfConfig& GetConfig       ( void ) const { return m_Config; }

    // User switch: when false the governor only measures, it never changes
    // quality.  Handy for people who want to lock everything by hand.
    void             SetAutoQuality   ( bool Enabled ) { m_AutoQuality = Enabled; }
    bool             GetAutoQuality   ( void ) const   { return m_AutoQuality; }

    // Hard caps chosen by the user in the settings screen.
    void             SetQualityCeiling( s32 Level );
    void             SetFixedScale    ( f32 Scale );      // <= 0 disables the lock

    // Platform feedback.
    void             SetThermalStatus ( s32 Status );     // PowerManager.THERMAL_STATUS_*
    void             SetMemoryPressure( f32 Pressure );   // 0.0 .. 1.0
    void             SetTargetFps     ( f32 Fps );

    // Feed one finished frame.  FrameMs is wall clock time of the whole frame,
    // CpuMs the part spent on the CPU (used to tell "GPU bound" from "CPU
    // bound" in the HUD).
    void             PushFrame        ( f32 FrameMs, f32 CpuMs );

    // What the renderer must use *this* frame.
    f32              GetRenderScale   ( void ) const { return m_RenderScale; }
    s32              GetQualityLevel  ( void ) const { return m_QualityLevel; }

    // Statistics for the HUD.
    f32              GetFps           ( void ) const { return m_DisplayFps; }
    f32              GetFrameTimeMs   ( void ) const { return m_EmaFrameMs; }
    f32              GetFrameTimeP95  ( void ) const;
    f32              GetAvgFrameMs    ( void ) const { return m_AvgFrameMs; }
    f32              GetWorstFrameMs  ( void ) const { return m_WorstFrameMs; }
    f32              GetCpuMs         ( void ) const { return m_EmaCpuMs; }
    f32              GetStability     ( void ) const;   // 0.0 (wild) .. 1.0 (rock solid)
    u32              GetFrameCount    ( void ) const { return m_FrameCount; }
    u32              GetDroppedFrames ( void ) const { return m_DroppedFrames; }
    u32              GetQualityDrops  ( void ) const { return m_QualityDrops; }
    u32              GetQualityRaises ( void ) const { return m_QualityRaises; }

    // Recommended caps.  The activity uses these to re-configure vsync / the
    // frame pacer when the situation changes.
    s32              GetSuggestedFpsCap( void ) const;
    s32              GetTextureBudgetMb( void ) const;

    // Reset all history (new level, coming back from background, ...).
    void             Reset            ( void );

private:
    void             ApplyThermalLimits( void );
    f32              BudgetMs          ( void ) const;
    f32              MaxScaleNow       ( void ) const;
    s32              MaxQualityNow     ( void ) const;

private:
    PerfConfig       m_Config;

    bool             m_AutoQuality;
    s32              m_QualityCeiling;
    f32              m_FixedScale;

    s32              m_ThermalStatus;
    f32              m_MemoryPressure;

    f32              m_RenderScale;
    s32              m_QualityLevel;

    // Control state.
    f32              m_EmaFrameMs;
    f32              m_EmaCpuMs;
    f32              m_DisplayFps;
    f32              m_AvgFrameMs;
    f32              m_WorstFrameMs;
    s32              m_BadStreak;
    s32              m_GoodStreak;
    s32              m_Cooldown;

    // Ring buffer of recent frame times.
    f32              m_History[ A51_PERF_HISTORY ];
    s32              m_HistoryCount;
    s32              m_HistoryIndex;

    u32              m_FrameCount;
    u32              m_DroppedFrames;
    u32              m_QualityDrops;
    u32              m_QualityRaises;
};

//==============================================================================
//  Frame pacer - keeps the frame rate honest when the target is below the
//  display refresh rate (60 fps target on a 120 Hz screen, 30 fps battery
//  saver, ...).  Deadline based, so small overruns do not accumulate.
//==============================================================================

class FramePacer
{
public:
                     FramePacer();

    void             SetTargetFps     ( f32 Fps );
    void             SetEnabled       ( bool Enabled ) { m_Enabled = Enabled; }
    bool             IsEnabled        ( void ) const   { return m_Enabled; }

    // Call with the time the frame *started* and the time it finished.
    // Returns how many milliseconds the thread should sleep (0 = go straight
    // on).  NowMs must come from the same clock as FrameStartMs.
    f32              SleepMs          ( f32 FrameStartMs, f32 NowMs );

    // True when the previous frame missed its deadline.
    bool             MissedDeadline   ( void ) const   { return m_Missed; }
    f32              GetBudgetMs      ( void ) const;

    void             Reset            ( void );

private:
    bool             m_Enabled;
    f32              m_TargetFps;
    f64              m_NextDeadline;
    bool             m_HaveDeadline;
    bool             m_Missed;
};

} // namespace a51

#endif // A51_PERF_GOVERNOR_HPP
//==============================================================================
