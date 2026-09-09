//==============================================================================
//
//  input_gadgets.hpp
//
//  Gadget enum + name lookup.  Generated from a51_gadget_list.hpp.
//
//==============================================================================

#ifndef A51_INPUT_GADGETS_HPP
#define A51_INPUT_GADGETS_HPP

#include "a51_types.hpp"

namespace a51 {

//------------------------------------------------------------------------------
//  Enum
//------------------------------------------------------------------------------

enum EGadget
{
#define A51_GADGET( name ) name,
    #include "a51_gadget_list.hpp"
#undef A51_GADGET
    GADGET_COUNT
};

//------------------------------------------------------------------------------
//  Names
//------------------------------------------------------------------------------

extern const char* const g_GadgetNames[ GADGET_COUNT ];

// Case sensitive by default, the engine spells them in upper case.
s32         gadget_id_by_name  ( const char* pName );
const char* gadget_name_by_id  ( s32 Id );

// True for gadgets that carry a continuous value rather than a press.
bool        gadget_is_analog   ( s32 Id );

} // namespace a51

#endif // A51_INPUT_GADGETS_HPP
//==============================================================================
