//==============================================================================
//
//  test_perf.cpp
//
//  The performance governor is what keeps the frame rate stable, so its
//  behaviour is pinned down here: it must react to load, converge, and then
//  stay put (no hunting), and it must back off when the phone gets hot.
//
//==============================================================================

#include "test_framework.hpp"

#include "a51/perf_governor.hpp"

using namespace a51;

//------------------------------------------------------------------------------

static void feed( PerfGovernor& G, f32 FrameMs, int Frames, f32 CpuMs = -1.0f )
{
    for( int i = 0; i < Frames; i++ )
        G.PushFrame( FrameMs, ( CpuMs < 0.0f ) ? FrameMs * 0.5f : CpuMs );
}

//==============================================================================

TEST( perf_defaults_are_sane )
{
    PerfConfig C;
    CHECK_NEAR( C.TargetFps, 60.0, 0.001 );
    CHECK( C.MinScale < C.MaxScale );
    CHECK( C.QualityLevels >= 2 );

    PerfGovernor G;
    G.Configure( C );
    CHECK_NEAR( G.GetRenderScale(), 1.0, 0.001 );
    CHECK_EQ_INT( G.GetQualityLevel(), C.QualityLevels - 1 );
    CHECK( G.GetAutoQuality() );
}

//------------------------------------------------------------------------------

TEST( perf_config_is_clamped )
{
    PerfGovernor G;

    PerfConfig Bad;
    Bad.TargetFps       = -5.0f;
    Bad.MinScale        = 5.0f;
    Bad.MaxScale        = 4.0f;
    Bad.ScaleStep       = 0.0f;
    Bad.DownFrames      = 0;
    Bad.UpFrames        = 0;
    Bad.CooldownFrames  = -3;
    Bad.QualityLevels   = 0;
    Bad.TextureBudgetMb = 1;
    G.Configure( Bad );

    const PerfConfig& C = G.GetConfig();
    CHECK( C.TargetFps      >= 1.0f );
    CHECK( C.MinScale       >= 0.20f );
    CHECK( C.MaxScale       <= 1.0f );
    CHECK( C.MinScale       <= C.MaxScale );
    CHECK( C.ScaleStep      >= 0.01f );
    CHECK( C.DownFrames     >= 1 );
    CHECK( C.UpFrames       >= 1 );
    CHECK( C.CooldownFrames >= 0 );
    CHECK( C.QualityLevels  >= 1 );
    CHECK( C.TextureBudgetMb >= 16 );

    G.SetTargetFps( 10000.0f );
    CHECK( G.GetConfig().TargetFps <= 240.0f );
    G.SetTargetFps( 0.001f );
    CHECK( G.GetConfig().TargetFps >= 15.0f );
}

//------------------------------------------------------------------------------

TEST( perf_steady_load_does_not_change_quality )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    // Exactly on budget: 60 fps target, 16.6 ms frames.
    feed( G, 1000.0f / 60.0f, 600 );

    CHECK_NEAR( G.GetRenderScale(), 1.0, 0.001 );
    CHECK_EQ_INT( G.GetQualityLevel(), C.QualityLevels - 1 );
    CHECK_EQ_INT( G.GetQualityDrops(),  0 );
    CHECK_EQ_INT( G.GetQualityRaises(), 0 );
    CHECK_NEAR( G.GetFps(), 60.0, 1.0 );
    CHECK( G.GetStability() > 0.9 );
    CHECK_EQ_INT( G.GetSuggestedFpsCap(), 60 );
}

//------------------------------------------------------------------------------

TEST( perf_backs_off_under_load_and_converges )
{
    PerfConfig C;
    C.DownFrames = 4;
    C.CooldownFrames = 2;
    PerfGovernor G;
    G.Configure( C );

    // 30 fps worth of work against a 60 fps target.
    feed( G, 33.3f, 2000 );

    // It must have walked all the way down: lowest scale and lowest quality.
    CHECK_NEAR( G.GetRenderScale(), C.MinScale, 0.001 );
    CHECK_EQ_INT( G.GetQualityLevel(), 0 );
    CHECK( G.GetQualityDrops() > 0 );
    CHECK( G.GetRenderScale() >= C.MinScale - 0.0001f );
    CHECK( G.GetRenderScale() <= C.MaxScale + 0.0001f );

    // And it must have stopped flapping once it hit the floor.
    u32 DropsAtFloor = G.GetQualityDrops();
    feed( G, 33.3f, 500 );
    CHECK_EQ_INT( (int)G.GetQualityDrops(), (int)DropsAtFloor );

    // Still hopeless at the floor -> recommend 30 fps.
    CHECK_EQ_INT( G.GetSuggestedFpsCap(), 30 );
}

//------------------------------------------------------------------------------

TEST( perf_recovers_when_load_drops )
{
    PerfConfig C;
    C.DownFrames     = 4;
    C.UpFrames       = 30;
    C.CooldownFrames = 2;
    PerfGovernor G;
    G.Configure( C );

    feed( G, 40.0f, 1500 );
    CHECK_NEAR( G.GetRenderScale(), C.MinScale, 0.001 );
    CHECK_EQ_INT( G.GetQualityLevel(), 0 );

    // Load disappears.
    feed( G, 8.0f, 3000 );

    CHECK_NEAR( G.GetRenderScale(), C.MaxScale, 0.001 );
    CHECK_EQ_INT( G.GetQualityLevel(), C.QualityLevels - 1 );
    CHECK( G.GetQualityRaises() > 0 );
    CHECK_EQ_INT( G.GetSuggestedFpsCap(), 60 );
}

//------------------------------------------------------------------------------

TEST( perf_scale_never_leaves_its_bounds )
{
    PerfConfig C;
    C.DownFrames = 1;
    C.UpFrames   = 1;
    C.CooldownFrames = 0;
    PerfGovernor G;
    G.Configure( C );

    // Alternating good/bad frames is the worst case for oscillation.
    for( int i = 0; i < 4000; i++ )
        G.PushFrame( ( i & 1 ) ? 5.0f : 60.0f, 3.0f );

    CHECK( G.GetRenderScale() >= C.MinScale - 0.0001f );
    CHECK( G.GetRenderScale() <= C.MaxScale + 0.0001f );
    CHECK( G.GetQualityLevel() >= 0 );
    CHECK( G.GetQualityLevel() <  C.QualityLevels );
}

//------------------------------------------------------------------------------

TEST( perf_respects_a_fixed_scale_lock )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    G.SetFixedScale( 0.75f );
    CHECK_NEAR( G.GetRenderScale(), 0.75f, 0.001 );

    feed( G, 50.0f, 1000 );
    CHECK_NEAR( G.GetRenderScale(), 0.75f, 0.001 );
    CHECK_EQ_INT( G.GetQualityDrops(), 0 );

    // Unlocking lets it work again.
    G.SetFixedScale( 0.0f );
    feed( G, 50.0f, 1000 );
    CHECK( G.GetRenderScale() < 0.75f );

    // Out of range locks are clamped.
    G.SetFixedScale( 9.0f );
    CHECK( G.GetRenderScale() <= C.MaxScale + 0.0001f );
    G.SetFixedScale( -1.0f );
    CHECK( G.GetAutoQuality() );
}

//------------------------------------------------------------------------------

TEST( perf_quality_ceiling_is_honoured )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    G.SetQualityCeiling( 1 );
    CHECK_EQ_INT( G.GetQualityLevel(), 1 );

    feed( G, 4.0f, 3000 );
    CHECK_EQ_INT( G.GetQualityLevel(), 1 );

    G.SetQualityCeiling( 99 );
    CHECK_EQ_INT( G.GetQualityLevel(), 1 );       // stays where it was
}

//------------------------------------------------------------------------------

TEST( perf_thermal_throttling )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    // SEVERE: cap the scale and the quality, and slow the target down.
    G.SetThermalStatus( 3 );
    feed( G, 8.0f, 1000 );
    CHECK( G.GetRenderScale()  <= 0.65f + 0.001f );
    CHECK( G.GetQualityLevel() <= 1 );
    CHECK( G.GetSuggestedFpsCap() <= 45 );

    // CRITICAL: everything to the floor.
    G.SetThermalStatus( 4 );
    feed( G, 8.0f, 1000 );
    CHECK( G.GetRenderScale()  <= 0.50f + 0.001f );
    CHECK_EQ_INT( G.GetQualityLevel(), 0 );
    CHECK_EQ_INT( G.GetSuggestedFpsCap(), 30 );

    // Cooling down must allow recovery.
    G.SetThermalStatus( 0 );
    feed( G, 8.0f, 5000 );
    CHECK( G.GetRenderScale() > 0.50f );
}

//------------------------------------------------------------------------------

TEST( perf_memory_pressure_shrinks_the_cache )
{
    PerfConfig C;
    C.TextureBudgetMb = 256;
    PerfGovernor G;
    G.Configure( C );

    CHECK_EQ_INT( G.GetTextureBudgetMb(), 256 );

    G.SetMemoryPressure( 1.0f );
    s32 Squeezed = G.GetTextureBudgetMb();
    CHECK( Squeezed < 256 );
    CHECK( Squeezed >= 32 );

    G.SetMemoryPressure( 5.0f );      // nonsense input is clamped
    CHECK( G.GetTextureBudgetMb() >= 32 );
}

//------------------------------------------------------------------------------

TEST( perf_statistics_are_reasonable )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    feed( G, 20.0f, 100 );

    CHECK_EQ_INT( (int)G.GetFrameCount(), 100 );
    CHECK_NEAR( G.GetAvgFrameMs(), 20.0, 0.5 );
    CHECK_NEAR( G.GetFrameTimeMs(), 20.0, 1.0 );
    CHECK_NEAR( G.GetFrameTimeP95(), 20.0, 0.5 );
    CHECK_NEAR( G.GetWorstFrameMs(), 20.0, 0.001 );
    CHECK( G.GetDroppedFrames() == 0 );       // 20 ms < 1.5 * 16.6 ms

    G.PushFrame( 200.0f, 100.0f );            // a real hitch
    CHECK( G.GetDroppedFrames() == 1 );
    CHECK_NEAR( G.GetWorstFrameMs(), 200.0, 0.001 );

    // Junk input must not poison the averages.
    G.PushFrame( -50.0f, -10.0f );
    CHECK( G.GetFrameTimeMs() > 0.0f );

    G.Reset();
    CHECK_EQ_INT( (int)G.GetFrameCount(), 0 );
    CHECK_EQ_INT( (int)G.GetDroppedFrames(), 0 );
}

//------------------------------------------------------------------------------

TEST( perf_stability_reports_jitter )
{
    PerfConfig C;
    PerfGovernor G;
    G.Configure( C );

    // Rock solid.
    feed( G, 16.6f, 200 );
    f32 Solid = G.GetStability();

    G.Reset();

    // Wildly inconsistent.
    for( int i = 0; i < 200; i++ )
        G.PushFrame( ( i & 1 ) ? 6.0f : 40.0f, 3.0f );
    f32 Wild = G.GetStability();

    CHECK( Solid > 0.9 );
    CHECK( Wild  < Solid );
    CHECK( Wild >= 0.0f && Wild <= 1.0f );
}

//==============================================================================
//  Frame pacer
//==============================================================================

TEST( pacer_holds_the_target_frame_rate )
{
    FramePacer P;
    P.SetTargetFps( 60.0f );
    CHECK_NEAR( P.GetBudgetMs(), 1000.0 / 60.0, 0.01 );

    // A frame that costs 6 ms should sleep for about 10.6 ms.
    f32 Start = 0.0f;
    f32 Sleep = P.SleepMs( Start, Start + 6.0f );
    CHECK_NEAR( Sleep, 1000.0 / 60.0 - 6.0, 0.2 );

    // Simulate a whole second of perfect frames: the deadline must not drift.
    P.Reset();
    f32 Now = 0.0f;
    int Frames = 0;
    while( Now < 1000.0f )
    {
        f32 FrameStart = Now;
        Now += 6.0f;                                   // work
        f32 Wait = P.SleepMs( FrameStart, Now );
        Now += Wait;                                   // sleep
        Frames++;
        if( Frames > 200 ) break;
    }
    CHECK( Frames >= 58 && Frames <= 61 );

    // Over budget: no sleep at all, and the miss is reported.
    P.Reset();
    CHECK_NEAR( P.SleepMs( 0.0f, 30.0f ), 0.0, 0.001 );
    CHECK( P.MissedDeadline() );

    // A huge hitch must resync instead of scheduling a burst of catch-up frames.
    P.Reset();
    P.SleepMs( 0.0f, 5.0f );
    f32 After = P.SleepMs( 5000.0f, 5010.0f );
    CHECK_NEAR( After, 0.0, 0.001 );
    CHECK( P.MissedDeadline() );

    // ... and the very next frame is paced normally again, not doubled.
    f32 Next = P.SleepMs( 5010.0f, 5016.0f );
    CHECK_NEAR( Next, 1000.0 / 60.0 - 6.0, 0.5 );

    // Disabled pacer never sleeps.
    P.SetEnabled( false );
    CHECK_NEAR( P.SleepMs( 0.0f, 1.0f ), 0.0, 0.001 );
}

//==============================================================================

int main( void )
{
    std::printf( "Running performance governor tests\n\n" );
    return ::test::run_all();
}
//==============================================================================
