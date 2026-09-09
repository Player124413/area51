//==============================================================================
//
//  input_state.hpp
//
//  The per frame snapshot of every gadget.  This is the Android side twin of
//  the engine's input system: the touch layer, the game controller layer and
//  the keyboard layer all write into here, and the game logic only ever reads
//  from here (input_IsPressed / input_WasPressed / input_GetValue).
//
//  Fixed size arrays, no allocations, no locks - the render thread owns it.
//
//==============================================================================

#ifndef A51_INPUT_STATE_HPP
#define A51_INPUT_STATE_HPP

#include "a51_types.hpp"
#include "input_gadgets.hpp"

namespace a51 {

class InputState
{
public:
                 InputState();

    // Call once at the start of every frame: current -> previous, so that
    // WasPressed()/WasReleased() describe this frame only.
    void         BeginFrame    ( void );

    // Call at the end of the frame to decay relative axes (mouse deltas etc.).
    void         EndFrame      ( void );

    void         Clear         ( void );

    // Writers.
    void         SetAnalog     ( s32 Gadget, f32 Value );
    void         SetDigital    ( s32 Gadget, bool Pressed );
    void         AddRelative   ( s32 Gadget, f32 Delta );

    // Readers.
    f32          GetValue      ( s32 Gadget ) const;
    bool         IsPressed     ( s32 Gadget ) const;
    bool         WasPressed    ( s32 Gadget ) const;
    bool         WasReleased   ( s32 Gadget ) const;

    // Convenience for the HUD / debug view.
    s32          CountPressed  ( void ) const;

private:
    f32          m_Value[ GADGET_COUNT ];
    f32          m_PrevValue[ GADGET_COUNT ];
};

} // namespace a51

#endif // A51_INPUT_STATE_HPP
//==============================================================================
