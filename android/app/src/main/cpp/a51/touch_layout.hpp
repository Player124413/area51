//==============================================================================
//
//  touch_layout.hpp
//
//  The touch control model.
//
//  Why in C++ and not in Kotlin?  Because this is the part that has to be
//  exactly right on every device: hit testing, joystick maths, dead zones and
//  the file format.  Keeping it here means it is shared by the game, the layout
//  editor and the desktop unit tests, and the Java side only has to draw what
//  the model tells it and forward raw MotionEvent coordinates.
//
//  Coordinate system
//  -----------------
//  Everything is expressed in units of the *screen height*, so a circle stays a
//  circle whatever the aspect ratio:
//
//      X : 0 .. aspect      (aspect = width / height)
//      Y : 0 .. 1
//
//  Control *positions* are stored as fractions of the screen (X = fraction of
//  the width, Y = fraction of the height) so that a layout edited on one phone
//  keeps its meaning on another: a fire button at 88% of the width stays at 88%
//  of the width whether the screen is 4:3 or 21:9.  Only the *size* uses height
//  units, so buttons keep a comfortable physical size on tall screens.
//
//==============================================================================

#ifndef A51_TOUCH_LAYOUT_HPP
#define A51_TOUCH_LAYOUT_HPP

#include "a51_types.hpp"
#include "input_state.hpp"

namespace a51 {

//==============================================================================
//  Types
//==============================================================================

enum ETouchKind
{
    TOUCH_STICK  = 0,     // analogue thumb stick
    TOUCH_BUTTON = 1,     // press / release
    TOUCH_DPAD   = 2      // four way digital pad
};

enum ETouchStyle
{
    TOUCH_STYLE_CIRCLE  = 0,
    TOUCH_STYLE_ROUNDED = 1,
    TOUCH_STYLE_SQUARE  = 2
};

#define A51_TOUCH_MAX_CONTROLS 24
#define A51_TOUCH_MAX_POINTERS 16

struct TouchControl
{
    s32      Id;                                  // stable, saved
    s32      Kind;                                // ETouchKind
    s32      Style;                               // ETouchStyle
    char     Name[24];                            // "STICK_MOVE", "BTN_FIRE", ...
    char     Label[16];                           // what is drawn on the button
    char     Gadget[40];                          // engine gadget it drives
    char     Gadget2[40];                         // second axis for sticks
    f32      X, Y;                                // centre: fraction of width / height
    f32      Size;                                // diameter, fraction of height
    f32      Opacity;                             // 0.1 .. 1.0
    bool     Visible;
    bool     Sticky;                              // sticks: centre follows the finger
};

//==============================================================================
//  Layout
//==============================================================================

class TouchLayout
{
public:
                 TouchLayout();

    // The stock Area 51 layout (left stick + look, fire/reload/use/jump/crouch/
    // melee/weapon/pause on the right).
    void         MakeDefault    ( void );

    s32          Count          ( void ) const { return m_Count; }
    const TouchControl& Get     ( s32 i ) const;
    s32          IndexOfId      ( s32 Id ) const;
    s32          IndexOfName    ( const char* pName ) const;
    TouchControl* Find          ( s32 Id );

    // Editing (used by the edit mode UI).
    void         SetVisible     ( s32 Id, bool Visible );
    void         SetOpacity     ( s32 Id, f32 Opacity );
    void         SetSize        ( s32 Id, f32 Diameter );
    void         Move           ( s32 Id, f32 X, f32 Y, f32 Aspect );   // fractions
    void         SetStyle       ( s32 Id, s32 Style );
    void         ResetControl   ( s32 Id );
    void         ShowAll        ( bool Visible );

    // Keep every control inside the visible area (also called after a rotation).
    void         ClampToScreen  ( f32 Aspect );

    // Hit testing, in height units.  Returns the control index or -1.
    s32          HitTest        ( f32 X, f32 Y, f32 Aspect ) const;
    // Resize handle (the little knob on the top right of a control).
    s32          HitTestHandle  ( f32 X, f32 Y, f32 Aspect ) const;

    // Radius of the grab area of one control, height units.
    f32          ControlRadius  ( s32 i ) const;
    // Where the resize knob sits.
    void         HandlePos      ( s32 i, f32 Aspect, f32& OutX, f32& OutY ) const;

    // Persistence.
    bool         ToJson         ( char* pBuffer, s32 BufferSize ) const;
    bool         FromJson       ( const char* pText );

    void         CopyFrom       ( const TouchLayout& Other );

    s32          Version        ( void ) const { return m_Version; }

private:
    TouchControl m_Controls[ A51_TOUCH_MAX_CONTROLS ];
    s32          m_Count;
    s32          m_Version;
};

//==============================================================================
//  Runtime - turns raw touches into gadget values
//==============================================================================

struct TouchPointer
{
    s32      PointerId;
    s32      ControlIndex;      // -1 = "look" drag
    f32      OriginX, OriginY;  // stick centre when the touch started
    f32      LastX,   LastY;    // last position of this finger
    bool     Used;
};

class TouchRuntime
{
public:
                 TouchRuntime();

    void         SetLayout       ( const TouchLayout* pLayout ) { m_pLayout = pLayout; }
    void         Reset           ( void );

    // Master switch - this is what the "touch controls off" setting drives.
    // Pass the input state so that anything held down at the moment the overlay
    // disappears is released instead of staying latched forever.
    void         SetEnabled      ( bool Enabled, InputState* pOut = nullptr );
    bool         IsEnabled       ( void ) const { return m_Enabled; }

    // Releases every finger the runtime is tracking.
    void         ReleaseAll      ( InputState& Out );

    // Feel.
    void         SetDeadZone     ( f32 DeadZone )  { m_DeadZone = a51_clamp( DeadZone, 0.0f, 0.5f ); }
    f32          GetDeadZone     ( void ) const    { return m_DeadZone; }
    void         SetLookSensitivity( f32 Sens )    { m_LookSensitivity = a51_clamp( Sens, 0.05f, 5.0f ); }
    f32          GetLookSensitivity( void ) const  { return m_LookSensitivity; }
    void         SetLookEnabled  ( bool Enabled )  { m_LookEnabled = Enabled; }
    bool         IsLookEnabled   ( void ) const    { return m_LookEnabled; }
    void         SetInvertLookY  ( bool Invert )   { m_InvertLookY = Invert; }
    bool         GetInvertLookY  ( void ) const    { return m_InvertLookY; }

    // MotionEvent -> gadgets.  Action is one of A51_ACTION_*, coordinates are
    // in height units.  Returns true when the touch was consumed.
    bool         OnTouch         ( s32 Action, s32 PointerId, f32 X, f32 Y, f32 Aspect, InputState& Out );

    // Called once per frame: releases anything the OS silently dropped and
    // decays the look deltas.
    void         Update          ( InputState& Out );

    // Position of the stick knob while it is being dragged (for the drawing
    // code).  Returns false when that stick is not currently touched.
    bool         GetStickKnob    ( s32 ControlIndex, f32& OutX, f32& OutY ) const;
    bool         IsControlActive ( s32 ControlIndex ) const;

    // Raw look delta of the last frame, in "degrees-ish" units for the HUD.
    f32          GetLookX        ( void ) const { return m_LookX; }
    f32          GetLookY        ( void ) const { return m_LookY; }

private:
    s32          FindPointer     ( s32 PointerId ) const;
    s32          ClaimPointer    ( s32 PointerId );
    void         ReleasePointer  ( s32 PointerId, InputState& Out );
    void         ApplyStick      ( s32 Slot, f32 X, f32 Y, InputState& Out );
    void         ClearStick      ( s32 Slot, InputState& Out );

    const TouchLayout* m_pLayout;

    bool         m_Enabled;
    bool         m_LookEnabled;
    bool         m_InvertLookY;
    f32          m_DeadZone;
    f32          m_LookSensitivity;

    f32          m_LookX, m_LookY;

    TouchPointer m_Pointers[ A51_TOUCH_MAX_POINTERS ];
};

// MotionEvent actions we understand (kept in sync with the Java constants).
enum
{
    A51_ACTION_DOWN         = 0,
    A51_ACTION_UP           = 1,
    A51_ACTION_MOVE         = 2,
    A51_ACTION_CANCEL       = 3,
    A51_ACTION_POINTER_DOWN = 5,
    A51_ACTION_POINTER_UP   = 6
};

} // namespace a51

#endif // A51_TOUCH_LAYOUT_HPP
//==============================================================================
