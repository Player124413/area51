//==============================================================================
//
//  touch_layout.cpp
//
//==============================================================================

#include "touch_layout.hpp"
#include "json.hpp"
#include "log.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace a51 {

//==============================================================================
//  Helpers
//==============================================================================

static void set_str( char* pDst, s32 DstSize, const char* pSrc )
{
    if( !pSrc ) { pDst[0] = 0; return; }
    snprintf( pDst, (size_t)DstSize, "%s", pSrc );
}

//==============================================================================
//  TouchLayout
//==============================================================================

TouchLayout::TouchLayout()
{
    m_Count   = 0;
    m_Version = 1;
    MakeDefault();
}

//------------------------------------------------------------------------------

const TouchControl& TouchLayout::Get( s32 i ) const
{
    static TouchControl s_Empty = {};
    if( i < 0 || i >= m_Count ) return s_Empty;
    return m_Controls[i];
}

//------------------------------------------------------------------------------

s32 TouchLayout::IndexOfId( s32 Id ) const
{
    for( s32 i = 0; i < m_Count; i++ )
        if( m_Controls[i].Id == Id ) return i;
    return -1;
}

//------------------------------------------------------------------------------

s32 TouchLayout::IndexOfName( const char* pName ) const
{
    if( !pName ) return -1;
    for( s32 i = 0; i < m_Count; i++ )
        if( strcmp( m_Controls[i].Name, pName ) == 0 ) return i;
    return -1;
}

//------------------------------------------------------------------------------

TouchControl* TouchLayout::Find( s32 Id )
{
    s32 i = IndexOfId( Id );
    return ( i >= 0 ) ? &m_Controls[i] : nullptr;
}

//==============================================================================
//  The stock layout
//==============================================================================

void TouchLayout::MakeDefault( void )
{
    m_Count = 0;

    //  id  kind            style              x     y    size  opac  gadget / gadget2
    struct Def
    {
        s32         Id;
        s32         Kind;
        s32         Style;
        const char* Name;
        const char* Label;
        const char* Gadget;
        const char* Gadget2;
        f32         X, Y, Size, Opacity;
        bool        Visible;
        bool        Sticky;
    };

    static const Def s_Defaults[] =
    {
        {  1, TOUCH_STICK,  TOUCH_STYLE_CIRCLE,  "STICK_MOVE",  "",      "INPUT_XBOX_STICK_LEFT_X", "INPUT_XBOX_STICK_LEFT_Y", 0.16f, 0.76f, 0.30f, 0.45f, true,  true  },
        {  2, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_FIRE",    "FIRE",  "INPUT_XBOX_R_TRIGGER",    "",                        0.88f, 0.74f, 0.17f, 0.55f, true,  false },
        {  3, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_AIM",     "AIM",   "INPUT_XBOX_L_TRIGGER",    "",                        0.14f, 0.46f, 0.11f, 0.50f, true,  false },
        {  4, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_JUMP",    "JUMP",  "INPUT_XBOX_BTN_A",        "",                        0.74f, 0.60f, 0.11f, 0.50f, true,  false },
        {  5, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_CROUCH",  "DUCK",  "INPUT_XBOX_BTN_B",        "",                        0.61f, 0.74f, 0.11f, 0.50f, true,  false },
        {  6, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_USE",     "USE",   "INPUT_XBOX_BTN_X",        "",                        0.95f, 0.50f, 0.11f, 0.50f, true,  false },
        {  7, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_RELOAD",  "RLD",   "INPUT_XBOX_BTN_Y",        "",                        0.73f, 0.90f, 0.10f, 0.50f, true,  false },
        {  8, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_MELEE",   "HIT",   "INPUT_XBOX_BTN_BLACK",    "",                        0.90f, 0.30f, 0.10f, 0.45f, true,  false },
        {  9, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_GRENADE", "GRN",   "INPUT_XBOX_BTN_WHITE",    "",                        0.60f, 0.46f, 0.10f, 0.45f, true,  false },
        { 10, TOUCH_BUTTON, TOUCH_STYLE_CIRCLE,  "BTN_SPRINT",  "RUN",   "INPUT_XBOX_BTN_L_STICK",  "",                        0.30f, 0.62f, 0.09f, 0.45f, true,  false },
        { 11, TOUCH_DPAD,   TOUCH_STYLE_CIRCLE,  "DPAD_WEAPON", "",      "INPUT_XBOX_BTN_LEFT",     "INPUT_XBOX_BTN_RIGHT",     0.30f, 0.90f, 0.20f, 0.35f, false, false },
        { 12, TOUCH_BUTTON, TOUCH_STYLE_ROUNDED, "BTN_PAUSE",   "II",    "INPUT_XBOX_BTN_START",    "",                        0.06f, 0.07f, 0.08f, 0.55f, true,  false },
        { 13, TOUCH_BUTTON, TOUCH_STYLE_ROUNDED, "BTN_BACK",    "<",     "INPUT_XBOX_BTN_BACK",     "",                        0.14f, 0.07f, 0.08f, 0.45f, false, false },
    };

    const s32 nDefs = (s32)( sizeof( s_Defaults ) / sizeof( s_Defaults[0] ) );

    for( s32 i = 0; i < nDefs && m_Count < A51_TOUCH_MAX_CONTROLS; i++ )
    {
        const Def&    D = s_Defaults[i];
        TouchControl& C = m_Controls[ m_Count++ ];

        C.Id      = D.Id;
        C.Kind    = D.Kind;
        C.Style   = D.Style;
        C.X       = D.X;
        C.Y       = D.Y;
        C.Size    = D.Size;
        C.Opacity = D.Opacity;
        C.Visible = D.Visible;
        C.Sticky  = D.Sticky;
        set_str( C.Name,    sizeof( C.Name ),    D.Name    );
        set_str( C.Label,   sizeof( C.Label ),   D.Label   );
        set_str( C.Gadget,  sizeof( C.Gadget ),  D.Gadget  );
        set_str( C.Gadget2, sizeof( C.Gadget2 ), D.Gadget2 );
    }
}

//==============================================================================
//  Editing
//==============================================================================

void TouchLayout::SetVisible( s32 Id, bool Visible )
{
    TouchControl* pC = Find( Id );
    if( pC ) pC->Visible = Visible;
}

//------------------------------------------------------------------------------

void TouchLayout::SetOpacity( s32 Id, f32 Opacity )
{
    TouchControl* pC = Find( Id );
    if( pC ) pC->Opacity = a51_clamp( Opacity, 0.10f, 1.00f );
}

//------------------------------------------------------------------------------

void TouchLayout::SetSize( s32 Id, f32 Diameter )
{
    TouchControl* pC = Find( Id );
    if( pC ) pC->Size = a51_clamp( Diameter, 0.05f, 0.60f );
}

//------------------------------------------------------------------------------

void TouchLayout::SetStyle( s32 Id, s32 Style )
{
    TouchControl* pC = Find( Id );
    if( pC ) pC->Style = a51_clamp( Style, (s32)TOUCH_STYLE_CIRCLE, (s32)TOUCH_STYLE_SQUARE );
}

//------------------------------------------------------------------------------

void TouchLayout::Move( s32 Id, f32 X, f32 Y, f32 Aspect )
{
    TouchControl* pC = Find( Id );
    if( !pC ) return;

    pC->X = X;
    pC->Y = Y;

    // Keep it on screen.
    f32 RadX = ( pC->Size * 0.5f ) / ( Aspect > 0.01f ? Aspect : 1.0f );
    f32 RadY = pC->Size * 0.5f;

    pC->X = a51_clamp( pC->X, RadX,        1.0f - RadX );
    pC->Y = a51_clamp( pC->Y, RadY + 0.01f, 1.0f - RadY - 0.01f );
}

//------------------------------------------------------------------------------

void TouchLayout::ResetControl( s32 Id )
{
    TouchLayout Defs;
    Defs.MakeDefault();

    s32 iDef = Defs.IndexOfId( Id );
    s32 iMine = IndexOfId( Id );
    if( iDef < 0 || iMine < 0 ) return;

    m_Controls[ iMine ] = Defs.Get( iDef );
}

//------------------------------------------------------------------------------

void TouchLayout::ShowAll( bool Visible )
{
    for( s32 i = 0; i < m_Count; i++ )
        m_Controls[i].Visible = Visible;
}

//------------------------------------------------------------------------------

void TouchLayout::ClampToScreen( f32 Aspect )
{
    if( Aspect < 0.1f ) Aspect = 1.0f;

    for( s32 i = 0; i < m_Count; i++ )
    {
        TouchControl& C = m_Controls[i];

        if( C.Size < 0.05f ) C.Size = 0.05f;
        if( C.Size > 0.60f ) C.Size = 0.60f;

        C.Opacity = a51_clamp( C.Opacity, 0.10f, 1.00f );

        f32 RadX = ( C.Size * 0.5f ) / Aspect;
        f32 RadY = C.Size * 0.5f;

        C.X = a51_clamp( C.X, RadX,        1.0f - RadX );
        C.Y = a51_clamp( C.Y, RadY + 0.01f, 1.0f - RadY - 0.01f );
    }
}

//==============================================================================
//  Hit testing
//==============================================================================

f32 TouchLayout::ControlRadius( s32 i ) const
{
    if( i < 0 || i >= m_Count ) return 0.0f;
    return m_Controls[i].Size * 0.5f;
}

//------------------------------------------------------------------------------

void TouchLayout::HandlePos( s32 i, f32 Aspect, f32& OutX, f32& OutY ) const
{
    if( i < 0 || i >= m_Count ) { OutX = OutY = 0.0f; return; }

    const TouchControl& C = m_Controls[i];
    const f32 Inv = 0.70710678f;                 // 45 degrees

    OutX = C.X * Aspect + C.Size * 0.5f * Inv;
    OutY = C.Y          - C.Size * 0.5f * Inv;
}

//------------------------------------------------------------------------------

s32 TouchLayout::HitTest( f32 X, f32 Y, f32 Aspect ) const
{
    s32 Best      = -1;
    f32 BestDist  = 1e30f;

    for( s32 i = 0; i < m_Count; i++ )
    {
        const TouchControl& C = m_Controls[i];
        if( !C.Visible ) continue;

        f32 dx = X - C.X * Aspect;
        f32 dy = Y - C.Y;
        f32 r  = C.Size * 0.5f;

        // A little forgiveness on top of the drawn radius: fingers are fat and
        // the drawn circle is usually smaller than the touch target should be.
        r += 0.012f;

        f32 Dist = sqrtf( dx * dx + dy * dy );
        if( Dist <= r && Dist < BestDist )
        {
            BestDist = Dist;
            Best     = i;
        }
    }

    return Best;
}

//------------------------------------------------------------------------------

s32 TouchLayout::HitTestHandle( f32 X, f32 Y, f32 Aspect ) const
{
    const f32 HandleRadius = 0.035f;

    for( s32 i = 0; i < m_Count; i++ )
    {
        f32 hx, hy;
        HandlePos( i, Aspect, hx, hy );

        f32 dx = X - hx;
        f32 dy = Y - hy;
        if( dx * dx + dy * dy <= HandleRadius * HandleRadius )
            return i;
    }
    return -1;
}

//==============================================================================
//  Persistence
//==============================================================================

bool TouchLayout::ToJson( char* pBuffer, s32 BufferSize ) const
{
    if( !pBuffer || BufferSize < 512 ) return false;

    JsonWriter W( pBuffer, BufferSize );

    W.ObjectBegin();
    W.Pair( "format",  (s32)1 );
    W.Pair( "version", m_Version );
    W.Key( "controls" );
    W.ArrayBegin();

    for( s32 i = 0; i < m_Count; i++ )
    {
        const TouchControl& C = m_Controls[i];

        W.ObjectBegin();
        W.Pair( "id",      C.Id );
        W.Pair( "name",    C.Name );
        W.Pair( "label",   C.Label );
        W.Pair( "kind",    C.Kind );
        W.Pair( "style",   C.Style );
        W.Pair( "gadget",  C.Gadget );
        W.Pair( "gadget2", C.Gadget2 );
        W.Pair( "x",       (f64)C.X );
        W.Pair( "y",       (f64)C.Y );
        W.Pair( "size",    (f64)C.Size );
        W.Pair( "opacity", (f64)C.Opacity );
        W.Pair( "visible", C.Visible );
        W.Pair( "sticky",  C.Sticky );
        W.ObjectEnd();
    }

    W.ArrayEnd();
    W.ObjectEnd();

    return !W.Overflowed();
}

//------------------------------------------------------------------------------

bool TouchLayout::FromJson( const char* pText )
{
    if( !pText ) return false;

    Json J;
    if( !J.Parse( pText ) )
    {
        A51_LOGW( "touch layout: %s", J.GetError() );
        return false;
    }

    s32 Root = J.Root();
    if( Root < 0 ) return false;

    s32 Controls = J.Find( Root, "controls" );
    if( Controls < 0 ) return false;
    if( J.Node( Controls ).Type != JSON_ARRAY )
    {
        A51_LOGW( "touch layout: 'controls' is not an array" );
        return false;
    }

    // Start from the stock layout so that a control added by a newer version of
    // the app still exists when an older layout file is loaded.
    MakeDefault();

    s32 n = J.Count( Controls );
    for( s32 i = 0; i < n; i++ )
    {
        s32 Item = J.At( Controls, i );
        if( Item < 0 ) continue;

        s32 Id = J.GetInt( Item, "id", -1 );
        s32 Index = IndexOfId( Id );

        TouchControl* pC;
        if( Index >= 0 )
        {
            pC = &m_Controls[ Index ];
        }
        else if( m_Count < A51_TOUCH_MAX_CONTROLS )
        {
            pC = &m_Controls[ m_Count++ ];
            memset( pC, 0, sizeof( *pC ) );
            pC->Id      = ( Id >= 0 ) ? Id : ( 1000 + m_Count );
            pC->Opacity = 0.5f;
            pC->Size    = 0.11f;
        }
        else break;

        char Tmp[64];

        if( J.CopyStr( J.Find( Item, "name" ),    Tmp, sizeof( Tmp ) ) > 0 ) set_str( pC->Name,    sizeof( pC->Name ),    Tmp );
        if( J.CopyStr( J.Find( Item, "label" ),   Tmp, sizeof( Tmp ) ) > 0 || J.Find( Item, "label" ) >= 0 )
                                                                            set_str( pC->Label,   sizeof( pC->Label ),   Tmp );
        if( J.CopyStr( J.Find( Item, "gadget" ),  Tmp, sizeof( Tmp ) ) > 0 ) set_str( pC->Gadget,  sizeof( pC->Gadget ),  Tmp );
        if( J.CopyStr( J.Find( Item, "gadget2" ), Tmp, sizeof( Tmp ) ) > 0 ) set_str( pC->Gadget2, sizeof( pC->Gadget2 ), Tmp );

        pC->Kind    = J.GetInt ( Item, "kind",    pC->Kind    );
        pC->Style   = J.GetInt ( Item, "style",   pC->Style   );
        pC->X       = (f32)J.GetNum( Item, "x",       (f64)pC->X       );
        pC->Y       = (f32)J.GetNum( Item, "y",       (f64)pC->Y       );
        pC->Size    = (f32)J.GetNum( Item, "size",    (f64)pC->Size    );
        pC->Opacity = (f32)J.GetNum( Item, "opacity", (f64)pC->Opacity );
        pC->Visible = J.GetBool( Item, "visible", pC->Visible );
        pC->Sticky  = J.GetBool( Item, "sticky",  pC->Sticky  );
    }

    m_Version = J.GetInt( Root, "version", 1 );

    ClampToScreen( 16.0f / 9.0f );
    return true;
}

//------------------------------------------------------------------------------

void TouchLayout::CopyFrom( const TouchLayout& Other )
{
    m_Count   = Other.m_Count;
    m_Version = Other.m_Version;
    for( s32 i = 0; i < m_Count; i++ )
        m_Controls[i] = Other.m_Controls[i];
}

//==============================================================================
//  TouchRuntime
//==============================================================================

TouchRuntime::TouchRuntime()
{
    m_pLayout         = nullptr;
    m_Enabled         = true;
    m_LookEnabled     = true;
    m_InvertLookY     = false;
    m_DeadZone        = 0.15f;
    m_LookSensitivity = 1.0f;
    m_LookX           = 0.0f;
    m_LookY           = 0.0f;
    Reset();
}

//------------------------------------------------------------------------------

void TouchRuntime::Reset( void )
{
    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
    {
        m_Pointers[i].PointerId    = -1;
        m_Pointers[i].ControlIndex = -1;
        m_Pointers[i].OriginX      = 0.0f;
        m_Pointers[i].OriginY      = 0.0f;
        m_Pointers[i].LastX        = 0.0f;
        m_Pointers[i].LastY        = 0.0f;
        m_Pointers[i].Used         = false;
    }
    m_LookX = 0.0f;
    m_LookY = 0.0f;
}

//------------------------------------------------------------------------------

void TouchRuntime::SetEnabled( bool Enabled, InputState* pOut )
{
    if( m_Enabled == Enabled ) return;

    // Dropping the overlay must not leave a button stuck down.
    if( !Enabled )
    {
        if( pOut ) ReleaseAll( *pOut );
        else       Reset();
    }

    m_Enabled = Enabled;
}

//------------------------------------------------------------------------------

void TouchRuntime::ReleaseAll( InputState& Out )
{
    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
    {
        if( !m_Pointers[i].Used ) continue;

        s32 PointerId = m_Pointers[i].PointerId;
        ReleasePointer( PointerId, Out );
    }
}

//------------------------------------------------------------------------------

s32 TouchRuntime::FindPointer( s32 PointerId ) const
{
    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
        if( m_Pointers[i].Used && m_Pointers[i].PointerId == PointerId )
            return i;
    return -1;
}

//------------------------------------------------------------------------------

s32 TouchRuntime::ClaimPointer( s32 PointerId )
{
    s32 Slot = FindPointer( PointerId );
    if( Slot >= 0 ) return Slot;

    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
    {
        if( !m_Pointers[i].Used )
        {
            m_Pointers[i].Used      = true;
            m_Pointers[i].PointerId = PointerId;
            return i;
        }
    }
    return -1;
}

//==============================================================================
//  Sticks
//==============================================================================

void TouchRuntime::ApplyStick( s32 Slot, f32 X, f32 Y, InputState& Out )
{
    const TouchPointer& P = m_Pointers[ Slot ];
    if( P.ControlIndex < 0 || !m_pLayout ) return;

    const TouchControl& C = m_pLayout->Get( P.ControlIndex );

    s32 GadgetX = gadget_id_by_name( C.Gadget  );
    s32 GadgetY = gadget_id_by_name( C.Gadget2 );

    f32 Radius = m_pLayout->ControlRadius( P.ControlIndex );
    if( Radius < 0.001f ) Radius = 0.001f;

    f32 dx = ( X - P.OriginX ) / Radius;
    f32 dy = ( Y - P.OriginY ) / Radius;

    f32 Mag = sqrtf( dx * dx + dy * dy );
    if( Mag > 1.0f ) { dx /= Mag; dy /= Mag; Mag = 1.0f; }

    // Dead zone with a smooth ramp so the stick does not jump at the edge.
    f32 dz = m_DeadZone;
    if( Mag <= dz )
    {
        dx = 0.0f;
        dy = 0.0f;
    }
    else
    {
        f32 Scaled = ( Mag - dz ) / ( 1.0f - dz );
        f32 nx = dx / Mag;
        f32 ny = dy / Mag;
        dx = nx * Scaled;
        dy = ny * Scaled;
    }

    // Screen space Y grows downwards, the engine expects up = positive.
    Out.SetAnalog( GadgetX,  dx );
    Out.SetAnalog( GadgetY, -dy );
}

//------------------------------------------------------------------------------

void TouchRuntime::ClearStick( s32 Slot, InputState& Out )
{
    const TouchPointer& P = m_Pointers[ Slot ];
    if( P.ControlIndex < 0 || !m_pLayout ) return;

    const TouchControl& C = m_pLayout->Get( P.ControlIndex );
    Out.SetAnalog( gadget_id_by_name( C.Gadget  ), 0.0f );
    Out.SetAnalog( gadget_id_by_name( C.Gadget2 ), 0.0f );
}

//==============================================================================
//  Buttons
//==============================================================================

static void set_control_value( const TouchControl& C, bool Pressed, InputState& Out )
{
    s32 Gadget = gadget_id_by_name( C.Gadget );

    if( gadget_is_analog( Gadget ) ) Out.SetAnalog ( Gadget, Pressed ? 1.0f : 0.0f );
    else                             Out.SetDigital( Gadget, Pressed );
}

//==============================================================================
//  D-Pad
//==============================================================================

static void apply_dpad( const TouchControl& C, f32 dx, f32 dy, bool Pressed, InputState& Out )
{
    s32 Left   = gadget_id_by_name( C.Gadget  );     // "left"  gadget
    s32 Right  = gadget_id_by_name( C.Gadget2 );     // "right" gadget
    s32 Up     = gadget_id_by_name( "INPUT_XBOX_BTN_UP"   );
    s32 Down   = gadget_id_by_name( "INPUT_XBOX_BTN_DOWN" );

    bool L = false, R = false, U = false, D = false;

    if( Pressed )
    {
        if( fabsf( dx ) > fabsf( dy ) )
        {
            if( dx < -0.25f ) L = true;
            if( dx >  0.25f ) R = true;
        }
        else
        {
            if( dy < -0.25f ) U = true;
            if( dy >  0.25f ) D = true;
        }
    }

    Out.SetDigital( Left,  L );
    Out.SetDigital( Right, R );
    Out.SetDigital( Up,    U );
    Out.SetDigital( Down,  D );
}

//==============================================================================
//  Touch entry point
//==============================================================================

bool TouchRuntime::OnTouch( s32 Action, s32 PointerId, f32 X, f32 Y, f32 Aspect, InputState& Out )
{
    if( !m_Enabled || !m_pLayout ) return false;

    s32 Masked = Action;

    switch( Masked )
    {
        //------------------------------------------------------------------
        case A51_ACTION_DOWN:
        case A51_ACTION_POINTER_DOWN:
        {
            s32 Slot = ClaimPointer( PointerId );
            if( Slot < 0 ) return false;

            s32 Hit = m_pLayout->HitTest( X, Y, Aspect );
            m_Pointers[Slot].ControlIndex = Hit;
            m_Pointers[Slot].OriginX      = X;
            m_Pointers[Slot].OriginY      = Y;
            m_Pointers[Slot].LastX        = X;
            m_Pointers[Slot].LastY        = Y;

            if( Hit >= 0 )
            {
                const TouchControl& C = m_pLayout->Get( Hit );

                if( C.Kind == TOUCH_STICK )
                {
                    // A "sticky" stick recentres under the finger, which is what
                    // most phone shooters do and what feels best without a
                    // physical rim to find by feel.
                    if( C.Sticky )
                    {
                        m_Pointers[Slot].OriginX = X;
                        m_Pointers[Slot].OriginY = Y;
                    }
                    else
                    {
                        m_Pointers[Slot].OriginX = C.X * Aspect;
                        m_Pointers[Slot].OriginY = C.Y;
                    }
                    ApplyStick( Slot, X, Y, Out );
                }
                else if( C.Kind == TOUCH_DPAD )
                {
                    f32 Radius = m_pLayout->ControlRadius( Hit );
                    if( Radius < 0.001f ) Radius = 0.001f;
                    m_Pointers[Slot].OriginX = C.X * Aspect;
                    m_Pointers[Slot].OriginY = C.Y;
                    apply_dpad( C, ( X - m_Pointers[Slot].OriginX ) / Radius,
                                     ( Y - m_Pointers[Slot].OriginY ) / Radius, true, Out );
                }
                else
                {
                    set_control_value( C, true, Out );
                }
            }
            return true;
        }

        //------------------------------------------------------------------
        case A51_ACTION_MOVE:
        {
            s32 Slot = FindPointer( PointerId );
            if( Slot < 0 ) return false;

            TouchPointer& P = m_Pointers[ Slot ];

            P.LastX = X;
            P.LastY = Y;

            if( P.ControlIndex >= 0 )
            {
                const TouchControl& C = m_pLayout->Get( P.ControlIndex );

                if( C.Kind == TOUCH_STICK )
                {
                    ApplyStick( Slot, X, Y, Out );
                }
                else if( C.Kind == TOUCH_DPAD )
                {
                    f32 Radius = m_pLayout->ControlRadius( P.ControlIndex );
                    if( Radius < 0.001f ) Radius = 0.001f;
                    apply_dpad( C, ( X - P.OriginX ) / Radius,
                                   ( Y - P.OriginY ) / Radius, true, Out );
                }
                // Plain buttons ignore movement: the press stays latched until
                // the finger lifts, which prevents "lost" shots when a player
                // slides off the button while firing.
            }
            else if( m_LookEnabled )
            {
                // Camera drag.
                f32 dx = X - P.OriginX;
                f32 dy = Y - P.OriginY;

                m_Pointers[Slot].OriginX = X;
                m_Pointers[Slot].OriginY = Y;

                const f32 Scale = 0.35f * m_LookSensitivity;

                m_LookX = dx * Scale;
                m_LookY = dy * Scale * ( m_InvertLookY ? 1.0f : -1.0f );

                Out.AddRelative( INPUT_MOUSE_X_REL,  m_LookX );
                Out.AddRelative( INPUT_MOUSE_Y_REL,  m_LookY );
            }
            return true;
        }

        //------------------------------------------------------------------
        case A51_ACTION_UP:
        case A51_ACTION_POINTER_UP:
        case A51_ACTION_CANCEL:
        {
            ReleasePointer( PointerId, Out );
            return true;
        }

        default:
            return false;
    }
}

//------------------------------------------------------------------------------

void TouchRuntime::ReleasePointer( s32 PointerId, InputState& Out )
{
    s32 Slot = FindPointer( PointerId );
    if( Slot < 0 ) return;

    s32 Hit = m_Pointers[ Slot ].ControlIndex;

    if( Hit >= 0 && m_pLayout )
    {
        const TouchControl& C = m_pLayout->Get( Hit );

        if( C.Kind == TOUCH_STICK )      ClearStick( Slot, Out );
        else if( C.Kind == TOUCH_DPAD )  apply_dpad( C, 0.0f, 0.0f, false, Out );
        else                             set_control_value( C, false, Out );
    }

    m_Pointers[ Slot ].Used         = false;
    m_Pointers[ Slot ].PointerId    = -1;
    m_Pointers[ Slot ].ControlIndex = -1;
    m_Pointers[ Slot ].LastX        = 0.0f;
    m_Pointers[ Slot ].LastY        = 0.0f;
}

//------------------------------------------------------------------------------

void TouchRuntime::Update( InputState& Out )
{
    (void)Out;

    // Decay the HUD readout of the look delta.
    m_LookX *= 0.80f;
    m_LookY *= 0.80f;
    if( fabsf( m_LookX ) < 0.01f ) m_LookX = 0.0f;
    if( fabsf( m_LookY ) < 0.01f ) m_LookY = 0.0f;
}

//==============================================================================
//  Drawing helpers
//==============================================================================

bool TouchRuntime::GetStickKnob( s32 ControlIndex, f32& OutX, f32& OutY ) const
{
    if( !m_pLayout ) return false;

    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
    {
        if( !m_Pointers[i].Used ) continue;
        if( m_Pointers[i].ControlIndex != ControlIndex ) continue;

        const TouchControl& C = m_pLayout->Get( ControlIndex );
        if( C.Kind != TOUCH_STICK ) return false;

        f32 Radius = m_pLayout->ControlRadius( ControlIndex );
        f32 dx     = m_Pointers[i].LastX - m_Pointers[i].OriginX;
        f32 dy     = m_Pointers[i].LastY - m_Pointers[i].OriginY;
        f32 Mag    = sqrtf( dx * dx + dy * dy );
        if( Mag > Radius && Mag > 0.0001f ) { dx = dx / Mag * Radius; dy = dy / Mag * Radius; }

        OutX = m_Pointers[i].OriginX + dx;
        OutY = m_Pointers[i].OriginY + dy;
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------

bool TouchRuntime::IsControlActive( s32 ControlIndex ) const
{
    for( s32 i = 0; i < A51_TOUCH_MAX_POINTERS; i++ )
        if( m_Pointers[i].Used && m_Pointers[i].ControlIndex == ControlIndex )
            return true;
    return false;
}

} // namespace a51
//==============================================================================
