//==============================================================================
//
//  input_state.cpp
//
//==============================================================================

#include "input_state.hpp"

namespace a51 {

InputState::InputState()
{
    Clear();
}

//------------------------------------------------------------------------------

void InputState::Clear( void )
{
    for( s32 i = 0; i < GADGET_COUNT; i++ )
    {
        m_Value[i]     = 0.0f;
        m_PrevValue[i] = 0.0f;
    }
}

//------------------------------------------------------------------------------

void InputState::BeginFrame( void )
{
    for( s32 i = 0; i < GADGET_COUNT; i++ )
        m_PrevValue[i] = m_Value[i];
}

//------------------------------------------------------------------------------

void InputState::EndFrame( void )
{
    // Relative axes only mean something for the frame they were produced in.
    m_Value[ INPUT_MOUSE_X_REL ] = 0.0f;
    m_Value[ INPUT_MOUSE_Y_REL ] = 0.0f;
    m_Value[ INPUT_MOUSE_WHEEL ] = 0.0f;
}

//------------------------------------------------------------------------------

void InputState::SetAnalog( s32 Gadget, f32 Value )
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return;
    m_Value[ Gadget ] = Value;
}

//------------------------------------------------------------------------------

void InputState::SetDigital( s32 Gadget, bool Pressed )
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return;
    m_Value[ Gadget ] = Pressed ? 1.0f : 0.0f;
}

//------------------------------------------------------------------------------

void InputState::AddRelative( s32 Gadget, f32 Delta )
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return;
    m_Value[ Gadget ] += Delta;
}

//------------------------------------------------------------------------------

f32 InputState::GetValue( s32 Gadget ) const
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return 0.0f;
    return m_Value[ Gadget ];
}

//------------------------------------------------------------------------------

bool InputState::IsPressed( s32 Gadget ) const
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return false;
    return m_Value[ Gadget ] > 0.5f;
}

//------------------------------------------------------------------------------

bool InputState::WasPressed( s32 Gadget ) const
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return false;
    return ( m_Value[ Gadget ] > 0.5f ) && ( m_PrevValue[ Gadget ] <= 0.5f );
}

//------------------------------------------------------------------------------

bool InputState::WasReleased( s32 Gadget ) const
{
    if( Gadget <= INPUT_UNDEFINED || Gadget >= GADGET_COUNT ) return false;
    return ( m_Value[ Gadget ] <= 0.5f ) && ( m_PrevValue[ Gadget ] > 0.5f );
}

//------------------------------------------------------------------------------

s32 InputState::CountPressed( void ) const
{
    s32 n = 0;
    for( s32 i = 0; i < GADGET_COUNT; i++ )
        if( m_Value[i] > 0.5f ) n++;
    return n;
}

} // namespace a51
//==============================================================================
