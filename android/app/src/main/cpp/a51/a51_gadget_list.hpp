//==============================================================================
//
//  a51_gadget_list.hpp
//
//  The list of input "gadgets" the runtime understands.  The names are the ones
//  from the original engine (xCore/Entropy/e_Input_Gadget_Defines.hpp) so that a
//  saved touch layout stays meaningful after more of the engine gets ported, and
//  so that input_IsPressed( INPUT_XBOX_BTN_A ) keeps working unchanged.
//
//  This file uses the same X-macro trick as the original: it is included twice,
//  once with A51_GADGET( name ) expanding to an enum value and once expanding to
//  a string, which keeps the enum and the name table in sync by construction.
//
//  Do not include directly - see input_gadgets.hpp.
//
//==============================================================================

#ifndef A51_GADGET
#error "a51_gadget_list.hpp must be included through input_gadgets.hpp"
#endif

    //----------------------------------------------------------------------
    //  System
    //----------------------------------------------------------------------
    A51_GADGET( INPUT_UNDEFINED )
    A51_GADGET( INPUT_MSG_EXIT )

    //----------------------------------------------------------------------
    //  Pad - digital
    //----------------------------------------------------------------------
    A51_GADGET( INPUT_XBOX_BTN_START )
    A51_GADGET( INPUT_XBOX_BTN_BACK )
    A51_GADGET( INPUT_XBOX_BTN_UP )
    A51_GADGET( INPUT_XBOX_BTN_DOWN )
    A51_GADGET( INPUT_XBOX_BTN_LEFT )
    A51_GADGET( INPUT_XBOX_BTN_RIGHT )
    A51_GADGET( INPUT_XBOX_BTN_L_STICK )
    A51_GADGET( INPUT_XBOX_BTN_R_STICK )

    //----------------------------------------------------------------------
    //  Pad - analogue buttons
    //----------------------------------------------------------------------
    A51_GADGET( INPUT_XBOX_BTN_A )
    A51_GADGET( INPUT_XBOX_BTN_B )
    A51_GADGET( INPUT_XBOX_BTN_X )
    A51_GADGET( INPUT_XBOX_BTN_Y )
    A51_GADGET( INPUT_XBOX_BTN_WHITE )
    A51_GADGET( INPUT_XBOX_BTN_BLACK )
    A51_GADGET( INPUT_XBOX_L_TRIGGER )
    A51_GADGET( INPUT_XBOX_R_TRIGGER )

    //----------------------------------------------------------------------
    //  Sticks (-1 .. +1)
    //----------------------------------------------------------------------
    A51_GADGET( INPUT_XBOX_STICK_LEFT_X )
    A51_GADGET( INPUT_XBOX_STICK_LEFT_Y )
    A51_GADGET( INPUT_XBOX_STICK_RIGHT_X )
    A51_GADGET( INPUT_XBOX_STICK_RIGHT_Y )

    //----------------------------------------------------------------------
    //  Mouse (used when the touch controls are switched off and a mouse or
    //  trackpad is attached)
    //----------------------------------------------------------------------
    A51_GADGET( INPUT_MOUSE_X_REL )
    A51_GADGET( INPUT_MOUSE_Y_REL )
    A51_GADGET( INPUT_MOUSE_BTN_L )
    A51_GADGET( INPUT_MOUSE_BTN_R )
    A51_GADGET( INPUT_MOUSE_BTN_M )
    A51_GADGET( INPUT_MOUSE_WHEEL )
