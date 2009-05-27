#include <stdio.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>

#include "errors.h"
#include "glio.h"

/*
 * common variables
 */

size_t fbW, fbH;
GLuint fboTex[2];

GLuint paintShader;
GLuint paintShader_tex;
GLuint paintShader_w;
GLuint paintShader_h;

GLuint sumReductionShader;
GLuint sumReductionShader_tex;
GLuint sumReductionShader_w;
GLuint sumReductionShader_h;

GLuint squareShader;
GLuint squareShader_tex;
GLuint squareShader_w;
GLuint squareShader_h;

/*
 * function declarations
 */

void loadGLResources(CGLContextObj cgl_ctx);

GLuint createTextureFromPath(CGLContextObj cgl_ctx, size_t* w, size_t *h, char* pathBytes);
GLuint createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h);
void saveTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);
void saveFloatTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);

GLuint paint(CGLContextObj cgl_ctx,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t dstW, size_t dstH);

GLuint sumReduce(CGLContextObj cgl_ctx,
    size_t times,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t* dstW, size_t* dstH);

GLuint square(CGLContextObj cgl_ctx,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t dstW, size_t dstH);

/*
 * function implementations
 */

void loadGLResources(CGLContextObj cgl_ctx)
{
    /* global state */
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    CHK_OGL;
    
    /* framebuffer, except for color attachments */
    GLuint fbo;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    
    GLuint renderbuffer;
    glGenRenderbuffersEXT(1, &renderbuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, fbW, fbH);
    glFramebufferRenderbufferEXT(
        GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_RENDERBUFFER_EXT, renderbuffer);
    CHK_OGL;
    
    /* scratch FBO color attachments */
    size_t i;
    for (i = 0; i < 2; i++)
    {
        fboTex[i] = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
        glFramebufferTexture2DEXT(
            GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i,
            GL_TEXTURE_RECTANGLE_ARB, fboTex[i], 0);
    }
    
    /* buffers for full screen quad */
    // T2F_V3F: texture coordinates, then vertex position
    GLfloat vertexData[] = {
        0.0, 0.0,
        0.0, 0.0, 0.0,
        
        0.0, 1.0,
        0.0, 1.0, 0.0,
        
        1.0, 1.0,
        1.0, 1.0, 0.0,
        
        1.0, 0.0,
        1.0, 0.0, 0.0
    };
    GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(GLfloat), vertexData, GL_STATIC_DRAW);
    CHK_OGL;
    
    GLuint indexData[] = {0, 1, 2, 3};
    GLuint ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), indexData, GL_STATIC_DRAW);
    CHK_OGL;
    
    glInterleavedArrays(GL_T2F_V3F, 0, 0);
    CHK_OGL;
    
    /* paint shader */
    paintShader = loadProgram(cgl_ctx, "../src/paint.vert", "../src/paint.frag");
    paintShader_tex = glGetUniformLocation(paintShader, "tex");
    paintShader_w = glGetUniformLocation(paintShader, "w");
    paintShader_h = glGetUniformLocation(paintShader, "h");
    CHK_OGL;
    
    /* sum reduction shader */
    sumReductionShader = loadProgram(cgl_ctx, "../src/sumReduction.vert", "../src/sumReduction.frag");
    sumReductionShader_tex = glGetUniformLocation(sumReductionShader, "tex");
    sumReductionShader_w = glGetUniformLocation(sumReductionShader, "w");
    sumReductionShader_h = glGetUniformLocation(sumReductionShader, "h");
    CHK_OGL;
    
    /* square shader */
    squareShader = loadProgram(cgl_ctx, "../src/square.vert", "../src/square.frag");
    squareShader_tex = glGetUniformLocation(squareShader, "tex");
    squareShader_w = glGetUniformLocation(squareShader, "w");
    squareShader_h = glGetUniformLocation(squareShader, "h");
    CHK_OGL;
}

int main(int argc, char** argv)
{
    /* get a CGL context */
    CGLContextObj cgl_ctx;
    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAAccelerated,
        kCGLPFAPBuffer, // bypasses default of window drawable. use kCGLPFARemotePBuffer if this fails on remote session
        //kCGLPFANoRecovery, // disables software fallback
        0
    };
    CGLPixelFormatObj pxlFmt;
    GLint numPxlFmts;
    CHK_CGL(CGLChoosePixelFormat(attribs, &pxlFmt, &numPxlFmts));
    //dumpPixFmt(cgl_ctx, pxlFmt);
    CHK_CGL(CGLCreateContext(pxlFmt, NULL, &cgl_ctx));
    CHK_CGL(CGLSetCurrentContext(cgl_ctx));
    
    size_t domainSize = 8;
    size_t rangeSize  = 4;
    
    /* load image to process */
    GLuint srcImgTex = createTextureFromPath(cgl_ctx, &fbW, &fbH, "../data/lena.png");
    
    loadGLResources(cgl_ctx);
    
    GLuint R_tex = paint(cgl_ctx,
        srcImgTex, fbW, fbH,
        fbW, fbH);
    saveTexture(cgl_ctx, R_tex, fbW, fbH, "R_tex.png");
    
    GLuint R2_tex = square(cgl_ctx,
        srcImgTex, fbW, fbH,
        fbW, fbH);
    saveTexture(cgl_ctx, R2_tex, fbW, fbH, "R2_tex.png");
    
    size_t dstW, dstH;
    GLuint sumR_tex = sumReduce(cgl_ctx,
        2,
        R_tex, fbW, fbH,
        &dstW, &dstH);
    saveTexture(cgl_ctx, sumR_tex, fbW, fbH, "sumR_tex.png");
    
    GLuint D_tex = paint(cgl_ctx,
        R_tex, fbW, fbH,
        fbW / 2, fbH / 2);
    saveTexture(cgl_ctx, D_tex, fbW, fbH, "D_tex.png");
    
    printf("ok\n");
    return EXIT_SUCCESS;
}

GLuint paint(CGLContextObj cgl_ctx,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t dstW, size_t dstH)
{
    glUseProgram(paintShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(paintShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    GLuint dstTex = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(paintShader_w, srcW);
    glUniform1f(paintShader_h, srcH);
    CHK_OGL;
    
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTex);
    CHK_OGL;
    
    glViewport(0, 0, dstW, dstH);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_QUADS, 0, 4);
    glFlush();
    CHK_OGL;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    
    return dstTex;
}

GLuint square(CGLContextObj cgl_ctx,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t dstW, size_t dstH)
{
    glUseProgram(squareShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(squareShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    GLuint dstTex = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(squareShader_w, srcW);
    glUniform1f(squareShader_h, srcH);
    CHK_OGL;
    
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTex);
    CHK_OGL;
    
    glViewport(0, 0, dstW, dstH);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_QUADS, 0, 4);
    glFlush();
    CHK_OGL;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    
    return dstTex;
}

GLuint sumReduce(CGLContextObj cgl_ctx,
    size_t times,
    GLuint srcTex, size_t srcW, size_t srcH,
    size_t* dstW, size_t* dstH)
{
    if (times == 0)
    {
        ERR("degenerate reduction", "did you do something wrong?");
    }
    
    size_t w = srcW;
    size_t h = srcH;
    
    GLuint dstTex = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstTex, 0);
    
    glUseProgram(sumReductionShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(sumReductionShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    if (times == 1)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTex);
        glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
        CHK_OGL;
        CHK_FBO;
        
        glUniform1f(sumReductionShader_w, w);
        glUniform1f(sumReductionShader_h, h);
        CHK_OGL;
        
        glViewport(0, 0, w / 2, h / 2);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_QUADS, 0, 4);
        glFlush();
        CHK_OGL;
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTex);
        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        CHK_OGL;
        CHK_FBO;
        
        glUniform1f(sumReductionShader_w, w);
        glUniform1f(sumReductionShader_h, h);
        CHK_OGL;
        
        glViewport(0, 0, w / 2, h / 2);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_QUADS, 0, 4);
        glFlush();
        CHK_OGL;
        
        GLuint ping = 0;
        GLuint pong = 1;
        
        size_t i;
        for (i = 0; i < times - 2; i++)
        {
            w /= 2;
            h /= 2;
            
            glUniform1f(sumReductionShader_w, w);
            glUniform1f(sumReductionShader_h, h);
            CHK_OGL;
            
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fboTex[ping]);
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + pong);
            CHK_OGL;
            CHK_FBO;
            
            glViewport(0, 0, w / 2, h / 2);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_QUADS, 0, 4);
            glFlush();
            CHK_OGL;
            
            ping ^= 1;
            pong ^= 1;
        }
        
        w /= 2;
        h /= 2;
        
        glUniform1f(sumReductionShader_w, w);
        glUniform1f(sumReductionShader_h, h);
        CHK_OGL;
        
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fboTex[ping]);
        glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
        CHK_OGL;
        CHK_FBO;
        
        glViewport(0, 0, w / 2, h / 2);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_QUADS, 0, 4);
        glFlush();
        CHK_OGL;
    }
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    
    *dstW = w / 2;
    *dstH = h / 2;
    return dstTex;
}