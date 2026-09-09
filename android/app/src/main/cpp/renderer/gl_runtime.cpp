//==============================================================================
//
//  gl_runtime.cpp
//
//==============================================================================

#include "gl_runtime.hpp"
#include "../a51/log.hpp"

#include <GLES3/gl3.h>

#include <cstdio>
#include <cstring>
#include <cmath>

namespace a51 {

//==============================================================================
//  Shaders
//==============================================================================

static const char* const SCENE_VERT =
    "#version 300 es\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in float aSize;\n"
    "uniform vec2 uResolution;\n"
    "out vec4 vColor;\n"
    "void main()\n"
    "{\n"
    "    vec2 p = aPos * 2.0 - 1.0;\n"
    "    p.y = -p.y;\n"
    "    gl_Position  = vec4( p, 0.0, 1.0 );\n"
    "    gl_PointSize = aSize * ( uResolution.y / 720.0 );\n"
    "    vColor       = aColor;\n"
    "}\n";

static const char* const SCENE_FRAG =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 vColor;\n"
    "uniform int uIsPoint;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    if( uIsPoint == 1 )\n"
    "    {\n"
    "        vec2  d = gl_PointCoord - vec2( 0.5 );\n"
    "        float r = length( d ) * 2.0;\n"
    "        if( r > 1.0 ) discard;\n"
    "        fragColor = vec4( vColor.rgb, vColor.a * ( 1.0 - r * r ) );\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        fragColor = vColor;\n"
    "    }\n"
    "}\n";

static const char* const BLIT_VERT =
    "#version 300 es\n"
    "layout(location = 0) in vec2 aPos;\n"
    "out vec2 vUv;\n"
    "void main()\n"
    "{\n"
    "    vUv         = aPos * 0.5 + 0.5;\n"
    "    gl_Position = vec4( aPos, 0.0, 1.0 );\n"
    "}\n";

static const char* const BLIT_FRAG =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vUv;\n"
    "uniform sampler2D uTexture;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    vec3  c = texture( uTexture, vUv ).rgb;\n"
    "    vec2  d = vUv - 0.5;\n"
    "    float v = 1.0 - dot( d, d ) * 0.55;\n"
    "    fragColor = vec4( c * v, 1.0 );\n"
    "}\n";

//==============================================================================
//  Vertex plumbing
//==============================================================================

static const s32 FLOATS_PER_VERT = 7;      // x, y, r, g, b, a, size
static const s32 MAX_VERTS       = 4096;

static f32 s_Vertices[ MAX_VERTS * FLOATS_PER_VERT ];

static s32 s_VertCount;

static void vert_begin( void )
{
    s_VertCount = 0;
}

static void vert_push( f32 x, f32 y, f32 r, f32 g, f32 b, f32 a, f32 size )
{
    if( s_VertCount >= MAX_VERTS ) return;

    f32* p = s_Vertices + s_VertCount * FLOATS_PER_VERT;
    p[0] = x;    p[1] = y;
    p[2] = r;    p[3] = g;    p[4] = b;    p[5] = a;
    p[6] = size;
    s_VertCount++;
}

// Deterministic pseudo random in [0,1) - no rand() state to worry about.
static f32 hash01( s32 i )
{
    u32 x = (u32)i * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return (f32)( x & 0xFFFFFF ) / 16777216.0f;
}

//==============================================================================
//  GlRuntime
//==============================================================================

GlRuntime::GlRuntime()
{
    m_Ready           = false;
    m_Width           = 1;
    m_Height          = 1;
    m_RenderScale     = 1.0f;
    m_BackWidth       = 1;
    m_BackHeight      = 1;
    m_SceneProgram    = 0;
    mBlitProgram      = 0;
    m_LocSceneResolution = -1;
    m_LocSceneTime    = -1;
    m_LocSceneIsPoint = -1;
    m_LocBlitTexture  = -1;
    m_Fbo             = 0;
    m_ColorTexture    = 0;
    m_SceneVao        = 0;
    m_SceneVbo        = 0;
    m_BlitVao         = 0;
    m_BlitVbo         = 0;
    m_DrawCalls       = 0;
    m_VertexCount     = 0;
    m_Error[0]        = 0;
}

//------------------------------------------------------------------------------

GlRuntime::~GlRuntime()
{
    Shutdown();
}

//------------------------------------------------------------------------------

void GlRuntime::Fail( const char* pMsg )
{
    snprintf( m_Error, sizeof( m_Error ), "%s", pMsg );
    A51_LOGE( "gl: %s", pMsg );
}

//------------------------------------------------------------------------------

bool GlRuntime::CheckGl( const char* pWhere )
{
    GLenum Err = glGetError();
    if( Err == GL_NO_ERROR ) return true;

    char Msg[192];
    snprintf( Msg, sizeof( Msg ), "GL error 0x%04X in %s", (unsigned)Err, pWhere );
    Fail( Msg );
    return false;
}

//==============================================================================
//  Shaders
//==============================================================================

u32 GlRuntime::CompileShader( u32 Type, const char* pSource )
{
    GLuint Shader = glCreateShader( Type );
    if( Shader == 0 ) { Fail( "glCreateShader failed" ); return 0; }

    glShaderSource( Shader, 1, &pSource, nullptr );
    glCompileShader( Shader );

    GLint Ok = 0;
    glGetShaderiv( Shader, GL_COMPILE_STATUS, &Ok );
    if( !Ok )
    {
        char  Log[ 512 ];
        GLint Len = 0;
        glGetShaderInfoLog( Shader, sizeof( Log ), &Len, Log );
        char Msg[ 640 ];
        snprintf( Msg, sizeof( Msg ), "shader compile: %s", Log );
        Fail( Msg );
        glDeleteShader( Shader );
        return 0;
    }
    return (u32)Shader;
}

//------------------------------------------------------------------------------

u32 GlRuntime::LinkProgram( u32 Vert, u32 Frag )
{
    GLuint Program = glCreateProgram();
    if( Program == 0 ) { Fail( "glCreateProgram failed" ); return 0; }

    glAttachShader( Program, Vert );
    glAttachShader( Program, Frag );
    glLinkProgram( Program );

    GLint Ok = 0;
    glGetProgramiv( Program, GL_LINK_STATUS, &Ok );
    if( !Ok )
    {
        char  Log[ 512 ];
        GLint Len = 0;
        glGetProgramInfoLog( Program, sizeof( Log ), &Len, Log );
        char Msg[ 640 ];
        snprintf( Msg, sizeof( Msg ), "program link: %s", Log );
        Fail( Msg );
        glDeleteProgram( Program );
        return 0;
    }

    glDetachShader( Program, Vert );
    glDetachShader( Program, Frag );
    return (u32)Program;
}

//------------------------------------------------------------------------------

bool GlRuntime::CompilePrograms( void )
{
    u32 SceneVert = CompileShader( GL_VERTEX_SHADER,   SCENE_VERT );
    u32 SceneFrag = CompileShader( GL_FRAGMENT_SHADER, SCENE_FRAG );
    if( SceneVert == 0 || SceneFrag == 0 ) return false;

    m_SceneProgram = LinkProgram( SceneVert, SceneFrag );
    glDeleteShader( SceneVert );
    glDeleteShader( SceneFrag );
    if( m_SceneProgram == 0 ) return false;

    u32 BlitVert = CompileShader( GL_VERTEX_SHADER,   BLIT_VERT );
    u32 BlitFrag = CompileShader( GL_FRAGMENT_SHADER, BLIT_FRAG );
    if( BlitVert == 0 || BlitFrag == 0 ) return false;

    mBlitProgram = LinkProgram( BlitVert, BlitFrag );
    glDeleteShader( BlitVert );
    glDeleteShader( BlitFrag );
    if( mBlitProgram == 0 ) return false;

    m_LocSceneResolution = glGetUniformLocation( m_SceneProgram, "uResolution" );
    m_LocSceneTime       = glGetUniformLocation( m_SceneProgram, "uTime" );
    m_LocSceneIsPoint    = glGetUniformLocation( m_SceneProgram, "uIsPoint" );
    m_LocBlitTexture     = glGetUniformLocation( mBlitProgram,  "uTexture" );

    return true;
}

//==============================================================================
//  Back buffer (the thing the render scale actually scales)
//==============================================================================

bool GlRuntime::CreateBackBuffer( void )
{
    DestroyBackBuffer();

    s32 W = (s32)( (f32)m_Width  * m_RenderScale + 0.5f );
    s32 H = (s32)( (f32)m_Height * m_RenderScale + 0.5f );
    if( W < 16 ) W = 16;
    if( H < 16 ) H = 16;
    if( W > 8192 ) W = 8192;
    if( H > 8192 ) H = 8192;

    m_BackWidth  = W;
    m_BackHeight = H;

    glGenTextures( 1, &m_ColorTexture );
    glBindTexture( GL_TEXTURE_2D, m_ColorTexture );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

    glGenFramebuffers( 1, &m_Fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, m_Fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorTexture, 0 );

    GLenum Status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

    if( Status != GL_FRAMEBUFFER_COMPLETE )
    {
        char Msg[160];
        snprintf( Msg, sizeof( Msg ), "framebuffer incomplete: 0x%04X", (unsigned)Status );
        Fail( Msg );
        DestroyBackBuffer();
        return false;
    }

    return CheckGl( "CreateBackBuffer" );
}

//------------------------------------------------------------------------------

void GlRuntime::DestroyBackBuffer( void )
{
    if( m_Fbo )          { glDeleteFramebuffers( 1, &m_Fbo );          m_Fbo          = 0; }
    if( m_ColorTexture ) { glDeleteTextures( 1, &m_ColorTexture );     m_ColorTexture = 0; }
}

//==============================================================================
//  Init / Shutdown
//==============================================================================

bool GlRuntime::Init( s32 Width, s32 Height )
{
    if( m_Ready ) return true;

    m_Width  = ( Width  > 0 ) ? Width  : 1;
    m_Height = ( Height > 0 ) ? Height : 1;

    if( !CompilePrograms() ) return false;

    // Scene geometry buffer.
    glGenVertexArrays( 1, &m_SceneVao );
    glGenBuffers( 1, &m_SceneVbo );
    glBindVertexArray( m_SceneVao );
    glBindBuffer( GL_ARRAY_BUFFER, m_SceneVbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof( f32 ) * FLOATS_PER_VERT * MAX_VERTS, nullptr, GL_DYNAMIC_DRAW );

    const GLsizei Stride = sizeof( f32 ) * FLOATS_PER_VERT;
    glEnableVertexAttribArray( 0 );
    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, Stride, (void*)( 0 * sizeof( f32 ) ) );
    glEnableVertexAttribArray( 1 );
    glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, Stride, (void*)( 2 * sizeof( f32 ) ) );
    glEnableVertexAttribArray( 2 );
    glVertexAttribPointer( 2, 1, GL_FLOAT, GL_FALSE, Stride, (void*)( 6 * sizeof( f32 ) ) );

    // Blit triangle.
    static const f32 s_Quad[ 6 ] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };
    glGenVertexArrays( 1, &m_BlitVao );
    glGenBuffers( 1, &m_BlitVbo );
    glBindVertexArray( m_BlitVao );
    glBindBuffer( GL_ARRAY_BUFFER, m_BlitVbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof( s_Quad ), s_Quad, GL_STATIC_DRAW );
    glEnableVertexAttribArray( 0 );
    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof( f32 ), (void*)0 );

    glBindVertexArray( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    if( !CreateBackBuffer() ) return false;

    m_Ready = true;
    A51_LOGI( "gl: ready, back buffer %dx%d (scale %.2f)", m_BackWidth, m_BackHeight, m_RenderScale );
    return CheckGl( "Init" );
}

//------------------------------------------------------------------------------

void GlRuntime::Shutdown( void )
{
    if( !m_Ready && m_SceneProgram == 0 ) return;

    DestroyBackBuffer();

    if( m_SceneVbo ) { glDeleteBuffers( 1, &m_SceneVbo ); m_SceneVbo = 0; }
    if( m_SceneVao ) { glDeleteVertexArrays( 1, &m_SceneVao ); m_SceneVao = 0; }
    if( m_BlitVbo )  { glDeleteBuffers( 1, &m_BlitVbo );  m_BlitVbo  = 0; }
    if( m_BlitVao )  { glDeleteVertexArrays( 1, &m_BlitVao );  m_BlitVao  = 0; }
    if( m_SceneProgram ) { glDeleteProgram( m_SceneProgram ); m_SceneProgram = 0; }
    if( mBlitProgram )   { glDeleteProgram( mBlitProgram );   mBlitProgram   = 0; }

    m_Ready = false;
}

//------------------------------------------------------------------------------

void GlRuntime::Resize( s32 Width, s32 Height )
{
    if( Width  < 1 ) Width  = 1;
    if( Height < 1 ) Height = 1;
    if( Width == m_Width && Height == m_Height ) return;

    m_Width  = Width;
    m_Height = Height;

    if( m_Ready ) CreateBackBuffer();
}

//------------------------------------------------------------------------------

void GlRuntime::SetRenderScale( f32 Scale )
{
    Scale = a51_clamp( Scale, 0.20f, 1.00f );

    // Ignore tiny changes: recreating the back buffer costs a frame.
    if( fabsf( Scale - m_RenderScale ) < 0.02f ) return;

    m_RenderScale = Scale;
    if( m_Ready ) CreateBackBuffer();
}

//==============================================================================
//  Drawing
//==============================================================================

void GlRuntime::DrawFrame( f32 TimeSec, f32 DtSec, s32 Quality )
{
    (void)DtSec;

    if( !m_Ready ) return;

    Quality = a51_clamp( Quality, 0, 3 );

    m_DrawCalls   = 0;
    m_VertexCount = 0;

    //--------------------------------------------------------------------------
    //  Scene -> back buffer
    //--------------------------------------------------------------------------
    glBindFramebuffer( GL_FRAMEBUFFER, m_Fbo );
    glViewport( 0, 0, m_BackWidth, m_BackHeight );

    glClearColor( 0.016f, 0.024f, 0.047f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE );        // additive: everything glows

    glUseProgram( m_SceneProgram );
    glUniform2f( m_LocSceneResolution, (f32)m_BackWidth, (f32)m_BackHeight );
    glUniform1f( m_LocSceneTime, TimeSec );

    glBindVertexArray( m_SceneVao );
    glBindBuffer( GL_ARRAY_BUFFER, m_SceneVbo );

    vert_begin();

    const f32 Horizon = 0.42f;

    // ---- perspective floor grid ------------------------------------------
    const s32 Verticals   = 10 + Quality * 6;
    const s32 Horizontals =  8 + Quality * 4;
    const f32 Scroll      = fmodf( TimeSec * 0.18f, 1.0f );

    for( s32 i = 0; i <= Horizontals; i++ )
    {
        f32 t  = ( (f32)i + Scroll ) / (f32)( Horizontals + 1 );
        if( t > 1.0f ) t -= 1.0f;

        f32 y     = Horizon + ( 1.0f - Horizon ) * ( t * t );
        f32 Alpha = 0.10f + 0.35f * t;

        vert_push( 0.0f, y, 0.20f, 0.75f, 0.95f, Alpha, 1.0f );
        vert_push( 1.0f, y, 0.20f, 0.75f, 0.95f, Alpha, 1.0f );
    }

    for( s32 j = -Verticals; j <= Verticals; j++ )
    {
        f32 Spread = (f32)j / (f32)Verticals;
        f32 x0     = 0.5f + Spread * 0.02f;
        f32 x1     = 0.5f + Spread * 1.80f;
        f32 Alpha  = 0.10f + 0.22f * ( 1.0f - fabsf( Spread ) );

        vert_push( x0, Horizon, 0.35f, 0.55f, 0.90f, Alpha, 1.0f );
        vert_push( x1, 1.0f,    0.35f, 0.55f, 0.90f, Alpha, 1.0f );
    }

    // ---- horizon glow -----------------------------------------------------
    vert_push( 0.0f, Horizon - 0.004f, 0.95f, 0.55f, 0.15f, 0.55f, 1.0f );
    vert_push( 1.0f, Horizon - 0.004f, 0.95f, 0.55f, 0.15f, 0.55f, 1.0f );
    vert_push( 0.0f, Horizon + 0.004f, 0.95f, 0.35f, 0.10f, 0.25f, 1.0f );
    vert_push( 1.0f, Horizon + 0.004f, 0.95f, 0.35f, 0.10f, 0.25f, 1.0f );

    s32 LineVerts = s_VertCount;

    // ---- starfield --------------------------------------------------------
    const s32 Stars = 60 + Quality * 110;
    for( s32 i = 0; i < Stars; i++ )
    {
        f32 x      = hash01( i * 3 + 1 );
        f32 Base   = hash01( i * 3 + 2 );
        f32 Speed  = 0.01f + hash01( i * 3 + 3 ) * 0.05f;
        f32 y      = fmodf( Base + TimeSec * Speed, 1.0f ) * Horizon;
        f32 Size   = 1.0f + hash01( i * 7 + 5 ) * 3.0f;
        f32 Twinkle= 0.35f + 0.65f * ( 0.5f + 0.5f * sinf( TimeSec * 2.0f + (f32)i ) );

        vert_push( x, y, 0.80f, 0.88f, 1.0f, 0.25f + 0.55f * Twinkle, Size );
    }

    s32 PointVerts = s_VertCount - LineVerts;

    glBufferData( GL_ARRAY_BUFFER,
                  sizeof( f32 ) * FLOATS_PER_VERT * s_VertCount,
                  s_Vertices,
                  GL_DYNAMIC_DRAW );

    glUniform1i( m_LocSceneIsPoint, 0 );
    glDrawArrays( GL_LINES, 0, LineVerts );
    m_DrawCalls++;

    if( PointVerts > 0 )
    {
        glUniform1i( m_LocSceneIsPoint, 1 );
        glDrawArrays( GL_POINTS, LineVerts, PointVerts );
        m_DrawCalls++;
    }

    m_VertexCount = (u32)s_VertCount;

    //--------------------------------------------------------------------------
    //  Back buffer -> screen
    //--------------------------------------------------------------------------
    glDisable( GL_BLEND );

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glViewport( 0, 0, m_Width, m_Height );

    glUseProgram( mBlitProgram );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, m_ColorTexture );
    glUniform1i( m_LocBlitTexture, 0 );

    glBindVertexArray( m_BlitVao );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    m_DrawCalls++;

    glBindVertexArray( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    CheckGl( "DrawFrame" );
}

} // namespace a51
//==============================================================================
