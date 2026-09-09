//==============================================================================
//
//  json.hpp
//
//  A very small JSON reader/writer, just enough for the app's own config files
//  (touch layouts, profiles).  Deliberately dependency free and allocation
//  light: the writer appends into a caller supplied buffer, the parser builds
//  into a caller supplied node pool.
//
//==============================================================================

#ifndef A51_JSON_HPP
#define A51_JSON_HPP

#include "a51_types.hpp"

namespace a51 {

//==============================================================================
//  Writer
//==============================================================================

class JsonWriter
{
public:
                 JsonWriter( char* pBuffer, s32 BufferSize );

    void         ObjectBegin ( void );
    void         ObjectEnd   ( void );
    void         ArrayBegin  ( void );
    void         ArrayEnd    ( void );

    void         Key         ( const char* pKey );
    void         Str         ( const char* pValue );
    void         Num         ( f64 Value );
    void         Int         ( s32 Value );
    void         Bool        ( bool Value );

    // Key + value pairs.
    void         Pair        ( const char* pKey, const char* pValue );
    void         Pair        ( const char* pKey, f64 Value );
    void         Pair        ( const char* pKey, s32 Value );
    void         Pair        ( const char* pKey, bool Value );

    bool         Overflowed  ( void ) const { return m_Overflow; }
    s32          Length      ( void ) const { return m_Length; }

private:
    void         Put         ( char c );
    void         Put         ( const char* pStr );
    void         Comma       ( void );
    void         PutEscaped  ( const char* pStr );

    char*        m_pBuffer;
    s32          m_Size;
    s32          m_Length;
    s32          m_Depth;
    s32          m_Items[ 32 ];      // item count per level
    bool         m_AfterKey;
    bool         m_Overflow;
};

//==============================================================================
//  Reader
//==============================================================================

enum EJsonType
{
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

struct JsonNode
{
    EJsonType    Type;
    f64          Number;
    s32          StringOffset;     // into the source text
    s32          StringLength;
    s32          Child;            // first child, -1 = none
    s32          Next;             // next sibling, -1 = none
    s32          KeyOffset;        // object key, -1 = none
    s32          KeyLength;
};

#define A51_JSON_MAX_NODES 1024
#define A51_JSON_MAX_TEXT  ( 256 * 1024 )

class Json
{
public:
                 Json();

    bool         Parse         ( const char* pText );
    const char*  GetError      ( void ) const { return m_Error; }

    s32          Root          ( void ) const { return m_Root; }

    // Navigation.  Returns -1 when the member is missing.
    s32          Find          ( s32 Node, const char* pKey ) const;
    s32          At            ( s32 Node, s32 Index ) const;
    s32          Count         ( s32 Node ) const;

    // Accessors with defaults so a missing/renamed field never breaks a load.
    const char*  GetStr        ( s32 Node, const char* pKey, const char* pDefault ) const;
    // Same, but for a node itself (array elements have no key).
    const char*  Str           ( s32 Node, const char* pDefault ) const;
    f64          Num           ( s32 Node, f64 Default ) const;
    bool         Bool          ( s32 Node, bool Default ) const;
    f64          GetNum        ( s32 Node, const char* pKey, f64 Default ) const;
    s32          GetInt        ( s32 Node, const char* pKey, s32 Default ) const;
    bool         GetBool       ( s32 Node, const char* pKey, bool Default ) const;

    // Copies a string value out (unescaped, NUL terminated).
    s32          CopyStr       ( s32 Node, char* pOut, s32 OutSize ) const;

    const JsonNode& Node       ( s32 Index ) const { return m_Nodes[ Index ]; }

private:
    s32          ParseValue    ( const char*& p );
    s32          ParseObject   ( const char*& p );
    s32          ParseArray    ( const char*& p );
    s32          ParseString   ( const char*& p, s32& Offset, s32& Length );
    s32          ParseNumber   ( const char*& p );
    void         SkipSpace     ( const char*& p );
    s32          NewNode       ( void );
    void         Fail          ( const char* pMsg );

    const char*  m_pText;
    s32          m_TextLength;
    JsonNode     m_Nodes[ A51_JSON_MAX_NODES ];
    s32          m_NodeCount;
    s32          m_Root;
    char         m_Error[ 128 ];
};

} // namespace a51

#endif // A51_JSON_HPP
//==============================================================================
