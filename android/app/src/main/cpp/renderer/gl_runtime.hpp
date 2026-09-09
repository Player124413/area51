//==============================================================================
//
//  gl_runtime.hpp
//
//  The GLES 3 renderer.
//
//  The scene is drawn into an offscreen colour buffer whose size is
//  renderScale * screen size and then upscaled to the window with a bilinear
//  blit.  That is what makes the dynamic resolution work: on a phone that is
//  fill rate limited (which is nearly all of them) halving the scale buys a
//  factor of four in fragment work for the price of a slightly softer image.
//
//  Everything is drawn from two tiny shader programs and two vertex buffers -
//  no textures to upload, no state to restore, no allocations per frame.
//
//==============================================================================

#ifndef A51_GL_RUNTIME_HPP
#define A51_GL_RUNTIME_HPP

#include "../a51/a51_types.hpp"

namespace a51 {

class GlRuntime
{
public:
                 GlRuntime();
                ~GlRuntime();

    // Must be called on the GL thread with a current context.
    bool         Init           ( s32 Width, s32 Height );
    void         Shutdown       ( void );
    bool         IsReady        ( void ) const { return m_Ready; }

    void         Resize         ( s32 Width, s32 Height );
    void         SetRenderScale ( f32 Scale );
    f32          GetRenderScale ( void ) const { return m_RenderScale; }

    s32          GetBackWidth   ( void ) const { return m_BackWidth; }
    s32          GetBackHeight  ( void ) const { return m_BackHeight; }

    // Quality 0 (cheapest) .. 3 (everything on).
    void         DrawFrame      ( f32 TimeSec, f32 DtSec, s32 Quality );

    const char*  GetError       ( void ) const { return m_Error; }
    u32          GetDrawCalls   ( void ) const { return m_DrawCalls; }
    u32          GetVertexCount ( void ) const { return m_VertexCount; }

private:
                 GlRuntime( const GlRuntime& );
    GlRuntime&   operator = ( const GlRuntime& );

    bool         CompilePrograms( void );
    u32          CompileShader  ( u32 Type, const char* pSource );
    u32          LinkProgram    ( u32 Vert, u32 Frag );
    bool         CreateBackBuffer( void );
    void         DestroyBackBuffer( void );
    void         Fail           ( const char* pMsg );
    bool         CheckGl        ( const char* pWhere );

    bool         m_Ready;

    s32          m_Width;
    s32          m_Height;
    f32          m_RenderScale;
    s32          m_BackWidth;
    s32          m_BackHeight;

    u32          m_SceneProgram;
    u32          mBlitProgram;

    s32          m_LocSceneResolution;
    s32          m_LocSceneTime;
    s32          m_LocSceneIsPoint;
    s32          m_LocBlitTexture;

    u32          m_Fbo;
    u32          m_ColorTexture;

    u32          m_SceneVao;
    u32          m_SceneVbo;
    u32          m_BlitVao;
    u32          m_BlitVbo;

    u32          m_DrawCalls;
    u32          m_VertexCount;

    char         m_Error[ 192 ];
};

} // namespace a51

#endif // A51_GL_RUNTIME_HPP
//==============================================================================
