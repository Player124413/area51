//==============================================================================
//
//  perf_governor.cpp
//
//==============================================================================

#include "perf_governor.hpp"

#include <algorithm>

namespace a51 {

//==============================================================================
//  PerfConfig
//==============================================================================

PerfConfig::PerfConfig()
{
    TargetFps        = 60.0f;
    MinScale         = 0.40f;
    MaxScale         = 1.00f;
    ScaleStep        = 0.05f;
    DownFrames       = 6;
    UpFrames         = 120;
    CooldownFrames   = 30;
    QualityLevels    = 4;
    TextureBudgetMb  = 256;
}

//==============================================================================
//  PerfGovernor
//==============================================================================

PerfGovernor::PerfGovernor()
{
    m_Config = PerfConfig();
    Reset();
}

//------------------------------------------------------------------------------

void PerfGovernor::Reset( void )
{
    m_AutoQuality    = true;
    m_QualityCeiling = a51_max( 0, m_Config.QualityLevels - 1 );
    m_FixedScale     = 0.0f;

    m_ThermalStatus  = 0;
    m_MemoryPressure = 0.0f;

    m_RenderScale    = m_Config.MaxScale;
    m_QualityLevel   = m_QualityCeiling;

    m_EmaFrameMs     = 0.0f;
    m_EmaCpuMs       = 0.0f;
    m_DisplayFps     = 0.0f;
    m_AvgFrameMs     = 0.0f;
    m_WorstFrameMs   = 0.0f;
    m_BadStreak      = 0;
    m_GoodStreak     = 0;
    m_Cooldown       = 0;

    for( s32 i = 0; i < A51_PERF_HISTORY; i++ )
        m_History[i] = 0.0f;
    m_HistoryCount   = 0;
    m_HistoryIndex   = 0;

    m_FrameCount     = 0;
    m_DroppedFrames  = 0;
    m_QualityDrops   = 0;
    m_QualityRaises  = 0;
}

//------------------------------------------------------------------------------

void PerfGovernor::Configure( const PerfConfig& Config )
{
    m_Config = Config;

    if( m_Config.TargetFps      < 1.0f  ) m_Config.TargetFps      = 60.0f;
    if( m_Config.TargetFps      > 240.0f) m_Config.TargetFps      = 240.0f;
    if( m_Config.MinScale       < 0.20f ) m_Config.MinScale       = 0.20f;
    if( m_Config.MaxScale       > 1.00f ) m_Config.MaxScale       = 1.00f;
    if( m_Config.MinScale       > m_Config.MaxScale ) m_Config.MinScale = m_Config.MaxScale;
    if( m_Config.ScaleStep      < 0.01f ) m_Config.ScaleStep      = 0.01f;
    if( m_Config.ScaleStep      > 0.25f ) m_Config.ScaleStep      = 0.25f;
    if( m_Config.DownFrames     < 1     ) m_Config.DownFrames     = 1;
    if( m_Config.UpFrames       < 1     ) m_Config.UpFrames       = 1;
    if( m_Config.CooldownFrames < 0     ) m_Config.CooldownFrames = 0;
    if( m_Config.QualityLevels  < 1     ) m_Config.QualityLevels  = 1;
    if( m_Config.TextureBudgetMb < 16   ) m_Config.TextureBudgetMb = 16;

    m_QualityCeiling = a51_clamp( m_QualityCeiling, 0, m_Config.QualityLevels - 1 );
    m_QualityLevel   = a51_clamp( m_QualityLevel,   0, m_Config.QualityLevels - 1 );
    m_RenderScale    = a51_clamp( m_RenderScale, m_Config.MinScale, m_Config.MaxScale );
}

//------------------------------------------------------------------------------

void PerfGovernor::SetQualityCeiling( s32 Level )
{
    m_QualityCeiling = a51_clamp( Level, 0, a51_max( 0, m_Config.QualityLevels - 1 ) );
    m_QualityLevel   = a51_min( m_QualityLevel, m_QualityCeiling );
}

//------------------------------------------------------------------------------

void PerfGovernor::SetFixedScale( f32 Scale )
{
    if( Scale <= 0.0f )
    {
        m_FixedScale = 0.0f;
        return;
    }
    m_FixedScale   = a51_clamp( Scale, m_Config.MinScale, m_Config.MaxScale );
    m_RenderScale  = m_FixedScale;
}

//------------------------------------------------------------------------------

void PerfGovernor::SetThermalStatus( s32 Status )
{
    m_ThermalStatus = a51_clamp( Status, 0, 6 );
}

//------------------------------------------------------------------------------

void PerfGovernor::SetMemoryPressure( f32 Pressure )
{
    m_MemoryPressure = a51_clamp( Pressure, 0.0f, 1.0f );
}

//------------------------------------------------------------------------------

void PerfGovernor::SetTargetFps( f32 Fps )
{
    m_Config.TargetFps = a51_clamp( Fps, 15.0f, 240.0f );
}

//------------------------------------------------------------------------------

f32 PerfGovernor::BudgetMs( void ) const
{
    return 1000.0f / m_Config.TargetFps;
}

//------------------------------------------------------------------------------
//  Thermal ceilings - PowerManager.THERMAL_STATUS_*:
//      0 NONE, 1 LIGHT, 2 MODERATE, 3 SEVERE, 4 CRITICAL, 5 EMERGENCY, 6 SHUTDOWN
//------------------------------------------------------------------------------

f32 PerfGovernor::MaxScaleNow( void ) const
{
    switch( m_ThermalStatus )
    {
        case 0:
        case 1:  return m_Config.MaxScale;
        case 2:  return a51_min( m_Config.MaxScale, 0.85f );
        case 3:  return a51_min( m_Config.MaxScale, 0.65f );
        case 4:  return a51_min( m_Config.MaxScale, 0.50f );
        default: return a51_min( m_Config.MaxScale, 0.40f );
    }
}

//------------------------------------------------------------------------------

s32 PerfGovernor::MaxQualityNow( void ) const
{
    s32 Ceiling = m_QualityCeiling;
    if( m_ThermalStatus >= 5 ) return 0;
    if( m_ThermalStatus >= 4 ) return 0;
    if( m_ThermalStatus >= 3 ) return a51_min( Ceiling, 1 );
    return Ceiling;
}

//------------------------------------------------------------------------------

void PerfGovernor::ApplyThermalLimits( void )
{
    f32 MaxScale   = MaxScaleNow();
    s32 MaxQuality = MaxQualityNow();

    if( m_RenderScale  > MaxScale   ) m_RenderScale  = MaxScale;
    if( m_QualityLevel > MaxQuality ) m_QualityLevel = MaxQuality;

    if( m_FixedScale > 0.0f && m_FixedScale > MaxScale )
        m_RenderScale = MaxScale;      // thermal always wins over a manual lock
    else if( m_FixedScale > 0.0f )
        m_RenderScale = m_FixedScale;
}

//------------------------------------------------------------------------------

void PerfGovernor::PushFrame( f32 FrameMs, f32 CpuMs )
{
    // Ignore nonsense (first frame after a pause, debugger stops, ...).
    if( FrameMs <  0.1f )   FrameMs = 0.1f;
    if( FrameMs > 1000.0f ) FrameMs = 1000.0f;
    if( CpuMs   <  0.0f )   CpuMs   = 0.0f;

    m_FrameCount++;

    // History ring buffer.
    m_History[ m_HistoryIndex ] = FrameMs;
    m_HistoryIndex = ( m_HistoryIndex + 1 ) % A51_PERF_HISTORY;
    if( m_HistoryCount < A51_PERF_HISTORY ) m_HistoryCount++;

    // Averages.
    if( m_FrameCount == 1 )
    {
        m_EmaFrameMs = FrameMs;
        m_EmaCpuMs   = CpuMs;
        m_AvgFrameMs = FrameMs;
    }
    else
    {
        m_EmaFrameMs += 0.15f * ( FrameMs - m_EmaFrameMs );
        m_EmaCpuMs   += 0.15f * ( CpuMs   - m_EmaCpuMs   );

        // Running average that does not lose precision after a long session.
        f64 Count = (f64)m_FrameCount;
        m_AvgFrameMs = (f32)( ( (f64)m_AvgFrameMs * ( Count - 1.0 ) + (f64)FrameMs ) / Count );
    }
    if( FrameMs > m_WorstFrameMs ) m_WorstFrameMs = FrameMs;

    // Instantaneous fps, smoothed only for display.
    f32 InstantFps = 1000.0f / FrameMs;
    m_DisplayFps += 0.05f * ( InstantFps - m_DisplayFps );

    const f32 Budget = BudgetMs();

    if( FrameMs > Budget * 1.5f )
        m_DroppedFrames++;

    //--------------------------------------------------------------------------
    //  Streaks.  Hysteresis: a frame between the two thresholds changes nothing,
    //  which is what stops the scale from hunting around the target.
    //--------------------------------------------------------------------------
    if( FrameMs > Budget * 1.10f )
    {
        m_BadStreak++;
        m_GoodStreak = 0;
    }
    else if( FrameMs < Budget * 0.92f )
    {
        m_GoodStreak++;
        m_BadStreak = 0;
    }

    if( m_Cooldown > 0 ) m_Cooldown--;

    //--------------------------------------------------------------------------
    //  React.
    //--------------------------------------------------------------------------
    if( m_AutoQuality && m_FixedScale <= 0.0f && m_Cooldown == 0 )
    {
        f32 MaxScale   = MaxScaleNow();
        s32 MaxQuality = MaxQualityNow();

        if( m_BadStreak >= m_Config.DownFrames )
        {
            bool Changed = false;

            // Resolution first - it is the cheapest frame time on a phone.
            if( m_RenderScale > m_Config.MinScale + 0.001f )
            {
                m_RenderScale = a51_max( m_Config.MinScale, m_RenderScale - m_Config.ScaleStep );
                Changed = true;
            }
            else if( m_QualityLevel > 0 )
            {
                m_QualityLevel--;
                Changed = true;
            }

            if( Changed )
            {
                m_QualityDrops++;
                m_Cooldown   = m_Config.CooldownFrames;
                m_BadStreak  = 0;
            }
        }
        else if( m_GoodStreak >= m_Config.UpFrames && m_ThermalStatus < 2 )
        {
            bool Changed = false;

            if( m_RenderScale < MaxScale - 0.001f )
            {
                m_RenderScale = a51_min( MaxScale, m_RenderScale + m_Config.ScaleStep );
                Changed = true;
            }
            else if( m_QualityLevel < MaxQuality )
            {
                m_QualityLevel++;
                Changed = true;
            }

            if( Changed )
            {
                m_QualityRaises++;
                m_Cooldown   = m_Config.CooldownFrames;
                m_GoodStreak = 0;
            }
        }
    }

    ApplyThermalLimits();
}

//------------------------------------------------------------------------------

f32 PerfGovernor::GetFrameTimeP95( void ) const
{
    if( m_HistoryCount == 0 ) return 0.0f;

    f32 Tmp[ A51_PERF_HISTORY ];
    for( s32 i = 0; i < m_HistoryCount; i++ )
        Tmp[i] = m_History[i];

    std::sort( Tmp, Tmp + m_HistoryCount );

    s32 Index = (s32)( (f32)( m_HistoryCount - 1 ) * 0.95f + 0.5f );
    return Tmp[ a51_clamp( Index, 0, m_HistoryCount - 1 ) ];
}

//------------------------------------------------------------------------------

f32 PerfGovernor::GetStability( void ) const
{
    if( m_HistoryCount < 8 ) return 1.0f;

    f32 Mean = 0.0f;
    for( s32 i = 0; i < m_HistoryCount; i++ ) Mean += m_History[i];
    Mean /= (f32)m_HistoryCount;

    f32 Var = 0.0f;
    for( s32 i = 0; i < m_HistoryCount; i++ )
    {
        f32 d = m_History[i] - Mean;
        Var += d * d;
    }
    f32 StdDev = sqrtf( Var / (f32)m_HistoryCount );

    // A jitter of half a frame budget already feels bad, so map that to 0.
    f32 Budget  = BudgetMs();
    f32 Normal  = StdDev / ( Budget * 0.5f );
    return a51_clamp( 1.0f - Normal, 0.0f, 1.0f );
}

//------------------------------------------------------------------------------

s32 PerfGovernor::GetSuggestedFpsCap( void ) const
{
    if( m_ThermalStatus >= 4 ) return 30;
    if( m_ThermalStatus >= 3 ) return a51_min( (s32)m_Config.TargetFps, 45 );

    // Everything is already at the floor and we still miss the target badly:
    // better to hold a smooth 30 than to stutter at 45.
    f32 Budget = BudgetMs();
    if( m_FrameCount > 120 &&
        m_EmaFrameMs > Budget * 1.35f &&
        m_RenderScale <= m_Config.MinScale + 0.001f &&
        m_QualityLevel == 0 )
        return 30;

    return (s32)( m_Config.TargetFps + 0.5f );
}

//------------------------------------------------------------------------------

s32 PerfGovernor::GetTextureBudgetMb( void ) const
{
    f32 Budget = (f32)m_Config.TextureBudgetMb;

    Budget *= ( 1.0f - 0.60f * m_MemoryPressure );
    if( m_ThermalStatus >= 3 ) Budget *= 0.60f;

    s32 Mb = (s32)( Budget + 0.5f );
    return a51_clamp( Mb, 32, m_Config.TextureBudgetMb );
}

//==============================================================================
//  FramePacer
//==============================================================================

FramePacer::FramePacer()
{
    m_Enabled      = true;
    m_TargetFps    = 60.0f;
    m_NextDeadline = 0.0;
    m_HaveDeadline = false;
    m_Missed       = false;
}

//------------------------------------------------------------------------------

void FramePacer::Reset( void )
{
    m_HaveDeadline = false;
    m_Missed       = false;
}

//------------------------------------------------------------------------------

void FramePacer::SetTargetFps( f32 Fps )
{
    f32 NewFps = a51_clamp( Fps, 10.0f, 240.0f );
    if( fabsf( NewFps - m_TargetFps ) > 0.01f )
    {
        m_TargetFps    = NewFps;
        m_HaveDeadline = false;
    }
}

//------------------------------------------------------------------------------

f32 FramePacer::GetBudgetMs( void ) const
{
    return 1000.0f / m_TargetFps;
}

//------------------------------------------------------------------------------

f32 FramePacer::SleepMs( f32 FrameStartMs, f32 NowMs )
{
    if( !m_Enabled || m_TargetFps <= 0.0f ) return 0.0f;

    f64 Budget = (f64)GetBudgetMs();

    // The deadline that applies to the frame that has just finished.
    f64 Deadline = m_HaveDeadline ? m_NextDeadline : ( (f64)FrameStartMs + Budget );
    f64 Sleep    = Deadline - (f64)NowMs;

    if( Sleep < 0.0 )
    {
        m_Missed = true;

        // Keep walking the deadline so short overruns are paid back, but never
        // let it fall behind "now": after a loading hitch or a GC pause we would
        // otherwise schedule a burst of zero-length frames to catch up.
        m_NextDeadline = Deadline + Budget;
        if( m_NextDeadline < (f64)NowMs )
            m_NextDeadline = (f64)NowMs + Budget;

        m_HaveDeadline = true;
        return 0.0f;
    }

    m_Missed       = false;
    m_NextDeadline = Deadline + Budget;
    m_HaveDeadline = true;
    return (f32)Sleep;
}

} // namespace a51
//==============================================================================
