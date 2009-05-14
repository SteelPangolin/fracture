#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>

GLuint createTextureFromPath(size_t* w, size_t *h, char* pathBytes);
GLuint createEmptyTexture(size_t w, size_t h);

void checkCGLError(char *file, const char* func, unsigned long line, CGLError error);
#define CHK_CGL(fn) (checkCGLError(__FILE__, __func__, __LINE__, (fn)))

void checkOGLError(char *file, const char* func, unsigned long line);
#define CHK_OGL (checkOGLError(__FILE__, __func__, __LINE__))

void checkFramebufferError(char *file, const char* func, unsigned long line);
#define CHK_FBO (checkFramebufferError(__FILE__, __func__, __LINE__))

int main(int argc, char** argv)
{
    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAAccelerated,
        0
    };
    CGLPixelFormatObj pxlFmt;
    GLint numPxlFmts;
    CHK_CGL(CGLChoosePixelFormat(attribs, &pxlFmt, &numPxlFmts));
    
    CGLContextObj cgl_ctx;
    CHK_CGL(CGLCreateContext(pxlFmt, NULL, &cgl_ctx));
    
    size_t w, h;
    GLuint imgTex = createTextureFromPath(&w, &h, "../data/lena.png");
    GLuint fboTex = createEmptyTexture(w, h);
    
    GLuint fbo;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_RECTANGLE_ARB, fboTex, 0);
    CHK_OGL;
    GLuint renderbuffer;
    glGenRenderbuffersEXT(1, &renderbuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbufferEXT(
        GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
        GL_RENDERBUFFER_EXT, renderbuffer);
    CHK_OGL;
    CHK_FBO;
    
    printf("ok\n");
    return EXIT_SUCCESS;
}

void checkCGLError(char *file, const char* func, unsigned long line, CGLError error)
{
    if (error != kCGLNoError)
    {
        printf("%s:%s:%d CGL error: %s\n", file, func, line, CGLErrorString(error));
        exit(EXIT_FAILURE);
    }
}

void checkOGLError(char *file, const char* func, unsigned long line)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("%s:%s:%d OpenGL error: %s\n", file, func, line, gluErrorString(error));
        exit(EXIT_FAILURE);
    }
}

void checkFramebufferError(char *file, const char* func, unsigned long line) {
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    CHK_OGL;
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            return;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, incomplete attachment");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Unsupported framebuffer format");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, missing attachment");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, attachments must have same dimensions");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, attached images must have same format");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, missing draw buffer");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "Framebuffer incomplete, missing read buffer");
            break;
        case 0:
            printf("%s:%s:%d framebuffer error: %s\n", file, func, line,
                "glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) failed");
            CHK_OGL;
            break;
        default:
            printf("%s:%s:%d framebuffer error: Unknown error code %d\n", file, func, line, status);
            break;
    }
    exit(EXIT_FAILURE);
}

GLuint createTextureFromPath(size_t* w, size_t *h, char* pathBytes)
{
    CFStringRef path = CFStringCreateWithCString(NULL, pathBytes, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, false);
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
    
    CFRelease(path);
    CFRelease(url);
    CGImageRelease(img);
    CFRelease(imgSrc);
    CGColorSpaceRelease(colorspace);
    CGContextRelease(cgCtx);
    free(data);
    
    return tex;
}

GLuint createEmptyTexture(size_t w, size_t h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, tex);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, w, h,
        0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    CHK_OGL;
    
    return tex;
}