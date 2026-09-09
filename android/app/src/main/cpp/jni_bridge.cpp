//==============================================================================
//
//  jni_bridge.cpp
//
//  The only file that knows about both JNI and the runtime.  Everything is a
//  thin forward to a51::runtime() or a51::GlRuntime - no logic lives here, so
//  the interesting code stays testable on a desktop.
//
//  Java side: com.a51.android.core.NativeBridge
//
//==============================================================================

#include <jni.h>
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "a51/runtime.hpp"
#include "a51/json.hpp"
#include "a51/log.hpp"
#include "renderer/gl_runtime.hpp"

using namespace a51;

#define JNI_PREFIX Java_com_a51_android_core_NativeBridge_

//==============================================================================
//  Globals
//==============================================================================

static GlRuntime g_Gl;
static DfsArchive g_BrowseArchive;      // the archive opened by the file browser

//==============================================================================
//  Helpers
//==============================================================================

static jstring to_jstring( JNIEnv* pEnv, const char* pStr )
{
    return pEnv->NewStringUTF( pStr ? pStr : "" );
}

static const char* jstring_chars( JNIEnv* pEnv, jstring Str )
{
    return Str ? pEnv->GetStringUTFChars( Str, nullptr ) : nullptr;
}

static void jstring_release( JNIEnv* pEnv, jstring Str, const char* pChars )
{
    if( Str && pChars ) pEnv->ReleaseStringUTFChars( Str, pChars );
}

// Copies a java string into a caller supplied buffer (no leaks on any path).
static bool jstring_copy( JNIEnv* pEnv, jstring Str, char* pOut, s32 OutSize )
{
    if( !Str || !pOut || OutSize <= 0 ) return false;
    const char* pChars = pEnv->GetStringUTFChars( Str, nullptr );
    if( !pChars ) return false;
    snprintf( pOut, (size_t)OutSize, "%s", pChars );
    pEnv->ReleaseStringUTFChars( Str, pChars );
    return true;
}

static jbyteArray to_jbytes( JNIEnv* pEnv, const void* pData, s32 Length )
{
    jbyteArray Array = pEnv->NewByteArray( Length > 0 ? Length : 0 );
    if( !Array ) return nullptr;
    if( Length > 0 && pData )
        pEnv->SetByteArrayRegion( Array, 0, Length, (const jbyte*)pData );
    return Array;
}

//==============================================================================
//  Info
//==============================================================================

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeGetVersion( JNIEnv* pEnv, jclass )
{
    return to_jstring( pEnv, "1.0.0" );
}

//==============================================================================
//  Runtime / frame
//==============================================================================

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeInit( JNIEnv*, jclass )
{
    return runtime().Init() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeShutdown( JNIEnv*, jclass )
{
    runtime().Shutdown();
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetScreen( JNIEnv*, jclass, jint Width, jint Height )
{
    runtime().SetScreen( Width, Height );
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetAspect( JNIEnv*, jclass )
{
    return runtime().GetAspect();
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeBeginFrame( JNIEnv*, jclass )
{
    runtime().BeginFrame();
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeEndFrame( JNIEnv*, jclass, jfloat FrameMs, jfloat CpuMs )
{
    runtime().EndFrame( FrameMs, CpuMs );
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeSleepMs( JNIEnv*, jclass, jfloat FrameStartMs, jfloat NowMs )
{
    return runtime().SleepMs( FrameStartMs, NowMs );
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeGetLog( JNIEnv* pEnv, jclass )
{
    // One JSON array of the last log lines, for the HUD.
    static char Buffer[ 16384 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ArrayBegin();
    for( s32 i = 0; i < runtime().LogCount(); i++ )
        W.Str( runtime().LogLine( i ) );
    W.ArrayEnd();

    return to_jstring( pEnv, Buffer );
}

//==============================================================================
//  Performance
//==============================================================================

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetTargetFps( JNIEnv*, jclass, jfloat Fps )
{
    runtime().Perf().SetTargetFps( Fps );
    runtime().Pacer().SetTargetFps( runtime().Perf().GetConfig().TargetFps );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetAutoQuality( JNIEnv*, jclass, jboolean Enabled )
{
    runtime().Perf().SetAutoQuality( Enabled == JNI_TRUE );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetQualityCeiling( JNIEnv*, jclass, jint Level )
{
    runtime().Perf().SetQualityCeiling( Level );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetFixedScale( JNIEnv*, jclass, jfloat Scale )
{
    runtime().Perf().SetFixedScale( Scale );
    if( Scale > 0.0f ) g_Gl.SetRenderScale( Scale );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetThermalStatus( JNIEnv*, jclass, jint Status )
{
    runtime().Perf().SetThermalStatus( Status );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetMemoryPressure( JNIEnv*, jclass, jfloat Pressure )
{
    runtime().Perf().SetMemoryPressure( Pressure );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeConfigurePerf( JNIEnv*, jclass, jfloat TargetFps, jfloat MinScale,
                                jint QualityLevels, jint TextureBudgetMb )
{
    PerfConfig Config = runtime().Perf().GetConfig();
    Config.TargetFps       = TargetFps;
    Config.MinScale        = MinScale;
    Config.QualityLevels   = QualityLevels;
    Config.TextureBudgetMb = TextureBudgetMb;
    runtime().Perf().Configure( Config );
    runtime().Pacer().SetTargetFps( runtime().Perf().GetConfig().TargetFps );
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetRenderScale( JNIEnv*, jclass )
{
    return runtime().Perf().GetRenderScale();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetQualityLevel( JNIEnv*, jclass )
{
    return runtime().Perf().GetQualityLevel();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetFps( JNIEnv*, jclass )
{
    return runtime().Perf().GetFps();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetFrameTimeMs( JNIEnv*, jclass )
{
    return runtime().Perf().GetFrameTimeMs();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetAvgFrameMs( JNIEnv*, jclass )
{
    return runtime().Perf().GetAvgFrameMs();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetWorstFrameMs( JNIEnv*, jclass )
{
    return runtime().Perf().GetWorstFrameMs();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetP95FrameMs( JNIEnv*, jclass )
{
    return runtime().Perf().GetFrameTimeP95();
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetStability( JNIEnv*, jclass )
{
    return runtime().Perf().GetStability();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetSuggestedFpsCap( JNIEnv*, jclass )
{
    return runtime().Perf().GetSuggestedFpsCap();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetTextureBudgetMb( JNIEnv*, jclass )
{
    return runtime().Perf().GetTextureBudgetMb();
}

extern "C" JNIEXPORT jlong JNICALL
JNI_PREFIX_nativeGetFrameCount( JNIEnv*, jclass )
{
    return (jlong)runtime().Perf().GetFrameCount();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetDroppedFrames( JNIEnv*, jclass )
{
    return (jint)runtime().Perf().GetDroppedFrames();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetQualityDrops( JNIEnv*, jclass )
{
    return (jint)runtime().Perf().GetQualityDrops();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGetQualityRaises( JNIEnv*, jclass )
{
    return (jint)runtime().Perf().GetQualityRaises();
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeResetPerf( JNIEnv*, jclass )
{
    runtime().Perf().Reset();
    runtime().Pacer().Reset();
}

//==============================================================================
//  OpenGL
//==============================================================================

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeGlInit( JNIEnv* pEnv, jclass, jint Width, jint Height )
{
    if( !g_Gl.Init( Width, Height ) )
    {
        A51_LOGE( "gl init failed: %s", g_Gl.GetError() );
        (void)pEnv;
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeGlResize( JNIEnv*, jclass, jint Width, jint Height )
{
    g_Gl.Resize( Width, Height );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeGlDrawFrame( JNIEnv*, jclass, jfloat TimeSec, jfloat DtSec )
{
    // The governor decides the resolution for this frame; the renderer only
    // recreates its back buffer when the change is big enough to matter.
    g_Gl.SetRenderScale( runtime().Perf().GetRenderScale() );
    g_Gl.DrawFrame( TimeSec, DtSec, runtime().Perf().GetQualityLevel() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeGlShutdown( JNIEnv*, jclass )
{
    g_Gl.Shutdown();
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeGlGetError( JNIEnv* pEnv, jclass )
{
    return to_jstring( pEnv, g_Gl.GetError() );
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGlGetBackWidth( JNIEnv*, jclass )
{
    return g_Gl.GetBackWidth();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGlGetBackHeight( JNIEnv*, jclass )
{
    return g_Gl.GetBackHeight();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGlGetDrawCalls( JNIEnv*, jclass )
{
    return (jint)g_Gl.GetDrawCalls();
}

//==============================================================================
//  Input
//==============================================================================

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetTouchEnabled( JNIEnv*, jclass, jboolean Enabled )
{
    runtime().Touch().SetEnabled( Enabled == JNI_TRUE, &runtime().Input() );
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeIsTouchEnabled( JNIEnv*, jclass )
{
    return runtime().Touch().IsEnabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetDeadZone( JNIEnv*, jclass, jfloat DeadZone )
{
    runtime().Touch().SetDeadZone( DeadZone );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetLookSensitivity( JNIEnv*, jclass, jfloat Sensitivity )
{
    runtime().Touch().SetLookSensitivity( Sensitivity );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetLookEnabled( JNIEnv*, jclass, jboolean Enabled )
{
    runtime().Touch().SetLookEnabled( Enabled == JNI_TRUE );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetInvertLookY( JNIEnv*, jclass, jboolean Invert )
{
    runtime().Touch().SetInvertLookY( Invert == JNI_TRUE );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeTouchEvent( JNIEnv*, jclass, jint Action, jint PointerId, jfloat X, jfloat Y )
{
    runtime().Touch().OnTouch( Action, PointerId, X, Y, runtime().GetAspect(), runtime().Input() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeReleaseAllTouches( JNIEnv*, jclass )
{
    runtime().Touch().ReleaseAll( runtime().Input() );
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeGadgetId( JNIEnv* pEnv, jclass, jstring Name )
{
    char Buffer[64];
    if( !jstring_copy( pEnv, Name, Buffer, sizeof( Buffer ) ) ) return INPUT_UNDEFINED;
    return gadget_id_by_name( Buffer );
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeGadgetName( JNIEnv* pEnv, jclass, jint Id )
{
    return to_jstring( pEnv, gadget_name_by_id( Id ) );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetGadget( JNIEnv*, jclass, jint Gadget, jfloat Value )
{
    runtime().Input().SetAnalog( Gadget, Value );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeSetGadgetDigital( JNIEnv*, jclass, jint Gadget, jboolean Pressed )
{
    runtime().Input().SetDigital( Gadget, Pressed == JNI_TRUE );
}

extern "C" JNIEXPORT jfloat JNICALL
JNI_PREFIX_nativeGetGadgetValue( JNIEnv*, jclass, jint Gadget )
{
    return runtime().Input().GetValue( Gadget );
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeIsGadgetPressed( JNIEnv*, jclass, jint Gadget )
{
    return runtime().Input().IsPressed( Gadget ) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeWasGadgetPressed( JNIEnv*, jclass, jint Gadget )
{
    return runtime().Input().WasPressed( Gadget ) ? JNI_TRUE : JNI_FALSE;
}

//==============================================================================
//  Touch layout
//==============================================================================

// The whole layout as JSON - the overlay parses it only when it changes.
extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeLayoutSnapshot( JNIEnv* pEnv, jclass )
{
    static char Buffer[ 16384 ];
    if( !runtime().Layout().ToJson( Buffer, sizeof( Buffer ) ) )
        return to_jstring( pEnv, "{}" );
    return to_jstring( pEnv, Buffer );
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeLayoutLoad( JNIEnv* pEnv, jclass, jstring Json )
{
    char* pBuffer = nullptr;
    const char* pChars = jstring_chars( pEnv, Json );
    if( !pChars ) return JNI_FALSE;

    size_t Len = strlen( pChars );
    pBuffer = (char*)malloc( Len + 1 );
    bool Ok = false;
    if( pBuffer )
    {
        memcpy( pBuffer, pChars, Len + 1 );
        Ok = runtime().Layout().FromJson( pBuffer );
        if( Ok ) runtime().Layout().ClampToScreen( runtime().GetAspect() );
        free( pBuffer );
    }

    jstring_release( pEnv, Json, pChars );
    return Ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutReset( JNIEnv*, jclass )
{
    runtime().Layout().MakeDefault();
    runtime().Layout().ClampToScreen( runtime().GetAspect() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutSetVisible( JNIEnv*, jclass, jint Id, jboolean Visible )
{
    runtime().Layout().SetVisible( Id, Visible == JNI_TRUE );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutSetSize( JNIEnv*, jclass, jint Id, jfloat Size )
{
    runtime().Layout().SetSize( Id, Size );
    runtime().Layout().ClampToScreen( runtime().GetAspect() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutSetOpacity( JNIEnv*, jclass, jint Id, jfloat Opacity )
{
    runtime().Layout().SetOpacity( Id, Opacity );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutSetStyle( JNIEnv*, jclass, jint Id, jint Style )
{
    runtime().Layout().SetStyle( Id, Style );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutMove( JNIEnv*, jclass, jint Id, jfloat X, jfloat Y )
{
    runtime().Layout().Move( Id, X, Y, runtime().GetAspect() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutResetControl( JNIEnv*, jclass, jint Id )
{
    runtime().Layout().ResetControl( Id );
    runtime().Layout().ClampToScreen( runtime().GetAspect() );
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeLayoutClamp( JNIEnv*, jclass )
{
    runtime().Layout().ClampToScreen( runtime().GetAspect() );
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeLayoutHitTest( JNIEnv*, jclass, jfloat X, jfloat Y )
{
    return runtime().Layout().HitTest( X, Y, runtime().GetAspect() );
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeLayoutHitHandle( JNIEnv*, jclass, jfloat X, jfloat Y )
{
    return runtime().Layout().HitTestHandle( X, Y, runtime().GetAspect() );
}

// Position/size of the resize knob, in height units.  Returns { x, y, radius }.
extern "C" JNIEXPORT jfloatArray JNICALL
JNI_PREFIX_nativeLayoutHandle( JNIEnv* pEnv, jclass, jint Index )
{
    jfloatArray Out = pEnv->NewFloatArray( 3 );
    if( !Out ) return nullptr;

    f32 x = 0.0f, y = 0.0f;
    runtime().Layout().HandlePos( Index, runtime().GetAspect(), x, y );

    jfloat Values[3] = { x, y, runtime().Layout().ControlRadius( Index ) };
    pEnv->SetFloatArrayRegion( Out, 0, 3, Values );
    return Out;
}

// The knob position of a stick that is currently being dragged, or an empty
// array when it is not touched.
extern "C" JNIEXPORT jfloatArray JNICALL
JNI_PREFIX_nativeLayoutStickKnob( JNIEnv* pEnv, jclass, jint Index )
{
    f32 x = 0.0f, y = 0.0f;
    if( !runtime().Touch().GetStickKnob( Index, x, y ) ) return nullptr;

    jfloatArray Out = pEnv->NewFloatArray( 2 );
    if( !Out ) return nullptr;

    jfloat Values[2] = { x, y };
    pEnv->SetFloatArrayRegion( Out, 0, 2, Values );
    return Out;
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeLayoutControlActive( JNIEnv*, jclass, jint Index )
{
    return runtime().Touch().IsControlActive( Index ) ? JNI_TRUE : JNI_FALSE;
}

//==============================================================================
//  Game data - scanning and browsing
//==============================================================================

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeScanFolder( JNIEnv* pEnv, jclass, jstring Path, jint MaxDepth )
{
    char Dir[ 1024 ];
    if( !jstring_copy( pEnv, Path, Dir, sizeof( Dir ) ) )
        return to_jstring( pEnv, "[]" );

    static DfsSummary Summaries[ 32 ];
    s32 Count = dfs_scan_folder( Dir, MaxDepth, Summaries, 32 );

    static char Buffer[ 65536 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ArrayBegin();
    for( s32 i = 0; i < Count; i++ )
    {
        const DfsSummary& S = Summaries[i];

        W.ObjectBegin();
        W.Pair( "path",     S.Path );
        W.Pair( "valid",    S.Valid );
        W.Pair( "error",    S.Error );
        W.Pair( "size",     (f64)S.TotalSize );
        W.Pair( "dfsSize",  (f64)S.DfsSize );
        W.Pair( "files",    S.FileCount );
        W.Pair( "splits",   S.SubFileCount );
        W.Pair( "version",  S.Version );
        W.ObjectEnd();
    }
    W.ArrayEnd();

    return to_jstring( pEnv, Buffer );
}

extern "C" JNIEXPORT jlong JNICALL
JNI_PREFIX_nativeFolderSize( JNIEnv* pEnv, jclass, jstring Path, jint MaxDepth )
{
    char Dir[ 1024 ];
    if( !jstring_copy( pEnv, Path, Dir, sizeof( Dir ) ) ) return 0;
    return (jlong)dfs_folder_size( Dir, MaxDepth );
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeBrowseOpen( JNIEnv* pEnv, jclass, jstring Path )
{
    char File[ 1024 ];
    if( !jstring_copy( pEnv, Path, File, sizeof( File ) ) ) return JNI_FALSE;

    g_BrowseArchive.Close();
    return g_BrowseArchive.Open( File ) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeBrowseClose( JNIEnv*, jclass )
{
    g_BrowseArchive.Close();
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeBrowseInfo( JNIEnv* pEnv, jclass )
{
    static char Buffer[ 2048 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ObjectBegin();
    W.Pair( "open",     g_BrowseArchive.IsOpen() );
    W.Pair( "path",     g_BrowseArchive.GetPath() );
    W.Pair( "error",    g_BrowseArchive.GetError() );
    W.Pair( "files",    g_BrowseArchive.GetFileCount() );
    W.Pair( "splits",   g_BrowseArchive.GetSubFileCount() );
    W.Pair( "version",  g_BrowseArchive.GetVersion() );
    W.Pair( "sector",   g_BrowseArchive.GetSectorSize() );
    W.Pair( "splitSize",(f64)g_BrowseArchive.GetSplitSize() );
    W.Pair( "dataSize", (f64)g_BrowseArchive.GetDataSize() );
    W.ObjectEnd();

    return to_jstring( pEnv, Buffer );
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX_nativeBrowseList( JNIEnv* pEnv, jclass, jint First, jint MaxEntries )
{
    static char Buffer[ 131072 ];
    JsonWriter W( Buffer, sizeof( Buffer ) );

    W.ObjectBegin();
    W.Pair( "total", g_BrowseArchive.GetFileCount() );
    W.Pair( "first", First );
    W.Key( "entries" );
    W.ArrayBegin();

    s32 Last = First + MaxEntries;
    if( Last > g_BrowseArchive.GetFileCount() ) Last = g_BrowseArchive.GetFileCount();

    for( s32 i = First; i < Last; i++ )
    {
        const DfsFileInfo& Info = g_BrowseArchive.GetFileInfo( i );
        W.ObjectBegin();
        W.Pair( "i",      i );
        W.Pair( "name",   Info.Path );
        W.Pair( "size",   (f64)Info.Length );
        W.Pair( "offset", (f64)Info.DataOffset );
        W.ObjectEnd();
    }

    W.ArrayEnd();
    W.ObjectEnd();

    return to_jstring( pEnv, Buffer );
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_PREFIX_nativeBrowseRead( JNIEnv* pEnv, jclass, jint Index, jint Offset, jint Length )
{
    if( Length <= 0 || Length > ( 8 * 1024 * 1024 ) ) return nullptr;

    u8* pBuffer = (u8*)malloc( (size_t)Length );
    if( !pBuffer ) return nullptr;

    s32 Got = g_BrowseArchive.Read( Index, (u32)Offset, pBuffer, Length );
    jbyteArray Out = to_jbytes( pEnv, pBuffer, Got );

    free( pBuffer );
    return Out;
}

// Returns the number of bad chunks, or -1 when there is no checksum table.
extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeBrowseVerify( JNIEnv*, jclass, jint MaxChunks )
{
    s32 Bad = 0, Checked = 0;
    if( !g_BrowseArchive.VerifyChecksums( MaxChunks, &Bad, &Checked ) ) return -1;
    return Bad;
}

//==============================================================================
//  Mounted archives (the ones the game session actually uses)
//==============================================================================

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeMountArchive( JNIEnv* pEnv, jclass, jstring Path )
{
    char File[ 1024 ];
    if( !jstring_copy( pEnv, Path, File, sizeof( File ) ) ) return JNI_FALSE;
    return runtime().MountArchive( File ) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX_nativeUnmountAll( JNIEnv*, jclass )
{
    runtime().UnmountAll();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeMountedCount( JNIEnv*, jclass )
{
    return runtime().MountCount();
}

extern "C" JNIEXPORT jint JNICALL
JNI_PREFIX_nativeMountedFileTotal( JNIEnv*, jclass )
{
    return runtime().TotalFiles();
}

extern "C" JNIEXPORT jboolean JNICALL
JNI_PREFIX_nativeFileExists( JNIEnv* pEnv, jclass, jstring Path )
{
    char File[ 1024 ];
    if( !jstring_copy( pEnv, Path, File, sizeof( File ) ) ) return JNI_FALSE;
    return runtime().FileExists( File ) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
JNI_PREFIX_nativeReadFile( JNIEnv* pEnv, jclass, jstring Path, jint Offset, jint Length )
{
    char File[ 1024 ];
    if( !jstring_copy( pEnv, Path, File, sizeof( File ) ) ) return nullptr;
    if( Length <= 0 || Length > ( 8 * 1024 * 1024 ) ) return nullptr;

    u8* pBuffer = (u8*)malloc( (size_t)Length );
    if( !pBuffer ) return nullptr;

    s32 Got = runtime().ReadFile( File, (u32)Offset, pBuffer, Length );
    jbyteArray Out = to_jbytes( pEnv, pBuffer, Got );

    free( pBuffer );
    return Out;
}
//==============================================================================
