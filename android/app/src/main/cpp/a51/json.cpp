//==============================================================================
//
//  json.cpp
//
//==============================================================================

#include "json.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace a51 {

//==============================================================================
//  JsonWriter
//==============================================================================

JsonWriter::JsonWriter( char* pBuffer, s32 BufferSize )
{
    m_pBuffer  = pBuffer;
    m_Size     = BufferSize;
    m_Length   = 0;
    m_Depth    = 0;
    m_AfterKey = false;
    m_Overflow = false;

    for( s32 i = 0; i < 32; i++ ) m_Items[i] = 0;
    if( m_pBuffer && m_Size > 0 ) m_pBuffer[0] = 0;
}

//------------------------------------------------------------------------------

void JsonWriter::Put( char c )
{
    if( m_Length + 1 >= m_Size ) { m_Overflow = true; return; }
    m_pBuffer[ m_Length++ ] = c;
    m_pBuffer[ m_Length ]   = 0;
}

//------------------------------------------------------------------------------

void JsonWriter::Put( const char* pStr )
{
    while( *pStr ) Put( *pStr++ );
}

//------------------------------------------------------------------------------

void JsonWriter::PutEscaped( const char* pStr )
{
    Put( '"' );
    for( const char* p = pStr ? pStr : ""; *p; p++ )
    {
        switch( *p )
        {
            case '"':  Put( '\\' ); Put( '"'  ); break;
            case '\\': Put( '\\' ); Put( '\\' ); break;
            case '\n': Put( '\\' ); Put( 'n'  ); break;
            case '\r': Put( '\\' ); Put( 'r'  ); break;
            case '\t': Put( '\\' ); Put( 't'  ); break;
            default:
                if( (unsigned char)*p < 0x20 )
                {
                    char Tmp[8];
                    snprintf( Tmp, sizeof( Tmp ), "\\u%04x", (unsigned char)*p );
                    Put( Tmp );
                }
                else Put( *p );
                break;
        }
    }
    Put( '"' );
}

//------------------------------------------------------------------------------

void JsonWriter::Comma( void )
{
    if( m_AfterKey ) { m_AfterKey = false; return; }

    if( m_Depth > 0 && m_Items[ m_Depth - 1 ] > 0 ) Put( ',' );
    if( m_Depth > 0 ) m_Items[ m_Depth - 1 ]++;
}

//------------------------------------------------------------------------------

void JsonWriter::ObjectBegin( void )
{
    Comma();
    Put( '{' );
    if( m_Depth < 32 ) m_Items[ m_Depth ] = 0;
    m_Depth++;
}

//------------------------------------------------------------------------------

void JsonWriter::ObjectEnd( void )
{
    if( m_Depth > 0 ) m_Depth--;
    Put( '}' );
}

//------------------------------------------------------------------------------

void JsonWriter::ArrayBegin( void )
{
    Comma();
    Put( '[' );
    if( m_Depth < 32 ) m_Items[ m_Depth ] = 0;
    m_Depth++;
}

//------------------------------------------------------------------------------

void JsonWriter::ArrayEnd( void )
{
    if( m_Depth > 0 ) m_Depth--;
    Put( ']' );
}

//------------------------------------------------------------------------------

void JsonWriter::Key( const char* pKey )
{
    Comma();
    PutEscaped( pKey );
    Put( ':' );
    m_AfterKey = true;
}

//------------------------------------------------------------------------------

void JsonWriter::Str( const char* pValue ) { Comma(); PutEscaped( pValue ); }

//------------------------------------------------------------------------------

void JsonWriter::Num( f64 Value )
{
    Comma();
    char Tmp[64];
    snprintf( Tmp, sizeof( Tmp ), "%.4f", Value );
    Put( Tmp );
}

//------------------------------------------------------------------------------

void JsonWriter::Int( s32 Value )
{
    Comma();
    char Tmp[32];
    snprintf( Tmp, sizeof( Tmp ), "%d", Value );
    Put( Tmp );
}

//------------------------------------------------------------------------------

void JsonWriter::Bool( bool Value )
{
    Comma();
    Put( Value ? "true" : "false" );
}

//------------------------------------------------------------------------------

void JsonWriter::Pair( const char* pKey, const char* pValue ) { Key( pKey ); Str ( pValue ); }
void JsonWriter::Pair( const char* pKey, f64 Value )          { Key( pKey ); Num ( Value ); }
void JsonWriter::Pair( const char* pKey, s32 Value )          { Key( pKey ); Int ( Value ); }
void JsonWriter::Pair( const char* pKey, bool Value )         { Key( pKey ); Bool( Value ); }

//==============================================================================
//  Json
//==============================================================================

Json::Json()
{
    m_pText      = nullptr;
    m_TextLength = 0;
    m_NodeCount  = 0;
    m_Root       = -1;
    m_Error[0]   = 0;
}

//------------------------------------------------------------------------------

void Json::Fail( const char* pMsg )
{
    if( m_Error[0] == 0 )
        snprintf( m_Error, sizeof( m_Error ), "%s", pMsg );
}

//------------------------------------------------------------------------------

s32 Json::NewNode( void )
{
    if( m_NodeCount >= A51_JSON_MAX_NODES )
    {
        Fail( "too many nodes" );
        return -1;
    }
    JsonNode& N   = m_Nodes[ m_NodeCount ];
    N.Type        = JSON_NULL;
    N.Number      = 0.0;
    N.StringOffset= -1;
    N.StringLength= 0;
    N.Child       = -1;
    N.Next        = -1;
    N.KeyOffset   = -1;
    N.KeyLength   = 0;
    return m_NodeCount++;
}

//------------------------------------------------------------------------------

void Json::SkipSpace( const char*& p )
{
    while( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) p++;
}

//------------------------------------------------------------------------------

bool Json::Parse( const char* pText )
{
    m_NodeCount = 0;
    m_Root      = -1;
    m_Error[0]  = 0;
    m_pText     = pText;

    if( !pText ) { Fail( "null input" ); return false; }
    m_TextLength = (s32)strlen( pText );
    if( m_TextLength > A51_JSON_MAX_TEXT ) { Fail( "input too large" ); return false; }

    const char* p = pText;
    SkipSpace( p );

    m_Root = ParseValue( p );
    if( m_Root < 0 ) return false;

    SkipSpace( p );
    if( *p != 0 ) { Fail( "trailing garbage" ); return false; }

    return true;
}

//------------------------------------------------------------------------------

s32 Json::ParseValue( const char*& p )
{
    SkipSpace( p );

    switch( *p )
    {
        case '{': return ParseObject( p );
        case '[': return ParseArray( p );
        case '"':
        {
            s32 Offset, Length;
            s32 Node = NewNode();
            if( Node < 0 ) return -1;
            if( ParseString( p, Offset, Length ) < 0 ) return -1;
            m_Nodes[Node].Type         = JSON_STRING;
            m_Nodes[Node].StringOffset = Offset;
            m_Nodes[Node].StringLength = Length;
            return Node;
        }
        case 't':
            if( strncmp( p, "true", 4 ) == 0 )
            {
                s32 Node = NewNode(); if( Node < 0 ) return -1;
                m_Nodes[Node].Type = JSON_BOOL; m_Nodes[Node].Number = 1.0;
                p += 4; return Node;
            }
            Fail( "bad literal" ); return -1;
        case 'f':
            if( strncmp( p, "false", 5 ) == 0 )
            {
                s32 Node = NewNode(); if( Node < 0 ) return -1;
                m_Nodes[Node].Type = JSON_BOOL; m_Nodes[Node].Number = 0.0;
                p += 5; return Node;
            }
            Fail( "bad literal" ); return -1;
        case 'n':
            if( strncmp( p, "null", 4 ) == 0 )
            {
                s32 Node = NewNode(); if( Node < 0 ) return -1;
                m_Nodes[Node].Type = JSON_NULL;
                p += 4; return Node;
            }
            Fail( "bad literal" ); return -1;
        default:
            if( *p == '-' || ( *p >= '0' && *p <= '9' ) )
                return ParseNumber( p );
            Fail( "unexpected character" );
            return -1;
    }
}

//------------------------------------------------------------------------------

s32 Json::ParseString( const char*& p, s32& Offset, s32& Length )
{
    if( *p != '"' ) { Fail( "expected string" ); return -1; }
    p++;
    Offset = (s32)( p - m_pText );

    while( *p && *p != '"' )
    {
        if( *p == '\\' )
        {
            p++;
            if( !*p ) break;
            if( *p == 'u' ) { p += 4; if( !*p ) break; }   // escapes are decoded on copy
        }
        p++;
    }
    if( *p != '"' ) { Fail( "unterminated string" ); return -1; }

    Length = (s32)( p - m_pText ) - Offset;
    p++;
    return 0;
}

//------------------------------------------------------------------------------

s32 Json::ParseNumber( const char*& p )
{
    const char* pStart = p;
    if( *p == '-' || *p == '+' ) p++;

    // At least one digit is required, otherwise "-" or "." would parse as 0.
    s32 Digits = 0;
    while( *p >= '0' && *p <= '9' ) { p++; Digits++; }
    if( *p == '.' ) { p++; while( *p >= '0' && *p <= '9' ) { p++; Digits++; } }
    if( Digits == 0 ) { Fail( "bad number" ); return -1; }
    if( *p == 'e' || *p == 'E' )
    {
        p++;
        if( *p == '+' || *p == '-' ) p++;
        s32 ExpDigits = 0;
        while( *p >= '0' && *p <= '9' ) { p++; ExpDigits++; }
        if( ExpDigits == 0 ) { Fail( "bad exponent" ); return -1; }
    }

    s32 Node = NewNode();
    if( Node < 0 ) return -1;

    char Tmp[64];
    s32  Len = (s32)( p - pStart );
    if( Len <= 0 || Len >= (s32)sizeof( Tmp ) ) { Fail( "bad number" ); return -1; }

    memcpy( Tmp, pStart, (size_t)Len );
    Tmp[Len] = 0;

    m_Nodes[Node].Type   = JSON_NUMBER;
    m_Nodes[Node].Number = atof( Tmp );
    return Node;
}

//------------------------------------------------------------------------------

s32 Json::ParseObject( const char*& p )
{
    s32 Node = NewNode();
    if( Node < 0 ) return -1;
    m_Nodes[Node].Type = JSON_OBJECT;

    p++;    // '{'
    SkipSpace( p );

    s32 LastChild = -1;

    if( *p == '}' ) { p++; return Node; }

    for( ;; )
    {
        SkipSpace( p );

        s32 KeyOffset, KeyLength;
        if( ParseString( p, KeyOffset, KeyLength ) < 0 ) return -1;

        SkipSpace( p );
        if( *p != ':' ) { Fail( "expected ':'" ); return -1; }
        p++;

        s32 Child = ParseValue( p );
        if( Child < 0 ) return -1;

        m_Nodes[ Child ].KeyOffset = KeyOffset;
        m_Nodes[ Child ].KeyLength = KeyLength;

        if( LastChild < 0 ) m_Nodes[ Node ].Child = Child;
        else                m_Nodes[ LastChild ].Next = Child;
        LastChild = Child;

        SkipSpace( p );
        if( *p == ',' ) { p++; continue; }
        if( *p == '}' ) { p++; break; }
        Fail( "expected ',' or '}'" );
        return -1;
    }

    return Node;
}

//------------------------------------------------------------------------------

s32 Json::ParseArray( const char*& p )
{
    s32 Node = NewNode();
    if( Node < 0 ) return -1;
    m_Nodes[Node].Type = JSON_ARRAY;

    p++;    // '['
    SkipSpace( p );

    s32 LastChild = -1;

    if( *p == ']' ) { p++; return Node; }

    for( ;; )
    {
        s32 Child = ParseValue( p );
        if( Child < 0 ) return -1;

        if( LastChild < 0 ) m_Nodes[ Node ].Child = Child;
        else                m_Nodes[ LastChild ].Next = Child;
        LastChild = Child;

        SkipSpace( p );
        if( *p == ',' ) { p++; continue; }
        if( *p == ']' ) { p++; break; }
        Fail( "expected ',' or ']'" );
        return -1;
    }

    return Node;
}

//==============================================================================
//  Navigation
//==============================================================================

static bool key_equals( const char* pText, const JsonNode& N, const char* pKey )
{
    if( N.KeyOffset < 0 ) return false;
    s32 Len = (s32)strlen( pKey );
    if( N.KeyLength != Len ) return false;
    return strncmp( pText + N.KeyOffset, pKey, (size_t)Len ) == 0;
}

//------------------------------------------------------------------------------

s32 Json::Find( s32 Node, const char* pKey ) const
{
    if( Node < 0 || Node >= m_NodeCount || !pKey ) return -1;
    if( m_Nodes[Node].Type != JSON_OBJECT ) return -1;

    for( s32 i = m_Nodes[Node].Child; i >= 0; i = m_Nodes[i].Next )
        if( key_equals( m_pText, m_Nodes[i], pKey ) )
            return i;

    return -1;
}

//------------------------------------------------------------------------------

s32 Json::At( s32 Node, s32 Index ) const
{
    if( Node < 0 || Node >= m_NodeCount ) return -1;
    s32 i = 0;
    for( s32 C = m_Nodes[Node].Child; C >= 0; C = m_Nodes[C].Next, i++ )
        if( i == Index ) return C;
    return -1;
}

//------------------------------------------------------------------------------

s32 Json::Count( s32 Node ) const
{
    if( Node < 0 || Node >= m_NodeCount ) return 0;
    s32 n = 0;
    for( s32 C = m_Nodes[Node].Child; C >= 0; C = m_Nodes[C].Next ) n++;
    return n;
}

//==============================================================================
//  Accessors
//==============================================================================

s32 Json::CopyStr( s32 Node, char* pOut, s32 OutSize ) const
{
    if( !pOut || OutSize <= 0 ) return 0;
    pOut[0] = 0;

    if( Node < 0 || Node >= m_NodeCount ) return 0;
    const JsonNode& N = m_Nodes[Node];
    if( N.Type != JSON_STRING || N.StringOffset < 0 ) return 0;

    const char* pSrc = m_pText + N.StringOffset;
    const char* pEnd = pSrc + N.StringLength;
    s32         i    = 0;

    while( pSrc < pEnd && i < OutSize - 1 )
    {
        char c = *pSrc++;
        if( c != '\\' ) { pOut[i++] = c; continue; }
        if( pSrc >= pEnd ) break;

        char e = *pSrc++;
        switch( e )
        {
            case 'n':  pOut[i++] = '\n'; break;
            case 't':  pOut[i++] = '\t'; break;
            case 'r':  pOut[i++] = '\r'; break;
            case 'b':  pOut[i++] = '\b'; break;
            case 'f':  pOut[i++] = '\f'; break;
            case '"':  pOut[i++] = '"';  break;
            case '\\': pOut[i++] = '\\'; break;
            case '/':  pOut[i++] = '/';  break;
            case 'u':
            {
                if( pSrc + 4 > pEnd ) { pSrc = pEnd; break; }
                u32 Code = 0;
                for( s32 k = 0; k < 4; k++ )
                {
                    char h = pSrc[k];
                    Code <<= 4;
                    if( h >= '0' && h <= '9' )      Code |= (u32)( h - '0' );
                    else if( h >= 'a' && h <= 'f' ) Code |= (u32)( h - 'a' + 10 );
                    else if( h >= 'A' && h <= 'F' ) Code |= (u32)( h - 'A' + 10 );
                }
                pSrc += 4;

                // Encode as UTF-8.
                if( Code < 0x80 )
                {
                    if( i < OutSize - 1 ) pOut[i++] = (char)Code;
                }
                else if( Code < 0x800 )
                {
                    if( i < OutSize - 2 )
                    {
                        pOut[i++] = (char)( 0xC0 | ( Code >> 6 ) );
                        pOut[i++] = (char)( 0x80 | ( Code & 0x3F ) );
                    }
                }
                else
                {
                    if( i < OutSize - 3 )
                    {
                        pOut[i++] = (char)( 0xE0 | ( Code >> 12 ) );
                        pOut[i++] = (char)( 0x80 | ( ( Code >> 6 ) & 0x3F ) );
                        pOut[i++] = (char)( 0x80 | ( Code & 0x3F ) );
                    }
                }
                break;
            }
            default: pOut[i++] = e; break;
        }
    }

    pOut[i] = 0;
    return i;
}

//------------------------------------------------------------------------------

const char* Json::Str( s32 Node, const char* pDefault ) const
{
    // Returning a pointer into the source text is only safe for unescaped
    // strings, which is what the app writes, so callers that need escapes use
    // CopyStr() instead.
    if( Node < 0 || Node >= m_NodeCount ) return pDefault;

    const JsonNode& N = m_Nodes[Node];
    if( N.Type != JSON_STRING ) return pDefault;
    const char* p = m_pText + N.StringOffset;
    for( s32 i = 0; i < N.StringLength; i++ )
        if( p[i] == '\\' ) return pDefault;

    // NUL terminate in place is not possible on const text, so hand back a
    // pointer to a small rotating set of buffers.
    static char  s_Bufs[4][256];
    static s32   s_Next = 0;
    char* pBuf = s_Bufs[ s_Next ];
    s_Next = ( s_Next + 1 ) & 3;

    s32 Len = N.StringLength < 255 ? N.StringLength : 255;
    memcpy( pBuf, p, (size_t)Len );
    pBuf[Len] = 0;
    return pBuf;
}

//------------------------------------------------------------------------------

const char* Json::GetStr( s32 Node, const char* pKey, const char* pDefault ) const
{
    return Str( Find( Node, pKey ), pDefault );
}

//------------------------------------------------------------------------------

f64 Json::Num( s32 Node, f64 Default ) const
{
    if( Node < 0 || Node >= m_NodeCount ) return Default;
    if( m_Nodes[Node].Type != JSON_NUMBER && m_Nodes[Node].Type != JSON_BOOL ) return Default;
    return m_Nodes[Node].Number;
}

//------------------------------------------------------------------------------

bool Json::Bool( s32 Node, bool Default ) const
{
    if( Node < 0 || Node >= m_NodeCount ) return Default;
    if( m_Nodes[Node].Type == JSON_BOOL || m_Nodes[Node].Type == JSON_NUMBER )
        return m_Nodes[Node].Number != 0.0;
    return Default;
}

//------------------------------------------------------------------------------

f64 Json::GetNum( s32 Node, const char* pKey, f64 Default ) const
{
    return Num( Find( Node, pKey ), Default );
}

//------------------------------------------------------------------------------

s32 Json::GetInt( s32 Node, const char* pKey, s32 Default ) const
{
    return (s32)GetNum( Node, pKey, (f64)Default );
}

//------------------------------------------------------------------------------

bool Json::GetBool( s32 Node, const char* pKey, bool Default ) const
{
    return Bool( Find( Node, pKey ), Default );
}

} // namespace a51
//==============================================================================
