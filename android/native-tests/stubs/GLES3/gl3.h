//==============================================================================
//
//  stubs/GLES3/gl3.h
//
//  Compile check only stand-in for the real NDK header (see stubs/jni.h).  The
//  signatures mirror the OpenGL ES 3.0 declarations for the entry points that
//  renderer/gl_runtime.cpp uses, so a wrong argument type still fails to build.
//
//==============================================================================

#ifndef A51_STUB_GLES3_GL3_H
#define A51_STUB_GLES3_GL3_H

#include <cstddef>

typedef unsigned int    GLenum;
typedef unsigned char   GLboolean;
typedef unsigned int    GLbitfield;
typedef void            GLvoid;
typedef signed char     GLbyte;
typedef unsigned char   GLubyte;
typedef short           GLshort;
typedef unsigned short  GLushort;
typedef int             GLint;
typedef unsigned int    GLuint;
typedef int             GLsizei;
typedef float           GLfloat;
typedef float           GLclampf;
typedef char            GLchar;
typedef std::ptrdiff_t  GLsizeiptr;
typedef std::ptrdiff_t  GLintptr;

#define GL_NO_ERROR                     0
#define GL_FALSE                        0
#define GL_TRUE                         1

#define GL_POINTS                       0x0000
#define GL_LINES                        0x0001
#define GL_TRIANGLES                    0x0004

#define GL_BLEND                        0x0BE2
#define GL_SRC_ALPHA                    0x0302
#define GL_ONE                          1

#define GL_COLOR_BUFFER_BIT             0x00004000
#define GL_DEPTH_BUFFER_BIT             0x00000100

#define GL_TEXTURE_2D                   0x0DE1
#define GL_TEXTURE0                     0x84C0
#define GL_RGBA                         0x1908
#define GL_RGBA8                        0x8058
#define GL_UNSIGNED_BYTE                0x1401
#define GL_FLOAT                        0x1406
#define GL_TEXTURE_MIN_FILTER           0x2801
#define GL_TEXTURE_MAG_FILTER           0x2800
#define GL_TEXTURE_WRAP_S               0x2802
#define GL_TEXTURE_WRAP_T               0x2803
#define GL_LINEAR                       0x2601
#define GL_NEAREST                      0x2600
#define GL_CLAMP_TO_EDGE                0x812F

#define GL_FRAMEBUFFER                  0x8D40
#define GL_COLOR_ATTACHMENT0            0x8CE0
#define GL_FRAMEBUFFER_COMPLETE         0x8CD5

#define GL_ARRAY_BUFFER                 0x8892
#define GL_STATIC_DRAW                  0x88E4
#define GL_DYNAMIC_DRAW                 0x88E8

#define GL_VERTEX_SHADER                0x8B31
#define GL_FRAGMENT_SHADER              0x8B30
#define GL_COMPILE_STATUS               0x8B81
#define GL_LINK_STATUS                  0x8B82

#ifdef __cplusplus
extern "C" {
#endif

GLenum      glGetError            ( void );

GLuint      glCreateShader        ( GLenum Type );
void        glShaderSource        ( GLuint Shader, GLsizei Count, const GLchar* const* String, const GLint* Length );
void        glCompileShader       ( GLuint Shader );
void        glGetShaderiv         ( GLuint Shader, GLenum PName, GLint* Params );
void        glGetShaderInfoLog    ( GLuint Shader, GLsizei BufSize, GLsizei* Length, GLchar* InfoLog );
void        glDeleteShader        ( GLuint Shader );

GLuint      glCreateProgram       ( void );
void        glAttachShader        ( GLuint Program, GLuint Shader );
void        glDetachShader        ( GLuint Program, GLuint Shader );
void        glLinkProgram         ( GLuint Program );
void        glGetProgramiv        ( GLuint Program, GLenum PName, GLint* Params );
void        glGetProgramInfoLog   ( GLuint Program, GLsizei BufSize, GLsizei* Length, GLchar* InfoLog );
void        glDeleteProgram       ( GLuint Program );
void        glUseProgram          ( GLuint Program );
GLint       glGetUniformLocation  ( GLuint Program, const GLchar* Name );

void        glUniform1i           ( GLint Location, GLint v0 );
void        glUniform1f           ( GLint Location, GLfloat v0 );
void        glUniform2f           ( GLint Location, GLfloat v0, GLfloat v1 );

void        glGenTextures         ( GLsizei n, GLuint* Textures );
void        glDeleteTextures      ( GLsizei n, const GLuint* Textures );
void        glBindTexture         ( GLenum Target, GLuint Texture );
void        glActiveTexture       ( GLenum Texture );
void        glTexImage2D          ( GLenum Target, GLint Level, GLint InternalFormat, GLsizei Width,
                                    GLsizei Height, GLint Border, GLenum Format, GLenum Type,
                                    const void* Pixels );
void        glTexParameteri       ( GLenum Target, GLenum PName, GLint Param );

void        glGenFramebuffers     ( GLsizei n, GLuint* Framebuffers );
void        glDeleteFramebuffers  ( GLsizei n, const GLuint* Framebuffers );
void        glBindFramebuffer     ( GLenum Target, GLuint Framebuffer );
void        glFramebufferTexture2D( GLenum Target, GLenum Attachment, GLenum TexTarget,
                                    GLuint Texture, GLint Level );
GLenum      glCheckFramebufferStatus( GLenum Target );

void        glGenVertexArrays     ( GLsizei n, GLuint* Arrays );
void        glDeleteVertexArrays  ( GLsizei n, const GLuint* Arrays );
void        glBindVertexArray     ( GLuint Array );

void        glGenBuffers          ( GLsizei n, GLuint* Buffers );
void        glDeleteBuffers       ( GLsizei n, const GLuint* Buffers );
void        glBindBuffer          ( GLenum Target, GLuint Buffer );
void        glBufferData          ( GLenum Target, GLsizeiptr Size, const void* Data, GLenum Usage );
void        glBufferSubData       ( GLenum Target, GLintptr Offset, GLsizeiptr Size, const void* Data );

void        glEnableVertexAttribArray( GLuint Index );
void        glVertexAttribPointer ( GLuint Index, GLint Size, GLenum Type, GLboolean Normalized,
                                    GLsizei Stride, const void* Pointer );

void        glViewport            ( GLint x, GLint y, GLsizei Width, GLsizei Height );
void        glClearColor          ( GLfloat r, GLfloat g, GLfloat b, GLfloat a );
void        glClear               ( GLbitfield Mask );
void        glEnable              ( GLenum Cap );
void        glDisable             ( GLenum Cap );
void        glBlendFunc           ( GLenum Sfactor, GLenum Dfactor );
void        glDrawArrays          ( GLenum Mode, GLint First, GLsizei Count );

#ifdef __cplusplus
}
#endif

#endif // A51_STUB_GLES3_GL3_H
//==============================================================================
