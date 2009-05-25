#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLMacro.h>

#include "errors.h"

void reportGenericError(char *file, const char* func, unsigned long line, char* msg, char* detail)
{
    printf("%s:%s:%d %s: %s\n", file, func, line, msg, detail);
    exit(EXIT_FAILURE);
}

void checkCGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line, CGLError error)
{
    if (error != kCGLNoError)
    {
        reportGenericError(file, func, line, "CGL error", (char*)CGLErrorString(error));
    }
}

void checkOGLError(CGLContextObj cgl_ctx, char *file, const char* func, unsigned long line)
{
    // TODO: print entire OpenGL error stack instead of just the top
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        reportGenericError(file, func, line, "OpenGL error", (char*)gluErrorString(error));
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

/* credit: CS 101c */
void printShaderInfoLog(CGLContextObj cgl_ctx, GLuint obj)
{
    GLint infologLength = 0;
    GLint charsWritten  = 0;
    char *infoLog;
    
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
        printf("%s\n",infoLog);
        free(infoLog);
    }
}

/* credit: CS 101c */
void printProgramInfoLog(CGLContextObj cgl_ctx, GLuint obj)
{
    GLint infologLength = 0;
    GLint charsWritten  = 0;
    char *infoLog;
    
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        printf("%s\n",infoLog);
        free(infoLog);
    }
}

void checkCFURLError(char *file, const char* func, unsigned long line, Boolean success, SInt32 errorCode)
{
    if (!success)
    {
        switch (errorCode)
        {
        case kCFURLUnknownError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLUnknownError");
            break;
        case kCFURLUnknownSchemeError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLUnknownSchemeError");
            break;
        case kCFURLResourceNotFoundError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLResourceNotFoundError");
            break;
        case kCFURLResourceAccessViolationError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLResourceAccessViolationError");
            break;
        case kCFURLRemoteHostUnavailableError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLRemoteHostUnavailableError");
            break;
        case kCFURLImproperArgumentsError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLImproperArgumentsError");
            break;
        case kCFURLUnknownPropertyKeyError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLUnknownPropertyKeyError");
            break;
        case kCFURLPropertyKeyUnavailableError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLPropertyKeyUnavailableError");
            break;
        case kCFURLTimeoutError:
            reportGenericError(file, func, line, "CFURL error", "kCFURLTimeoutError");
            break;
        default:
            reportGenericError(file, func, line, "CFURL error", "unenumerated error");
            break;
        }
    }
}