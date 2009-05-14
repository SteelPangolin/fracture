#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>

GLuint createTextureFromPath(CGLContextObj cgl_ctx, size_t* w, size_t *h, char* pathBytes);
GLuint createEmptyTexture(CGLContextObj cgl_ctx, size_t w, size_t h);

void checkCGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line, CGLError error);
#define CHK_CGL(fn) (checkCGLError(cgl_ctx, __FILE__, __func__, __LINE__, (fn)))

void checkOGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line);
#define CHK_OGL (checkOGLError(cgl_ctx, __FILE__, __func__, __LINE__))

void checkFramebufferError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line);
#define CHK_FBO (checkFramebufferError(cgl_ctx, __FILE__, __func__, __LINE__))

void dumpPixFmt(CGLContextObj cgl_ctx, CGLPixelFormatObj pix);

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
    GLuint fboTex = createEmptyTexture(cgl_ctx, w, h);
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
    
    /* set up render area */
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // TODO: why doesn't gluOrtho2D(0, w, 0, h) work?
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    CHK_OGL;
    
    /* create full screen quad */
    // T2F_V3F: texture coordinates, then vertex position
    GLfloat vertexData[] = {
        0.0, 0.0,
        0.0, 0.0, 0.0,
        
        (GLfloat)w, 0.0,
        1.0, 0.0, 0.0,
        
        (GLfloat)w, (GLfloat)h,
        1.0, 1.0, 0.0,
        
        0.0, (GLfloat)h,
        0.0, 1.0, 0.0
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
    
    /* draw it */
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, imgTex);
    CHK_OGL;
    glInterleavedArrays(GL_T2F_V3F, 0, 0);
    glDrawArrays(GL_QUADS, 0, 4);
    CHK_OGL;
    glFlush();
    CHK_OGL;
    
    /* get the results */
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, fboTex);
    CHK_OGL;
    CFMutableDataRef cfData = CFDataCreateMutable(NULL, w * h * 4);
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
    char* pathBytes = "out.png";
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
    
    printf("ok\n");
    return EXIT_SUCCESS;
}

void checkCGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line, CGLError error)
{
    if (error != kCGLNoError)
    {
        printf("%s:%s:%d CGL error: %s\n", file, func, line, CGLErrorString(error));
        exit(EXIT_FAILURE);
    }
}

void checkOGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line)
{
    // TODO: print entire OpenGL error stack instead of just the top
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("%s:%s:%d OpenGL error: %s\n", file, func, line, gluErrorString(error));
        exit(EXIT_FAILURE);
    }
}

void checkFramebufferError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line) {
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

GLuint createEmptyTexture(CGLContextObj cgl_ctx, size_t w, size_t h)
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

/* credit: jfroy */
void dumpPixFmt(CGLContextObj cgl_ctx, CGLPixelFormatObj pix) {
    CGLError err;
    GLint val, ns;
    int i;
    CGLContextObj ctx = CGLGetCurrentContext();
    
    #define QUERYPF(vs, a) err = CGLDescribePixelFormat(pix, vs, a, &val);
    #define PRINTPF(a) printf("%-22s", #a); \
        for (i = 0; i < ns; i++) { \
            QUERYPF(i, a); \
            if (!err) { \
                if (a == kCGLPFARendererID) printf(" %8x", val); \
                else printf(" %8d", val); \
            } \
            else printf(" %s", CGLErrorString(err)); \
        } \
        printf("\n");

    QUERYPF(0, kCGLPFAVirtualScreenCount);
    ns = val;
    printf("%d %-20s", ns, ns>1?"virtual screens":"virtual screen");
    if (ctx) {
        CGLGetVirtualScreen(ctx, &val);
        for (i = 0; i < ns; i++) {
            char buf[256];
            
            CGLSetVirtualScreen(ctx, i);
            strncpy(buf, (char *)glGetString(GL_VENDOR), 256);
            strtok(buf, " ");
            printf(" %8s", buf);
        }
        CGLSetVirtualScreen(ctx, val);
    }
    printf("\n");
    
    PRINTPF(kCGLPFAAllRenderers);
    PRINTPF(kCGLPFADoubleBuffer);
    PRINTPF(kCGLPFAStereo);
    PRINTPF(kCGLPFAAuxBuffers);
    PRINTPF(kCGLPFAColorSize);
    PRINTPF(kCGLPFAAlphaSize);
    PRINTPF(kCGLPFADepthSize);
    PRINTPF(kCGLPFAStencilSize);
    PRINTPF(kCGLPFAAccumSize);
    PRINTPF(kCGLPFAMinimumPolicy);
    PRINTPF(kCGLPFAMaximumPolicy);
    PRINTPF(kCGLPFAOffScreen);
    PRINTPF(kCGLPFAFullScreen);
    PRINTPF(kCGLPFASampleBuffers);
    PRINTPF(kCGLPFASamples);
    PRINTPF(kCGLPFAAuxDepthStencil);
    PRINTPF(kCGLPFAColorFloat);
    PRINTPF(kCGLPFAMultisample);
    PRINTPF(kCGLPFASupersample);
    PRINTPF(kCGLPFASampleAlpha);
    PRINTPF(kCGLPFARendererID);
    PRINTPF(kCGLPFASingleRenderer);
    PRINTPF(kCGLPFANoRecovery);
    PRINTPF(kCGLPFAAccelerated);
    PRINTPF(kCGLPFAClosestPolicy);
    PRINTPF(kCGLPFARobust);
    PRINTPF(kCGLPFABackingStore);
    PRINTPF(kCGLPFAMPSafe);
    PRINTPF(kCGLPFAWindow);
    PRINTPF(kCGLPFAMultiScreen);
    PRINTPF(kCGLPFACompliant);
    PRINTPF(kCGLPFADisplayMask);
    PRINTPF(kCGLPFAPBuffer);
    PRINTPF(kCGLPFARemotePBuffer);
    PRINTPF(kCGLPFAAllowOfflineRenderers);
}