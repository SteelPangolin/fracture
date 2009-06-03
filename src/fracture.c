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

texInfo* paint(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t dstW, size_t dstH);

texInfo* sumReduce(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t times);

texInfo* square(CGLContextObj cgl_ctx,
    texInfo* srcT);

int log2int(int x);

/*
 * function implementations
 */

int log2int(int x)
{
    if (x <= 0)
    {
        ERR("argument <= 0", "");
    }
    
    int i = 0;
    while ((x >> i) != 1)
    {
        i++;
    }
    
    if (x != (1 << i))
    {
        i++;
    }
    
    return i;
}

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
        texInfo* t = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
        fboTex[i] = t->tex;
        free(t);
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
    paintShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/paint.frag");
    paintShader_tex = glGetUniformLocation(paintShader, "tex");
    paintShader_w = glGetUniformLocation(paintShader, "w");
    paintShader_h = glGetUniformLocation(paintShader, "h");
    CHK_OGL;
    
    /* sum reduction shader */
    sumReductionShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/sumReduction.frag");
    sumReductionShader_tex = glGetUniformLocation(sumReductionShader, "tex");
    sumReductionShader_w = glGetUniformLocation(sumReductionShader, "w");
    sumReductionShader_h = glGetUniformLocation(sumReductionShader, "h");
    CHK_OGL;
    
    /* square shader */
    squareShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/square.frag");
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
        kCGLPFANoRecovery, // disables software fallback
        0
    };
    CGLPixelFormatObj pxlFmt;
    GLint numPxlFmts;
    CHK_CGL(CGLChoosePixelFormat(attribs, &pxlFmt, &numPxlFmts));
    //dumpPixFmt(cgl_ctx, pxlFmt);
    CHK_CGL(CGLCreateContext(pxlFmt, NULL, &cgl_ctx));
    CHK_CGL(CGLSetCurrentContext(cgl_ctx));
    
    size_t d_size = 8;
    size_t r_size = 4;
    int m = log2int(d_size) - log2int(r_size);
    
    /* load image to process */
    texInfo* srcImgT = createTextureFromPath(cgl_ctx, "../data/lena.png");
    srcImgT->aC = 1;
    fbW = srcImgT->w;
    fbH = srcImgT->h;
    
    loadGLResources(cgl_ctx);
    
    /* range data */
    
    texInfo* R_T = paint(cgl_ctx,
        srcImgT,
        srcImgT->w, srcImgT->h);
    saveTexture(cgl_ctx, R_T, "OpenGL-R.png");
    
    texInfo* sumR_T = sumReduce(cgl_ctx,
        R_T,
        log2int(r_size));
    saveFloatTexture(cgl_ctx, sumR_T, "OpenGL-sumR.fl32");
    
    texInfo* R2_T = square(cgl_ctx,
        R_T);
    saveTexture(cgl_ctx, R2_T, "OpenGL-R2.png");
    
    texInfo* sumR2_T = sumReduce(cgl_ctx,
        R2_T,
        log2int(r_size));
    saveFloatTexture(cgl_ctx, sumR2_T, "OpenGL-sumR2.fl32");
    
    /* domain data */
    
    texInfo* D_T = paint(cgl_ctx,
        srcImgT,
        srcImgT->w >> m, srcImgT->h >> m);
    saveTexture(cgl_ctx, D_T, "OpenGL-D.png");
    
    texInfo* sumD_T = sumReduce(cgl_ctx,
        D_T,
        log2int(d_size));
    saveFloatTexture(cgl_ctx, sumD_T, "OpenGL-sumD.fl32");
    
    texInfo* D2_T = square(cgl_ctx,
        D_T);
    saveTexture(cgl_ctx, D2_T, "OpenGL-D2.png");
    
    texInfo* sumD2_T = sumReduce(cgl_ctx,
        D2_T,
        log2int(d_size));
    saveFloatTexture(cgl_ctx, sumD2_T, "OpenGL-sumD2.fl32");
    
    printf("ok\n");
    return EXIT_SUCCESS;
}

texInfo* paint(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t dstW, size_t dstH)
{
    glUseProgram(paintShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(paintShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = dstW;
    dstT->aH = dstH;
    dstT->aC = srcT->aC;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(paintShader_w, srcT->w);
    glUniform1f(paintShader_h, srcT->h);
    CHK_OGL;
    
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcT->tex);
    CHK_OGL;
    
    glViewport(0, 0, dstW, dstH);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_QUADS, 0, 4);
    glFlush();
    CHK_OGL;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    
    return dstT;
}

texInfo* square(CGLContextObj cgl_ctx,
    texInfo* srcT)
{
    glUseProgram(squareShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(squareShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = srcT->aW;
    dstT->aH = srcT->aH;
    dstT->aC = srcT->aC;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(squareShader_w, srcT->w);
    glUniform1f(squareShader_h, srcT->h);
    CHK_OGL;
    
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcT->tex);
    CHK_OGL;
    
    glViewport(0, 0, dstT->w, dstT->h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_QUADS, 0, 4);
    glFlush();
    CHK_OGL;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    
    return dstT;
}

texInfo* sumReduce(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t times)
{
    if (times == 0)
    {
        ERR("degenerate reduction", "did you do something wrong?");
    }
    
    size_t w = srcT->w;
    size_t h = srcT->h;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = w >> times;
    dstT->aH = h >> times;
    dstT->aC = srcT->aC;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    
    glUseProgram(sumReductionShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(sumReductionShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    if (times == 1)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcT->tex);
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
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcT->tex);
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
    
    return dstT;
}