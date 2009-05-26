#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>

#include "errors.h"
#include "fpimage.h"

GLuint createTextureFromPath(CGLContextObj cgl_ctx, size_t* w, size_t *h, char* pathBytes);
GLuint createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h);
void saveTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);
void saveFloatTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);

GLuint loadProgram(CGLContextObj cgl_ctx, char* vertProgPath, char* fragProgPath);
GLuint loadShader(CGLContextObj cgl_ctx, GLenum shaderType, char* filePath);

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
    
    /* load image to process */
    size_t w, h;
    GLuint imgTex = createTextureFromPath(cgl_ctx, &w, &h, "../data/lena.png");
    
    /* set up framebuffer */
    GLuint fbo;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
        
    GLuint fboTex[2];
    size_t i;
    for (i = 0; i < 2; i++)
    {
        //fboTex[i] = createEmptyTexture(cgl_ctx, GL_RGBA32F_ARB, w, h);
        fboTex[i] = createEmptyTexture(cgl_ctx, GL_RGBA8, w, h);
        glFramebufferTexture2DEXT(
            GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i,
            GL_TEXTURE_RECTANGLE_ARB, fboTex[i], 0);
        CHK_OGL;
    }
    
    GLuint renderbuffer;
    glGenRenderbuffersEXT(1, &renderbuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbufferEXT(
        GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_RENDERBUFFER_EXT, renderbuffer);
    CHK_OGL;
    CHK_FBO;
    
    /* set up shader */
    GLuint paintShader = loadProgram(cgl_ctx, "../src/test.vert", "../src/test.frag");
    glUseProgram(paintShader);
    GLuint paintShader_imgTex = glGetUniformLocation(paintShader, "imgTex");
    glUniform1i(paintShader_imgTex, 0); /* GL_TEXTURE0 */
    GLuint paintShader_w = glGetUniformLocation(paintShader, "w");
    glUniform1f(paintShader_w, w);
    GLuint paintShader_h = glGetUniformLocation(paintShader, "h");
    glUniform1f(paintShader_h, h);
    CHK_OGL;
    
    /* create full screen quad */
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
    
    /* draw it */
    glViewport(0, 0, w, h);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    CHK_OGL;
    CHK_FBO;
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, imgTex);
    CHK_OGL;
    glDrawArrays(GL_QUADS, 0, 4);
    CHK_OGL;
    glFlush();
    CHK_OGL;
    
    /* set up reduction shader */
    GLuint sumReductionShader = loadProgram(cgl_ctx, "../src/sumReduction.vert", "../src/sumReduction.frag");
    glUseProgram(sumReductionShader);
    GLuint sumReductionShader_tex = glGetUniformLocation(sumReductionShader, "tex");
    glUniform1i(sumReductionShader_tex, 0); /* GL_TEXTURE0 */
    GLuint sumReductionShader_w = glGetUniformLocation(sumReductionShader, "w");
    glUniform1f(sumReductionShader_w, w / 2);
    GLuint sumReductionShader_h = glGetUniformLocation(sumReductionShader, "h");
    glUniform1f(sumReductionShader_h, h / 2);
    CHK_OGL;
    
    /* draw it again */
    glViewport(0, 0, w / 2, h / 2);
    glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
    CHK_OGL;
    CHK_FBO;
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fboTex[0]);
    CHK_OGL;
    glInterleavedArrays(GL_T2F_V3F, 0, 0);
    glDrawArrays(GL_QUADS, 0, 4);
    CHK_OGL;
    glFlush();
    CHK_OGL;
    
    /* get the results */
    saveTexture(cgl_ctx, fboTex[1], w, h, "out.png");
    //saveFloatTexture(cgl_ctx, fboTex[1], w, h, "out.fl32");
    
    printf("ok\n");
    return EXIT_SUCCESS;
}

GLuint createTextureFromPath(CGLContextObj cgl_ctx, size_t* w, size_t *h, char* pathBytes)
{
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    CGImageSourceRef imgSrc = CGImageSourceCreateWithURL(url, NULL);
    CGImageRef img = CGImageSourceCreateImageAtIndex(imgSrc, 0, NULL);
    size_t width  = CGImageGetWidth(img);
    if (w != NULL) *w = width;
    size_t height = CGImageGetHeight(img);
    if (h != NULL) *h = height;
    CGRect rect = {{0, 0}, {width, height}};
    void* data = calloc(width * 4, height);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef cgCtx = CGBitmapContextCreate(
        data, width, height, 8, width * 4, colorspace,
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
    CGContextDrawImage(cgCtx, rect, img);
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, width, height,
        0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    CHK_OGL;
    
    CFRelease(url);
    CGImageRelease(img);
    CFRelease(imgSrc);
    CGColorSpaceRelease(colorspace);
    CGContextRelease(cgCtx);
    free(data);
    
    return tex;
}

GLuint createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_RECTANGLE_ARB, 0, format, w, h,
        0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    CHK_OGL;
    
    return tex;
}

void saveTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes)
{
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    CHK_OGL;
    CFMutableDataRef cfData = CFDataCreateMutable(NULL, w * h * 4);
    CFDataSetLength(cfData, w * h * 4);
    glGetTexImage(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV, CFDataGetMutableBytePtr(cfData));
    CHK_OGL;
    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(cfData);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGImageRef resultImg = CGImageCreate(
        w, h, 8, 4 * 8, 4 * w, colorspace,
        kCGBitmapByteOrder32Host | kCGImageAlphaFirst,
        dataProvider, NULL, false, kCGRenderingIntentDefault);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    CGImageDestinationRef imgDst = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(imgDst, resultImg, NULL);
    CGImageDestinationFinalize(imgDst);
    
    CFRelease(cfData);
    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorspace);
    CGImageRelease(resultImg);
    CFRelease(url);
    CFRelease(imgDst);
}

void saveFloatTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes)
{
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    CHK_OGL;
    size_t cookedImgByteCount = sizeof(struct floatImageHeader) + w * h * 4 * sizeof(GLfloat);
    CFMutableDataRef cfData = CFDataCreateMutable(NULL, cookedImgByteCount);
    CFDataSetLength(cfData, cookedImgByteCount);
    void* imgBase = CFDataGetMutableBytePtr(cfData);
    
    struct floatImageHeader* imgHeaderBase = (struct floatImageHeader*)imgBase;
    imgHeaderBase->sig[0] = '2';
    imgHeaderBase->sig[1] = '3';
    imgHeaderBase->sig[2] = 'l';
    imgHeaderBase->sig[3] = 'f';
    imgHeaderBase->numChannels = 4;
    imgHeaderBase->w = w;
    imgHeaderBase->h = h;
    
    void* imgDataBase = imgBase + sizeof(struct floatImageHeader);
    glGetTexImage(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_BGRA,
        GL_FLOAT, imgDataBase);
    CHK_OGL;
    
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    Boolean success;
    SInt32 errorCode;
    success = CFURLWriteDataAndPropertiesToResource(url, cfData, NULL, &errorCode);
    CHK_CFURL(success, errorCode);
    
    CFRelease(cfData);
    CFRelease(url);
}

GLuint loadProgram(CGLContextObj cgl_ctx, char* vertProgPath, char* fragProgPath)
{
    GLuint vertShader = loadShader(cgl_ctx, GL_VERTEX_SHADER, vertProgPath);
    GLuint fragShader = loadShader(cgl_ctx, GL_FRAGMENT_SHADER, fragProgPath);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    printProgramInfoLog(cgl_ctx, program);
    CHK_OGL;
    return program;
}

GLuint loadShader(CGLContextObj cgl_ctx, GLenum shaderType, char* filePath)
{
    int fd;
    struct stat sb;
    GLint len;
    GLchar* base;
    GLuint shader;
    
    fd = open(filePath, O_RDONLY);
    CHK_SYSCALL(fd, "open() failed", filePath);
    CHK_SYSCALL(fstat(fd, &sb), "fstat() failed", filePath);
    if (!S_ISREG(sb.st_mode)) ERR("not a regular file", filePath);
    if (sb.st_size == 0) ERR("file is empty", filePath);
    base = (GLchar*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    CHK_NULL(base, "mmap() failed", filePath);
    len = sb.st_size;
    
    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar**)&base, &len);
    CHK_OGL;
    glCompileShader(shader);
    printShaderInfoLog(cgl_ctx, shader);
    CHK_OGL;
    
    CHK_SYSCALL(munmap(base, len), "munmap() failed", filePath);
    CHK_SYSCALL(close(fd), "close() failed", filePath);
    
    return shader;
}