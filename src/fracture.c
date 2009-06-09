#include <stdio.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>

#include "errors.h"
#include "glio.h"

/*
 * configuration variables
 */

int originXMult = 4096;

/*
 * common variables
 */

size_t fbW, fbH;
GLuint fboTex[2];

GLuint paintShader;
GLuint paintShader_w;
GLuint paintShader_h;
GLuint paintShader_tex;

GLuint sumReductionShader;
GLuint sumReductionShader_w;
GLuint sumReductionShader_h;
GLuint sumReductionShader_tex;

GLuint squareShader;
GLuint squareShader_w;
GLuint squareShader_h;
GLuint squareShader_tex;

GLuint zipperShader;
GLuint zipperShader_w;
GLuint zipperShader_h;
GLuint zipperShader_RG_tex;
GLuint zipperShader_BA_tex;

GLuint multiplyTiledShader;
GLuint multiplyTiledShader_w;
GLuint multiplyTiledShader_h;
GLuint multiplyTiledShader_D_tex;
GLuint multiplyTiledShader_R_tex;
GLuint multiplyTiledShader_r_size;
GLuint multiplyTiledShader_r_x;
GLuint multiplyTiledShader_r_y;

GLuint calcSOShader;
GLuint calcSOShader_originXMult;
GLuint calcSOShader_w;
GLuint calcSOShader_h;
GLuint calcSOShader_sumD_sumD2_sumDr_tex;
GLuint calcSOShader_sumR_sumR2_tex;
GLuint calcSOShader_n;
GLuint calcSOShader_r_i;
GLuint calcSOShader_r_j;

GLuint searchReductionShader;
GLuint searchReductionShader_w;
GLuint searchReductionShader_h;
GLuint searchReductionShader_tex;

/*
 * function declarations
 */

int log2int(int x);

void loadGLResources(CGLContextObj cgl_ctx);

texInfo* paint(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t dstW, size_t dstH);

texInfo* sumReduce(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t times);

texInfo* square(CGLContextObj cgl_ctx,
    texInfo* srcT);

texInfo* zipper(CGLContextObj cgl_ctx,
    texInfo* rgT, texInfo* baT);

texInfo* multiplyTiled(CGLContextObj cgl_ctx,
    texInfo* D_T, texInfo* R_T,
    size_t r_size, size_t r_x, size_t r_y);

texInfo* calcSO(CGLContextObj cgl_ctx,
    texInfo* sumD_sumD2_sumDr_T, texInfo* sumR_sumR2_T,
    size_t n, size_t r_i, size_t r_j,
    GLfloat originXMult);

texInfo* searchReduce(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t times);

GLfloat* createTupleFromPointTexture(CGLContextObj cgl_ctx,
    texInfo* t);

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
    paintShader_w = glGetUniformLocation(paintShader, "w");
    paintShader_h = glGetUniformLocation(paintShader, "h");
    paintShader_tex = glGetUniformLocation(paintShader, "tex");
    CHK_OGL;
    
    /* sum reduction shader */
    sumReductionShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/sumReduction.frag");
    sumReductionShader_w = glGetUniformLocation(sumReductionShader, "w");
    sumReductionShader_h = glGetUniformLocation(sumReductionShader, "h");
    sumReductionShader_tex = glGetUniformLocation(sumReductionShader, "tex");
    CHK_OGL;
    
    /* square shader */
    squareShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/square.frag");
    squareShader_w = glGetUniformLocation(squareShader, "w");
    squareShader_h = glGetUniformLocation(squareShader, "h");
    squareShader_tex = glGetUniformLocation(squareShader, "tex");
    CHK_OGL;
    
    /* zipper shader */
    zipperShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/zipper.frag");
    zipperShader_w = glGetUniformLocation(zipperShader, "w");
    zipperShader_h = glGetUniformLocation(zipperShader, "h");
    zipperShader_RG_tex = glGetUniformLocation(zipperShader, "RG_tex");
    zipperShader_BA_tex = glGetUniformLocation(zipperShader, "BA_tex");
    CHK_OGL;
    
    /* multiply a repeated range block with each domain block */
    multiplyTiledShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/multiplyTiled.frag");
    multiplyTiledShader_w = glGetUniformLocation(multiplyTiledShader, "w");
    multiplyTiledShader_h = glGetUniformLocation(multiplyTiledShader, "h");
    multiplyTiledShader_D_tex = glGetUniformLocation(multiplyTiledShader, "D_tex");
    multiplyTiledShader_R_tex = glGetUniformLocation(multiplyTiledShader, "R_tex");
    multiplyTiledShader_r_size = glGetUniformLocation(multiplyTiledShader, "r_size");
    multiplyTiledShader_r_x = glGetUniformLocation(multiplyTiledShader, "r_x");
    multiplyTiledShader_r_y = glGetUniformLocation(multiplyTiledShader, "r_y");
    CHK_OGL;
    
    /* shader to calculate scale and offset */
    calcSOShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/calcSO.frag");
    calcSOShader_originXMult = glGetUniformLocation(calcSOShader, "originXMult");
    calcSOShader_w = glGetUniformLocation(calcSOShader, "w");
    calcSOShader_h = glGetUniformLocation(calcSOShader, "h");
    calcSOShader_sumD_sumD2_sumDr_tex = glGetUniformLocation(calcSOShader, "sumD_sumD2_sumDr_tex");
    calcSOShader_sumR_sumR2_tex = glGetUniformLocation(calcSOShader, "sumR_sumR2_tex");
    calcSOShader_n = glGetUniformLocation(calcSOShader, "n");
    calcSOShader_r_i = glGetUniformLocation(calcSOShader, "r_i");
    calcSOShader_r_j = glGetUniformLocation(calcSOShader, "r_j");
    CHK_OGL;
    
    /* search reduction shader */
    searchReductionShader = loadProgram(cgl_ctx, "../src/common.vert", "../src/searchReduction.frag");
    searchReductionShader_w = glGetUniformLocation(searchReductionShader, "w");
    searchReductionShader_h = glGetUniformLocation(searchReductionShader, "h");
    searchReductionShader_tex = glGetUniformLocation(searchReductionShader, "tex");
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
    
    size_t d_size;
    size_t r_size;
    char* trnOutPath;
    if (argc < 2)
    {
        ERR("not enough arguments", "");
    }
    else
    {
        if      (strncmp("SD", argv[1], 3) == 0)
        {        
            d_size = 8;
            r_size = 4;
            trnOutPath = "OpenGL-lena.trn";
        }
        else if (strncmp("HD", argv[1], 3) == 0)
        {        
            d_size = 4;
            r_size = 2;
            trnOutPath = "OpenGL-lena-HD.trn";
        }
        else
        {
            ERR("bad quality argument", argv[1]);
        }
    }
    
    /* load image to process */
    texInfo* srcImgT = createTextureFromPath(cgl_ctx, "../data/lena.png");
    srcImgT->aC = 1;
    fbW = srcImgT->w;
    fbH = srcImgT->h;
    
    FILE* trnOutFile = fopen(trnOutPath, "w");
    CHK_NULL(trnOutFile, "fopen() failed", trnOutPath);
    
    fprintf(trnOutFile, "# orig_w = %d\n", fbW);
    fprintf(trnOutFile, "# orig_h = %d\n", fbH);
    fprintf(trnOutFile, "# d_size = %d\n", d_size);
    fprintf(trnOutFile, "# r_size = %d\n", r_size);
    
    loadGLResources(cgl_ctx);
    
    /* range data */
    
    texInfo* R_T = paint(cgl_ctx,
        srcImgT,
        srcImgT->w, srcImgT->h);
    
    texInfo* R_R2_T = square(cgl_ctx,
        R_T);
    
    texInfo* sumR_sumR2_T = sumReduce(cgl_ctx,
        R_R2_T,
        log2int(r_size));
    
    /* domain data */
    
    int m = log2int(d_size) - log2int(r_size);
    
    texInfo* D_T = paint(cgl_ctx,
        srcImgT,
        srcImgT->w >> m, srcImgT->h >> m);
    
    texInfo* D_D2_T = square(cgl_ctx,
        D_T);
    
    texInfo* sumD_sumD2_T = sumReduce(cgl_ctx,
        D_D2_T,
        log2int(r_size));
    
    releaseTexture(cgl_ctx, R_R2_T);
    releaseTexture(cgl_ctx, D_D2_T);
    
    /* for each range... */
    size_t r_i, r_j;
    for (r_j = 0; r_j < R_T->aH / r_size; r_j++)
    {
        for (r_i = 0; r_i < R_T->aW / r_size; r_i++)
        {
            texInfo* Dr_T = multiplyTiled(cgl_ctx, D_T, R_T,
                r_size, r_i * r_size, r_j * r_size);
            
            texInfo* sumDr_T = sumReduce(cgl_ctx,
                Dr_T,
                log2int(r_size));
            
            texInfo* sumD_sumD2_sumDr_T = zipper(cgl_ctx,
                sumD_sumD2_T, sumDr_T);
            
            texInfo* rangeCandidates_T = calcSO(cgl_ctx,
                sumD_sumD2_sumDr_T, sumR_sumR2_T,
                r_size * r_size, r_i, r_j,
                originXMult);
            
            texInfo* rangeTransform_T = searchReduce(cgl_ctx,
                rangeCandidates_T,
                log2int(rangeCandidates_T->aW));
            
            GLfloat* transform = createTupleFromPointTexture(cgl_ctx, rangeTransform_T);
            
            releaseTexture(cgl_ctx, Dr_T);
            releaseTexture(cgl_ctx, sumDr_T);
            releaseTexture(cgl_ctx, sumD_sumD2_sumDr_T);
            releaseTexture(cgl_ctx, rangeCandidates_T);
            releaseTexture(cgl_ctx, rangeTransform_T);
            
            GLfloat MSE = transform[0];
            GLfloat s = transform[1];
            GLfloat o = transform[2];
            GLfloat packedOrigin = transform[3];
            
            size_t d_i = (size_t)    (transform[3] / originXMult) / 2;
            size_t d_j = (size_t)fmod(transform[3],  originXMult) / 2;
            
            fprintf(trnOutFile, "[%03d : %03d, %03d : %03d] = % f + % f * [%03d : %03d, %03d : %03d]\n",
                r_i * r_size, (r_i + 1) * r_size,
                r_j * r_size, (r_j + 1) * r_size,
                o, s,
                d_i * d_size, (d_i + 1) * d_size,
                d_j * d_size, (d_j + 1) * d_size);
            
            free(transform);
        }
        
        printf("row %d / %d\n", 1 + r_j, R_T->aH / r_size);
    }
    
    fclose(trnOutFile);
    
    return EXIT_SUCCESS;
}

/* TODO: astoundingly inefficient, use glGetPixels maybe? */
GLfloat* createTupleFromPointTexture(CGLContextObj cgl_ctx,
    texInfo* t)
{
    void* texDataBase = malloc(sizeof(GLfloat) * 4 * t->w * t->h);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->tex);
    glGetTexImage(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
        GL_FLOAT, texDataBase);
    CHK_OGL;
    
    void* tuple = malloc(sizeof(GLfloat) * t->aC);
    memcpy(tuple, texDataBase, sizeof(GLfloat) * t->aC);
    
    free(texDataBase);
    
    return (GLfloat*)tuple;
}

texInfo* calcSO(CGLContextObj cgl_ctx,
    texInfo* sumD_sumD2_sumDr_T, texInfo* sumR_sumR2_T,
    size_t n, size_t r_i, size_t r_j,
    GLfloat originXMult)
{
    glUseProgram(calcSOShader);
    glUniform1i(calcSOShader_sumD_sumD2_sumDr_tex, 0 /* GL_TEXTURE0 */);
    glUniform1i(calcSOShader_sumR_sumR2_tex, 1 /* GL_TEXTURE1 */);
    glUniform1f(calcSOShader_n, n);
    glUniform1f(calcSOShader_r_i, r_i);
    glUniform1f(calcSOShader_r_j, r_j);
    glUniform1i(calcSOShader_originXMult, originXMult);
    CHK_OGL;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = sumD_sumD2_sumDr_T->aW;
    dstT->aH = sumD_sumD2_sumDr_T->aH;
    dstT->aC = 4;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(calcSOShader_w, sumD_sumD2_sumDr_T->w);
    glUniform1f(calcSOShader_h, sumD_sumD2_sumDr_T->h);
    CHK_OGL;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, sumD_sumD2_sumDr_T->tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, sumR_sumR2_T->tex);
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

texInfo* multiplyTiled(CGLContextObj cgl_ctx,
    texInfo* D_T, texInfo* R_T,
    size_t r_size, size_t r_x, size_t r_y)
{
    glUseProgram(multiplyTiledShader);
    glUniform1i(multiplyTiledShader_D_tex, 0 /* GL_TEXTURE0 */);
    glUniform1i(multiplyTiledShader_R_tex, 1 /* GL_TEXTURE1 */);
    glUniform1f(multiplyTiledShader_r_size, r_size);
    glUniform1f(multiplyTiledShader_r_x, r_x);
    glUniform1f(multiplyTiledShader_r_y, r_y);
    CHK_OGL;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = D_T->aW;
    dstT->aH = D_T->aH;
    dstT->aC = D_T->aC;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(multiplyTiledShader_w, D_T->w);
    glUniform1f(multiplyTiledShader_h, D_T->h);
    CHK_OGL;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, D_T->tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, R_T->tex);
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
    dstT->aC = 2;
    
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

texInfo* zipper(CGLContextObj cgl_ctx,
    texInfo* rgT, texInfo* baT)
{
    glUseProgram(zipperShader);
    glUniform1i(zipperShader_RG_tex, 0 /* GL_TEXTURE0 */);
    glUniform1i(zipperShader_BA_tex, 1 /* GL_TEXTURE1 */);
    CHK_OGL;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = rgT->aW;
    dstT->aH = rgT->aH;
    dstT->aC = rgT->aC + baT->aC; /* works if rgT->aC == 2 and baT->aC == 1 or 2 */
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
    CHK_OGL;
    CHK_FBO;
    
    glUniform1f(zipperShader_w, rgT->w);
    glUniform1f(zipperShader_h, rgT->h);
    CHK_OGL;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rgT->tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, baT->tex);
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
    
    size_t w = srcT->aW;
    size_t h = srcT->aH;
    
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

texInfo* searchReduce(CGLContextObj cgl_ctx,
    texInfo* srcT,
    size_t times)
{
    if (times == 0)
    {
        ERR("degenerate reduction", "did you do something wrong?");
    }
    
    size_t w = srcT->aW;
    size_t h = srcT->aH;
    
    texInfo* dstT = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, fbW, fbH);
    dstT->aW = w >> times;
    dstT->aH = h >> times;
    dstT->aC = 4;
    
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT,
        GL_TEXTURE_RECTANGLE_ARB, dstT->tex, 0);
    
    glUseProgram(searchReductionShader);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(searchReductionShader_tex, 0 /* GL_TEXTURE0 */);
    CHK_OGL;
    
    if (times == 1)
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcT->tex);
        glDrawBuffer(GL_COLOR_ATTACHMENT2_EXT);
        CHK_OGL;
        CHK_FBO;
        
        glUniform1f(searchReductionShader_w, w);
        glUniform1f(searchReductionShader_h, h);
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
        
        glUniform1f(searchReductionShader_w, w);
        glUniform1f(searchReductionShader_h, h);
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
            
            glUniform1f(searchReductionShader_w, w);
            glUniform1f(searchReductionShader_h, h);
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
        
        glUniform1f(searchReductionShader_w, w);
        glUniform1f(searchReductionShader_h, h);
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