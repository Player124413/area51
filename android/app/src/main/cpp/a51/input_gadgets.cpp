//==============================================================================
//
//  input_gadgets.cpp
//
//==============================================================================

#include "input_gadgets.hpp"

namespace a51 {

const char* const g_GadgetNames[ GADGET_COUNT ] =
{
#define A51_GADGET( name ) #name,
    #include "a51_gadget_list.hpp"
#undef A51_GADGET
};

//------------------------------------------------------------------------------

s32 gadget_id_by_name( const char* pName )
{
    if( !pName ) return INPUT_UNDEFINED;

    for( s32 i = 0; i < GADGET_COUNT; i++ )
        if( a51_strieq( g_GadgetNames[i], pName ) )
            return i;

    return INPUT_UNDEFINED;
}

//------------------------------------------------------------------------------

const char* gadget_name_by_id( s32 Id )
{
    if( Id < 0 || Id >= GADGET_COUNT ) return "INPUT_UNDEFINED";
    return g_GadgetNames[ Id ];
}

//------------------------------------------------------------------------------

// On the original Xbox pad the face buttons are analogue (they report how hard
// they are pressed), which is why they are listed here.  Sticks, triggers and
// mouse axes are analogue on every platform.
bool gadget_is_analog( s32 Id )
{
    switch( Id )
    {
        case INPUT_XBOX_L_TRIGGER:
        case INPUT_XBOX_R_TRIGGER:
        case INPUT_XBOX_BTN_A:
        case INPUT_XBOX_BTN_B:
        case INPUT_XBOX_BTN_X:
        case INPUT_XBOX_BTN_Y:
        case INPUT_XBOX_BTN_WHITE:
        case INPUT_XBOX_BTN_BLACK:
        case INPUT_XBOX_STICK_LEFT_X:
        case INPUT_XBOX_STICK_LEFT_Y:
        case INPUT_XBOX_STICK_RIGHT_X:
        case INPUT_XBOX_STICK_RIGHT_Y:
        case INPUT_MOUSE_X_REL:
        case INPUT_MOUSE_Y_REL:
        case INPUT_MOUSE_WHEEL:
            return true;
        default:
            return false;
    }
}

} // namespace a51
//==============================================================================
