//==============================================================================
//
//  test_touch.cpp
//
//  Touch controls: the default layout, hit testing, joystick maths, button
//  edges, multi-touch, the layout editor operations and the save file format.
//
//==============================================================================

#include "test_framework.hpp"

#include "a51/touch_layout.hpp"
#include "a51/input_state.hpp"
#include "a51/json.hpp"

#include <cstring>
#include <cstdio>

using namespace a51;

static const f32 ASPECT = 16.0f / 9.0f;

//==============================================================================
//  Layout
//==============================================================================

TEST( touch_default_layout_is_complete )
{
    TouchLayout L;
    CHECK( L.Count() > 8 );

    // Everything the game needs must be there, and wired to a real gadget.
    const char* pRequired[] =
    {
        "STICK_MOVE", "BTN_FIRE", "BTN_AIM", "BTN_JUMP", "BTN_CROUCH",
        "BTN_USE", "BTN_RELOAD", "BTN_MELEE", "BTN_GRENADE", "BTN_PAUSE"
    };

    for( size_t i = 0; i < sizeof( pRequired ) / sizeof( pRequired[0] ); i++ )
    {
        s32 Index = L.IndexOfName( pRequired[i] );
        CHECK( Index >= 0 );
        if( Index < 0 ) continue;

        const TouchControl& C = L.Get( Index );
        s32 Gadget = gadget_id_by_name( C.Gadget );
        if( Gadget == INPUT_UNDEFINED )
        {
            char Msg[256];
            snprintf( Msg, sizeof( Msg ), "%s maps to unknown gadget '%s'", pRequired[i], C.Gadget );
            ::test::fail( __FILE__, __LINE__, Msg );
        }
        CHECK( C.Size  > 0.04f );
        CHECK( C.X     >= 0.0f && C.X <= 1.0f );
        CHECK( C.Y     >= 0.0f && C.Y <= 1.0f );
        CHECK( C.Opacity >= 0.1f && C.Opacity <= 1.0f );
    }

    // The stick needs both axes.
    s32 Stick = L.IndexOfName( "STICK_MOVE" );
    CHECK( Stick >= 0 );
    CHECK_EQ_INT( L.Get( Stick ).Kind, TOUCH_STICK );
    CHECK( gadget_id_by_name( L.Get( Stick ).Gadget  ) == INPUT_XBOX_STICK_LEFT_X );
    CHECK( gadget_id_by_name( L.Get( Stick ).Gadget2 ) == INPUT_XBOX_STICK_LEFT_Y );
}

//------------------------------------------------------------------------------

TEST( touch_hit_testing )
{
    TouchLayout L;

    s32 Fire = L.IndexOfName( "BTN_FIRE" );
    CHECK( Fire >= 0 );
    const TouchControl& C = L.Get( Fire );

    // Dead centre of the button.
    CHECK_EQ_INT( L.HitTest( C.X * ASPECT, C.Y, ASPECT ), Fire );

    // Just inside / just outside the rim.
    f32 R = L.ControlRadius( Fire );
    CHECK_EQ_INT( L.HitTest( C.X * ASPECT + R * 0.9f, C.Y, ASPECT ), Fire );

    // Somewhere with nothing on it.
    CHECK_EQ_INT( L.HitTest( 0.5f * ASPECT, 0.25f, ASPECT ), -1 );

    // Invisible controls are not touchable.
    L.SetVisible( C.Id, false );
    CHECK_EQ_INT( L.HitTest( C.X * ASPECT, C.Y, ASPECT ), -1 );
    L.SetVisible( C.Id, true );
    CHECK_EQ_INT( L.HitTest( C.X * ASPECT, C.Y, ASPECT ), Fire );

    // The resize handle sits on the rim and is only found by its own test.
    f32 hx, hy;
    L.HandlePos( Fire, ASPECT, hx, hy );
    CHECK_EQ_INT( L.HitTestHandle( hx, hy, ASPECT ), Fire );
    CHECK_EQ_INT( L.HitTestHandle( 0.5f * ASPECT, 0.25f, ASPECT ), -1 );
}

//------------------------------------------------------------------------------

TEST( touch_layout_editor_operations )
{
    TouchLayout L;
    s32 Fire = L.IndexOfName( "BTN_FIRE" );
    s32 Id   = L.Get( Fire ).Id;

    // Move.
    L.Move( Id, 0.40f, 0.30f, ASPECT );
    CHECK_NEAR( L.Get( Fire ).X, 0.40f, 0.001 );
    CHECK_NEAR( L.Get( Fire ).Y, 0.30f, 0.001 );

    // Moving off screen clamps instead of losing the button.
    L.Move( Id, 5.0f, 5.0f, ASPECT );
    CHECK( L.Get( Fire ).X <= 1.0f );
    CHECK( L.Get( Fire ).Y <= 1.0f );
    L.Move( Id, -5.0f, -5.0f, ASPECT );
    CHECK( L.Get( Fire ).X >= 0.0f );
    CHECK( L.Get( Fire ).Y >= 0.0f );
    // ... and it is still reachable afterwards.
    CHECK_EQ_INT( L.HitTest( L.Get( Fire ).X * ASPECT, L.Get( Fire ).Y, ASPECT ), Fire );

    // Size.
    L.SetSize( Id, 0.25f );
    CHECK_NEAR( L.Get( Fire ).Size, 0.25f, 0.001 );
    L.SetSize( Id, 10.0f );
    CHECK_NEAR( L.Get( Fire ).Size, 0.60f, 0.001 );
    L.SetSize( Id, -1.0f );
    CHECK_NEAR( L.Get( Fire ).Size, 0.05f, 0.001 );

    // Opacity.
    L.SetOpacity( Id, 0.42f );
    CHECK_NEAR( L.Get( Fire ).Opacity, 0.42f, 0.001 );
    L.SetOpacity( Id, 5.0f );
    CHECK_NEAR( L.Get( Fire ).Opacity, 1.0f, 0.001 );

    // Style.
    L.SetStyle( Id, TOUCH_STYLE_SQUARE );
    CHECK_EQ_INT( L.Get( Fire ).Style, TOUCH_STYLE_SQUARE );

    // Visibility for everything.
    L.ShowAll( false );
    for( s32 i = 0; i < L.Count(); i++ ) CHECK( !L.Get( i ).Visible );
    L.ShowAll( true );
    for( s32 i = 0; i < L.Count(); i++ ) CHECK( L.Get( i ).Visible );

    // Reset restores the stock values.
    L.ResetControl( Id );
    CHECK_NEAR( L.Get( Fire ).X, 0.88f, 0.001 );
    CHECK_EQ_INT( L.Get( Fire ).Style, TOUCH_STYLE_CIRCLE );

    // Unknown ids are ignored, not fatal.
    L.Move( 99999, 0.5f, 0.5f, ASPECT );
    L.SetSize( 99999, 0.5f );
    L.ResetControl( 99999 );
    CHECK_EQ_INT( L.IndexOfId( 99999 ), -1 );
}

//------------------------------------------------------------------------------

TEST( touch_layout_clamps_to_screen )
{
    TouchLayout L;
    // A layout authored on a 16:9 phone, opened on a 4:3 tablet.
    L.ClampToScreen( 4.0f / 3.0f );
    for( s32 i = 0; i < L.Count(); i++ )
    {
        const TouchControl& C = L.Get( i );
        CHECK( C.X >= 0.0f && C.X <= 1.0f );
        CHECK( C.Y >= 0.0f && C.Y <= 1.0f );
    }

    // Nonsense aspect ratios must not produce NaNs.
    L.ClampToScreen( 0.0f );
    for( s32 i = 0; i < L.Count(); i++ )
    {
        const TouchControl& C = L.Get( i );
        CHECK( C.X == C.X );        // not NaN
        CHECK( C.Y == C.Y );
    }
}

//==============================================================================
//  Runtime
//==============================================================================

TEST( touch_stick_mathematics )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );
    R.SetDeadZone( 0.15f );

    s32 Stick = L.IndexOfName( "STICK_MOVE" );
    const TouchControl& C = L.Get( Stick );
    f32 cx = C.X * ASPECT;
    f32 cy = C.Y;
    f32 Radius = L.ControlRadius( Stick );

    // Full deflection to the right.
    In.BeginFrame();
    CHECK( R.OnTouch( A51_ACTION_DOWN, 0, cx, cy, ASPECT, In ) );
    CHECK( R.OnTouch( A51_ACTION_MOVE, 0, cx + Radius, cy, ASPECT, In ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ),  1.0, 0.001 );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_Y ),  0.0, 0.001 );

    // Half way, with the dead zone taken out.
    CHECK( R.OnTouch( A51_ACTION_MOVE, 0, cx + Radius * 0.5f, cy, ASPECT, In ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), ( 0.5 - 0.15 ) / 0.85, 0.001 );

    // Inside the dead zone -> nothing.
    CHECK( R.OnTouch( A51_ACTION_MOVE, 0, cx + Radius * 0.10f, cy, ASPECT, In ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 0.0, 0.001 );

    // Up on screen is positive Y for the engine.
    CHECK( R.OnTouch( A51_ACTION_MOVE, 0, cx, cy - Radius, ASPECT, In ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_Y ),  1.0, 0.001 );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ),  0.0, 0.001 );

    // Over-deflection is clamped to the unit circle.
    CHECK( R.OnTouch( A51_ACTION_MOVE, 0, cx + Radius * 3.0f, cy - Radius * 3.0f, ASPECT, In ) );
    f32 vx = In.GetValue( INPUT_XBOX_STICK_LEFT_X );
    f32 vy = In.GetValue( INPUT_XBOX_STICK_LEFT_Y );
    CHECK_NEAR( vx * vx + vy * vy, 1.0, 0.001 );

    // The knob follows the finger but stays inside the base.
    f32 kx = 0.0f, ky = 0.0f;
    CHECK( R.GetStickKnob( Stick, kx, ky ) );
    f32 ddx = kx - cx, ddy = ky - cy;
    CHECK( sqrtf( ddx * ddx + ddy * ddy ) <= Radius + 0.001f );

    // Letting go centres everything.
    CHECK( R.OnTouch( A51_ACTION_UP, 0, cx + Radius, cy, ASPECT, In ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 0.0, 0.001 );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_Y ), 0.0, 0.001 );
    CHECK( !R.GetStickKnob( Stick, kx, ky ) );
}

//------------------------------------------------------------------------------

TEST( touch_button_edges )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );

    s32 Jump = L.IndexOfName( "BTN_JUMP" );
    const TouchControl& C = L.Get( Jump );
    f32 cx = C.X * ASPECT, cy = C.Y;

    // Press.
    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, cx, cy, ASPECT, In );
    CHECK( In.IsPressed ( INPUT_XBOX_BTN_A ) );
    CHECK( In.WasPressed( INPUT_XBOX_BTN_A ) );

    // Held: still pressed, but no new edge.
    In.BeginFrame();
    CHECK(  In.IsPressed ( INPUT_XBOX_BTN_A ) );
    CHECK( !In.WasPressed( INPUT_XBOX_BTN_A ) );

    // Sliding off the button must not drop the press mid-shot.
    R.OnTouch( A51_ACTION_MOVE, 0, cx + 1.0f, cy - 1.0f, ASPECT, In );
    CHECK( In.IsPressed( INPUT_XBOX_BTN_A ) );

    // Release.
    R.OnTouch( A51_ACTION_UP, 0, cx, cy, ASPECT, In );
    CHECK( !In.IsPressed  ( INPUT_XBOX_BTN_A ) );
    CHECK(  In.WasReleased( INPUT_XBOX_BTN_A ) );

    // A cancel behaves like a release.
    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, cx, cy, ASPECT, In );
    CHECK( In.IsPressed( INPUT_XBOX_BTN_A ) );
    In.BeginFrame();
    R.OnTouch( A51_ACTION_CANCEL, 0, cx, cy, ASPECT, In );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_A ) );

    // Analogue buttons (the triggers) report a full scale value.
    s32 Fire = L.IndexOfName( "BTN_FIRE" );
    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, L.Get( Fire ).X * ASPECT, L.Get( Fire ).Y, ASPECT, In );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_R_TRIGGER ), 1.0, 0.001 );
    R.OnTouch( A51_ACTION_UP, 0, L.Get( Fire ).X * ASPECT, L.Get( Fire ).Y, ASPECT, In );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_R_TRIGGER ), 0.0, 0.001 );
}

//------------------------------------------------------------------------------

TEST( touch_dpad )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );

    s32 Pad = L.IndexOfName( "DPAD_WEAPON" );
    CHECK( Pad >= 0 );
    L.SetVisible( L.Get( Pad ).Id, true );      // hidden by default

    const TouchControl& C = L.Get( Pad );
    f32 cx = C.X * ASPECT, cy = C.Y;
    f32 Radius = L.ControlRadius( Pad );

    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, cx, cy, ASPECT, In );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_LEFT ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_RIGHT ) );

    // Slide right.
    R.OnTouch( A51_ACTION_MOVE, 0, cx + Radius * 0.7f, cy, ASPECT, In );
    CHECK(  In.IsPressed( INPUT_XBOX_BTN_RIGHT ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_LEFT  ) );

    // Slide left.
    R.OnTouch( A51_ACTION_MOVE, 0, cx - Radius * 0.7f, cy, ASPECT, In );
    CHECK(  In.IsPressed( INPUT_XBOX_BTN_LEFT  ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_RIGHT ) );

    R.OnTouch( A51_ACTION_UP, 0, cx, cy, ASPECT, In );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_LEFT  ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_RIGHT ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_UP    ) );
    CHECK( !In.IsPressed( INPUT_XBOX_BTN_DOWN  ) );
}

//------------------------------------------------------------------------------

TEST( touch_look_drag )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );
    R.SetLookSensitivity( 1.0f );

    f32 x = 0.5f * ASPECT, y = 0.25f;
    CHECK_EQ_INT( L.HitTest( x, y, ASPECT ), -1 );      // empty area

    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, x, y, ASPECT, In );
    R.OnTouch( A51_ACTION_MOVE, 0, x + 20.0f, y, ASPECT, In );
    CHECK( In.GetValue( INPUT_MOUSE_X_REL ) > 0.0f );

    In.EndFrame();
    CHECK_NEAR( In.GetValue( INPUT_MOUSE_X_REL ), 0.0, 0.001 );

    // Inverted look flips the vertical axis.
    In.BeginFrame();
    R.OnTouch( A51_ACTION_MOVE, 0, x + 20.0f, y + 10.0f, ASPECT, In );
    f32 Normal = In.GetValue( INPUT_MOUSE_Y_REL );
    R.SetInvertLookY( true );
    In.EndFrame();
    In.BeginFrame();
    R.OnTouch( A51_ACTION_MOVE, 0, x + 20.0f, y + 20.0f, ASPECT, In );
    f32 Inverted = In.GetValue( INPUT_MOUSE_Y_REL );
    CHECK( Normal * Inverted < 0.0f );

    // Look can be turned off on its own.
    R.SetLookEnabled( false );
    In.EndFrame();
    In.BeginFrame();
    R.OnTouch( A51_ACTION_MOVE, 0, x + 60.0f, y, ASPECT, In );
    CHECK_NEAR( In.GetValue( INPUT_MOUSE_X_REL ), 0.0, 0.001 );
    R.OnTouch( A51_ACTION_UP, 0, x, y, ASPECT, In );
}

//------------------------------------------------------------------------------

TEST( touch_multi_touch )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );

    s32 Fire  = L.IndexOfName( "BTN_FIRE" );
    s32 Stick = L.IndexOfName( "STICK_MOVE" );

    f32 fx = L.Get( Fire  ).X * ASPECT, fy = L.Get( Fire  ).Y;
    f32 sx = L.Get( Stick ).X * ASPECT, sy = L.Get( Stick ).Y;
    f32 sr = L.ControlRadius( Stick );

    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN,         0, fx, fy, ASPECT, In );      // finger 0 on fire
    R.OnTouch( A51_ACTION_POINTER_DOWN, 1, sx, sy, ASPECT, In );      // finger 1 on the stick
    R.OnTouch( A51_ACTION_MOVE,         1, sx + sr, sy, ASPECT, In );

    CHECK( !In.IsPressed( INPUT_XBOX_BTN_A ) );
    CHECK(  In.IsPressed( INPUT_XBOX_R_TRIGGER ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 1.0, 0.001 );
    CHECK( R.IsControlActive( Fire ) );
    CHECK( R.IsControlActive( Stick ) );

    // Lifting the firing finger must not disturb the stick.
    R.OnTouch( A51_ACTION_POINTER_UP, 0, fx, fy, ASPECT, In );
    CHECK( !In.IsPressed( INPUT_XBOX_R_TRIGGER ) );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 1.0, 0.001 );
    CHECK( !R.IsControlActive( Fire ) );

    R.OnTouch( A51_ACTION_UP, 1, sx, sy, ASPECT, In );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 0.0, 0.001 );

    // Releasing an unknown pointer is harmless.
    R.OnTouch( A51_ACTION_UP, 42, 0.1f, 0.1f, ASPECT, In );
    R.OnTouch( A51_ACTION_MOVE, 43, 0.1f, 0.1f, ASPECT, In );
}

//------------------------------------------------------------------------------

TEST( touch_master_switch_disables_everything )
{
    TouchLayout  L;
    TouchRuntime R;
    InputState   In;
    R.SetLayout( &L );

    s32 Fire = L.IndexOfName( "BTN_FIRE" );
    f32 fx = L.Get( Fire ).X * ASPECT, fy = L.Get( Fire ).Y;

    // Press it, then switch the overlay off mid-press: the gadget must not stay
    // latched, otherwise the player would fire forever.
    In.BeginFrame();
    R.OnTouch( A51_ACTION_DOWN, 0, fx, fy, ASPECT, In );
    CHECK( In.IsPressed( INPUT_XBOX_R_TRIGGER ) );

    R.SetEnabled( false, &In );
    In.BeginFrame();
    CHECK( !In.IsPressed( INPUT_XBOX_R_TRIGGER ) );
    CHECK( !R.OnTouch( A51_ACTION_DOWN, 0, fx, fy, ASPECT, In ) );
    CHECK( !R.OnTouch( A51_ACTION_MOVE, 0, fx, fy, ASPECT, In ) );
    CHECK( !R.OnTouch( A51_ACTION_UP,   0, fx, fy, ASPECT, In ) );

    // Turning it off without handing over the input state must at least forget
    // the fingers it was tracking.
    R.SetEnabled( true );
    In.BeginFrame();
    CHECK( R.OnTouch( A51_ACTION_DOWN, 0, fx, fy, ASPECT, In ) );
    CHECK( R.IsControlActive( Fire ) );
    R.SetEnabled( false );
    CHECK( !R.IsControlActive( Fire ) );
    CHECK( !R.OnTouch( A51_ACTION_MOVE, 0, fx, fy, ASPECT, In ) );

    // And back on.
    R.SetEnabled( true );
    In.BeginFrame();
    CHECK( R.OnTouch( A51_ACTION_DOWN, 0, fx, fy, ASPECT, In ) );
    CHECK( In.IsPressed( INPUT_XBOX_R_TRIGGER ) );
    R.OnTouch( A51_ACTION_UP, 0, fx, fy, ASPECT, In );
}

//------------------------------------------------------------------------------

TEST( touch_runtime_without_a_layout_is_safe )
{
    TouchRuntime R;
    InputState   In;

    CHECK( !R.OnTouch( A51_ACTION_DOWN, 0, 0.5f, 0.5f, ASPECT, In ) );
    CHECK( !R.OnTouch( A51_ACTION_MOVE, 0, 0.5f, 0.5f, ASPECT, In ) );
    CHECK( !R.OnTouch( A51_ACTION_UP,   0, 0.5f, 0.5f, ASPECT, In ) );
    R.Update( In );
    R.Reset();

    f32 x, y;
    CHECK( !R.GetStickKnob( 0, x, y ) );
    CHECK( !R.IsControlActive( 0 ) );
}

//==============================================================================
//  Save file
//==============================================================================

TEST( touch_layout_round_trips_through_json )
{
    TouchLayout L;

    s32 Fire  = L.IndexOfName( "BTN_FIRE" );
    s32 Pause = L.IndexOfName( "BTN_PAUSE" );
    s32 Id    = L.Get( Fire ).Id;

    L.Move( Id, 0.77f, 0.66f, ASPECT );
    L.SetSize( Id, 0.22f );
    L.SetOpacity( Id, 0.33f );
    L.SetVisible( Id, false );
    L.SetStyle( Id, TOUCH_STYLE_SQUARE );
    L.SetVisible( L.Get( Pause ).Id, false );

    char Buffer[ 16384 ];
    CHECK( L.ToJson( Buffer, sizeof( Buffer ) ) );
    CHECK( strlen( Buffer ) > 100 );
    CHECK( strstr( Buffer, "BTN_FIRE" ) != nullptr );

    TouchLayout Loaded;
    CHECK( Loaded.FromJson( Buffer ) );
    CHECK_EQ_INT( Loaded.Count(), L.Count() );

    s32 FireLoaded = Loaded.IndexOfName( "BTN_FIRE" );
    CHECK( FireLoaded >= 0 );
    CHECK_NEAR( Loaded.Get( FireLoaded ).X,       0.77f, 0.001 );
    CHECK_NEAR( Loaded.Get( FireLoaded ).Y,       0.66f, 0.001 );
    CHECK_NEAR( Loaded.Get( FireLoaded ).Size,    0.22f, 0.001 );
    CHECK_NEAR( Loaded.Get( FireLoaded ).Opacity, 0.33f, 0.001 );
    CHECK( !Loaded.Get( FireLoaded ).Visible );
    CHECK_EQ_INT( Loaded.Get( FireLoaded ).Style, TOUCH_STYLE_SQUARE );
    CHECK( !Loaded.Get( Loaded.IndexOfName( "BTN_PAUSE" ) ).Visible );

    // Every control survived with its gadget intact.
    for( s32 i = 0; i < Loaded.Count(); i++ )
        CHECK( gadget_id_by_name( Loaded.Get( i ).Gadget ) != INPUT_UNDEFINED );
}

//------------------------------------------------------------------------------

TEST( touch_layout_survives_garbage_files )
{
    TouchLayout L;
    CHECK( !L.FromJson( nullptr ) );
    CHECK( !L.FromJson( "" ) );
    CHECK( !L.FromJson( "not json" ) );
    CHECK( !L.FromJson( "{\"controls\": 5}" ) );
    CHECK( L.Count() > 0 );       // falls back to the stock layout

    // A file from an older version: unknown fields are ignored, missing ones
    // keep their defaults.
    const char* pOld =
        "{ \"format\": 1, \"controls\": [ { \"id\": 2, \"x\": 0.5, \"y\": 0.5 } ] }";
    CHECK( L.FromJson( pOld ) );
    s32 Fire = L.IndexOfName( "BTN_FIRE" );
    CHECK( Fire >= 0 );
    CHECK_NEAR( L.Get( Fire ).X, 0.5, 0.001 );
    CHECK( L.Get( Fire ).Size > 0.0f );
}

//------------------------------------------------------------------------------

TEST( touch_layout_buffer_overflow_is_reported )
{
    TouchLayout L;
    char Tiny[ 64 ];
    CHECK( !L.ToJson( Tiny, sizeof( Tiny ) ) );
    CHECK( !L.ToJson( nullptr, 0 ) );
}

//==============================================================================

int main( void )
{
    std::printf( "Running touch control tests\n\n" );
    return ::test::run_all();
}
//==============================================================================
