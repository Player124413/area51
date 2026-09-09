//==============================================================================
//
//  test_json.cpp
//
//  The config file reader/writer used for touch layouts and profiles.
//
//==============================================================================

#include "test_framework.hpp"

#include "a51/json.hpp"
#include "a51/input_state.hpp"

#include <cstring>
#include <cstdio>

using namespace a51;

//==============================================================================

TEST( json_parses_a_layout_like_document )
{
    const char* pText =
        "{\n"
        "  \"format\": 1,\n"
        "  \"name\": \"Area 51\",\n"
        "  \"enabled\": true,\n"
        "  \"scale\": 0.75,\n"
        "  \"nothing\": null,\n"
        "  \"controls\": [\n"
        "     { \"id\": 1, \"label\": \"FIRE\", \"x\": 0.88, \"visible\": true },\n"
        "     { \"id\": 2, \"label\": \"JUMP\", \"x\": -0.5, \"visible\": false }\n"
        "  ]\n"
        "}";

    Json J;
    CHECK( J.Parse( pText ) );
    if( J.Root() < 0 ) { ::test::fail( __FILE__, __LINE__, J.GetError() ); return; }

    CHECK_EQ_INT( J.GetInt ( J.Root(), "format",  -1 ), 1 );
    CHECK_STR   (          J.GetStr ( J.Root(), "name",   "" ), "Area 51" );
    CHECK       (          J.GetBool( J.Root(), "enabled", false ) );
    CHECK_NEAR  (          J.GetNum ( J.Root(), "scale",  0.0 ), 0.75, 0.0001 );

    // Defaults for missing keys.
    CHECK_EQ_INT( J.GetInt( J.Root(), "missing", 42 ), 42 );
    CHECK_STR   (          J.GetStr ( J.Root(), "missing", "dflt" ), "dflt" );
    CHECK       ( !J.GetBool( J.Root(), "missing", false ) );
    CHECK_NEAR  (          J.GetNum ( J.Root(), "missing", 1.5 ), 1.5, 0.0001 );

    // null is not a string or a number.
    CHECK_STR( J.GetStr( J.Root(), "nothing", "was null" ), "was null" );

    s32 Controls = J.Find( J.Root(), "controls" );
    CHECK( Controls >= 0 );
    CHECK_EQ_INT( J.Count( Controls ), 2 );

    s32 First = J.At( Controls, 0 );
    s32 Second= J.At( Controls, 1 );
    CHECK( First >= 0 && Second >= 0 );

    CHECK_EQ_INT( J.GetInt( First, "id", -1 ), 1 );
    CHECK_STR   (          J.GetStr( First, "label", "" ), "FIRE" );
    CHECK_NEAR  (          J.GetNum( First, "x", 0.0 ), 0.88, 0.0001 );
    CHECK       (          J.GetBool( First, "visible", false ) );

    CHECK_EQ_INT( J.GetInt( Second, "id", -1 ), 2 );
    CHECK_NEAR  (          J.GetNum( Second, "x", 0.0 ), -0.5, 0.0001 );
    CHECK       ( !J.GetBool( Second, "visible", true ) );

    CHECK_EQ_INT( J.At( Controls, 99 ), -1 );
    CHECK_EQ_INT( J.Find( J.Root(), "nope" ), -1 );
}

//------------------------------------------------------------------------------

TEST( json_strings_are_unescaped )
{
    const char* pText =
        "{ \"a\": \"line\\nbreak\", \"b\": \"quo\\\"te\", \"c\": \"back\\\\slash\","
        "  \"d\": \"caf\\u00e9\", \"e\": \"tab\\there\", \"f\": \"\" }";

    Json J;
    CHECK( J.Parse( pText ) );

    char Buf[64];

    CHECK( J.CopyStr( J.Find( J.Root(), "a" ), Buf, sizeof( Buf ) ) > 0 );
    CHECK_STR( Buf, "line\nbreak" );

    CHECK( J.CopyStr( J.Find( J.Root(), "b" ), Buf, sizeof( Buf ) ) > 0 );
    CHECK_STR( Buf, "quo\"te" );

    CHECK( J.CopyStr( J.Find( J.Root(), "c" ), Buf, sizeof( Buf ) ) > 0 );
    CHECK_STR( Buf, "back\\slash" );

    // \u00e9 must come out as UTF-8 (0xC3 0xA9).
    CHECK( J.CopyStr( J.Find( J.Root(), "d" ), Buf, sizeof( Buf ) ) > 0 );
    CHECK_STR( Buf, "caf\xC3\xA9" );

    CHECK( J.CopyStr( J.Find( J.Root(), "e" ), Buf, sizeof( Buf ) ) > 0 );
    CHECK_STR( Buf, "tab\there" );

    CHECK_EQ_INT( J.CopyStr( J.Find( J.Root(), "f" ), Buf, sizeof( Buf ) ), 0 );
    CHECK_STR( Buf, "" );

    // A string containing an escape is not handed out as a raw pointer.
    CHECK_STR( J.GetStr( J.Root(), "a", "fallback" ), "fallback" );
    CHECK_STR( J.GetStr( J.Root(), "c", "fallback" ), "fallback" );

    // Truncation is safe.
    CHECK( J.CopyStr( J.Find( J.Root(), "a" ), Buf, 4 ) < 4 );
}

//------------------------------------------------------------------------------

TEST( json_rejects_malformed_input )
{
    const char* pBad[] =
    {
        nullptr,
        "",
        "   ",
        "{",
        "}",
        "[",
        "{ \"a\" }",
        "{ \"a\": }",
        "{ \"a\": 1, }",
        "{ a: 1 }",
        "{ \"a\": 1 } garbage",
        "[1, 2",
        "[1 2]",
        "\"unterminated",
        "{ \"a\": tru }",
        "{ \"a\": 0x10 }",
        "-",
    };

    for( size_t i = 0; i < sizeof( pBad ) / sizeof( pBad[0] ); i++ )
    {
        Json J;
        if( J.Parse( pBad[i] ) )
        {
            char Msg[128];
            snprintf( Msg, sizeof( Msg ), "accepted malformed input #%d", (int)i );
            ::test::fail( __FILE__, __LINE__, Msg );
        }
        CHECK( strlen( J.GetError() ) > 0 );
    }

    // Valid edge cases.
    Json J;
    CHECK( J.Parse( "{}" ) );
    CHECK_EQ_INT( J.Count( J.Root() ), 0 );
    CHECK( J.Parse( "[]" ) );
    CHECK_EQ_INT( J.Count( J.Root() ), 0 );
    CHECK( J.Parse( "  { \"a\" : [ ] }  " ) );
    CHECK_EQ_INT( J.Count( J.Find( J.Root(), "a" ) ), 0 );
    CHECK( J.Parse( "{\"a\":1e3}" ) );
    CHECK_NEAR( J.GetNum( J.Root(), "a", 0.0 ), 1000.0, 0.001 );
    CHECK( J.Parse( "{\"a\":-2.5e-2}" ) );
    CHECK_NEAR( J.GetNum( J.Root(), "a", 0.0 ), -0.025, 0.0001 );
}

//------------------------------------------------------------------------------

TEST( json_writer_round_trip )
{
    char Buffer[ 2048 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ObjectBegin();
    W.Pair( "name",  "STICK_MOVE" );
    W.Pair( "id",    7 );
    W.Pair( "x",     0.25 );
    W.Pair( "y",     0.75 );
    W.Pair( "shown", true );
    W.Key( "tags" );
    W.ArrayBegin();
    W.Str( "move" );
    W.Str( "analog" );
    W.ArrayEnd();
    W.ObjectEnd();

    CHECK( !W.Overflowed() );

    Json J;
    CHECK( J.Parse( Buffer ) );
    CHECK_STR( J.GetStr( J.Root(), "name", "" ), "STICK_MOVE" );
    CHECK_EQ_INT( J.GetInt( J.Root(), "id", 0 ), 7 );
    CHECK_NEAR( J.GetNum( J.Root(), "x", 0.0 ), 0.25, 0.0001 );
    CHECK( J.GetBool( J.Root(), "shown", false ) );

    s32 Tags = J.Find( J.Root(), "tags" );
    CHECK_EQ_INT( J.Count( Tags ), 2 );
    CHECK_STR( J.Str( J.At( Tags, 0 ), "" ), "move" );
    CHECK_STR( J.Str( J.At( Tags, 1 ), "" ), "analog" );
    CHECK_NEAR( J.Num( J.At( Tags, 0 ), 3.0 ), 3.0, 0.0001 );   // not a number
    CHECK( !J.Bool( J.At( Tags, 0 ), false ) );
}

//------------------------------------------------------------------------------

TEST( json_writer_escapes_and_reports_overflow )
{
    char Buffer[ 256 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ObjectBegin();
    W.Pair( "quote",  "say \"hi\"" );
    W.Pair( "nl",     "a\nb" );
    W.Pair( "slash",  "c:\\path" );
    W.ObjectEnd();
    CHECK( !W.Overflowed() );

    Json J;
    CHECK( J.Parse( Buffer ) );
    char Out[64];
    CHECK( J.CopyStr( J.Find( J.Root(), "quote" ), Out, sizeof( Out ) ) > 0 );
    CHECK_STR( Out, "say \"hi\"" );
    CHECK( J.CopyStr( J.Find( J.Root(), "nl" ), Out, sizeof( Out ) ) > 0 );
    CHECK_STR( Out, "a\nb" );
    CHECK( J.CopyStr( J.Find( J.Root(), "slash" ), Out, sizeof( Out ) ) > 0 );
    CHECK_STR( Out, "c:\\path" );

    // Too small a buffer must be reported, never overrun.
    char Tiny[ 16 ];
    JsonWriter W2( Tiny, sizeof( Tiny ) );
    W2.ObjectBegin();
    for( int i = 0; i < 50; i++ ) W2.Pair( "some fairly long key name", i );
    W2.ObjectEnd();
    CHECK( W2.Overflowed() );
}

//==============================================================================
//  Input state (small, but it is the contract between the touch layer and the
//  game, so it is pinned down here).
//==============================================================================

TEST( input_state_edges )
{
    InputState In;

    CHECK( !In.IsPressed( INPUT_XBOX_BTN_A ) );

    In.BeginFrame();
    In.SetDigital( INPUT_XBOX_BTN_A, true );
    CHECK(  In.IsPressed ( INPUT_XBOX_BTN_A ) );
    CHECK(  In.WasPressed( INPUT_XBOX_BTN_A ) );
    CHECK( !In.WasReleased( INPUT_XBOX_BTN_A ) );

    In.BeginFrame();
    CHECK(  In.IsPressed ( INPUT_XBOX_BTN_A ) );
    CHECK( !In.WasPressed( INPUT_XBOX_BTN_A ) );

    In.SetDigital( INPUT_XBOX_BTN_A, false );
    CHECK( !In.IsPressed  ( INPUT_XBOX_BTN_A ) );
    CHECK(  In.WasReleased( INPUT_XBOX_BTN_A ) );

    // Analogue gadgets.
    In.SetAnalog( INPUT_XBOX_STICK_LEFT_X, 0.5f );
    CHECK_NEAR( In.GetValue( INPUT_XBOX_STICK_LEFT_X ), 0.5, 0.0001 );

    // Relative axes accumulate inside a frame and are cleared at the end of it.
    In.AddRelative( INPUT_MOUSE_X_REL, 3.0f );
    In.AddRelative( INPUT_MOUSE_X_REL, 4.0f );
    CHECK_NEAR( In.GetValue( INPUT_MOUSE_X_REL ), 7.0, 0.0001 );
    In.EndFrame();
    CHECK_NEAR( In.GetValue( INPUT_MOUSE_X_REL ), 0.0, 0.0001 );

    // Out of range gadgets are ignored, not crashes.
    In.SetDigital( -1, true );
    In.SetDigital( GADGET_COUNT + 5, true );
    In.SetAnalog( GADGET_COUNT, 1.0f );
    CHECK( !In.IsPressed( -1 ) );
    CHECK_NEAR( In.GetValue( GADGET_COUNT ), 0.0, 0.0001 );
    CHECK( In.CountPressed() >= 0 );

    In.Clear();
    CHECK_EQ_INT( In.CountPressed(), 0 );
}

//------------------------------------------------------------------------------

TEST( gadget_names_round_trip )
{
    for( s32 i = 0; i < GADGET_COUNT; i++ )
    {
        const char* pName = gadget_name_by_id( i );
        CHECK( pName != nullptr );
        CHECK_EQ_INT( gadget_id_by_name( pName ), i );
    }

    CHECK_EQ_INT( gadget_id_by_name( "input_xbox_btn_a" ), INPUT_XBOX_BTN_A );   // case insensitive
    CHECK_EQ_INT( gadget_id_by_name( "NOT_A_GADGET" ),   INPUT_UNDEFINED );
    CHECK_EQ_INT( gadget_id_by_name( nullptr ),          INPUT_UNDEFINED );
    CHECK_STR( gadget_name_by_id( -5 ), "INPUT_UNDEFINED" );
    CHECK_STR( gadget_name_by_id( GADGET_COUNT + 1 ), "INPUT_UNDEFINED" );

    // Sticks and triggers are analogue everywhere.
    CHECK(  gadget_is_analog( INPUT_XBOX_STICK_LEFT_X ) );
    CHECK(  gadget_is_analog( INPUT_XBOX_R_TRIGGER ) );
    CHECK(  gadget_is_analog( INPUT_MOUSE_X_REL ) );

    // On the original Xbox pad the face buttons report pressure too, which is
    // why the touch layer writes 0.0/1.0 into them instead of a digital flag.
    CHECK(  gadget_is_analog( INPUT_XBOX_BTN_A ) );

    // These really are digital.
    CHECK( !gadget_is_analog( INPUT_XBOX_BTN_START ) );
    CHECK( !gadget_is_analog( INPUT_XBOX_BTN_BACK ) );
    CHECK( !gadget_is_analog( INPUT_XBOX_BTN_LEFT ) );
    CHECK( !gadget_is_analog( INPUT_XBOX_BTN_L_STICK ) );
}

//==============================================================================

int main( void )
{
    std::printf( "Running json / input tests\n\n" );
    return ::test::run_all();
}
//==============================================================================
