#ifndef ERRORS_H
#define ERRORS_H

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/OpenGL.h>

void reportGenericError(char* file, const char* func, unsigned long line, char* msg, char* detail);
#define ERR(msg, detail) reportGenericError(__FILE__, __func__, __LINE__, (msg), (detail))

#define CHK_SYSCALL(fn, msg, detail) if ((fn) == -1) ERR(msg, detail)
#define CHK_NULL(ptr, msg, detail) if ((ptr) == NULL) ERR(msg, detail)

void checkCGLError(CGLContextObj cgl_ctx, char* file, const char* func, unsigned long line, CGLError error);
#define CHK_CGL(fn) (checkCGLError(cgl_ctx, __FILE__, __func__, __LINE__, (fn)))

void checkOGLError(CGLContextObj cgl_ctx, char* file, const char* func, unsigned long line);
#define CHK_OGL (checkOGLError(cgl_ctx, __FILE__, __func__, __LINE__))

void checkFramebufferError(CGLContextObj cgl_ctx, char* file, const char* func, unsigned long line);
#define CHK_FBO (checkFramebufferError(cgl_ctx, __FILE__, __func__, __LINE__))

void dumpPixFmt(CGLContextObj cgl_ctx, CGLPixelFormatObj pix);

void printProgramInfoLog(CGLContextObj cgl_ctx, GLuint obj);
void printShaderInfoLog(CGLContextObj cgl_ctx, GLuint obj);

void checkCFURLError(char* file, const char* func, unsigned long line, Boolean success, SInt32 errorCode);
#define CHK_CFURL(success, errorCode) (checkCFURLError(__FILE__, __func__, __LINE__, (success), (errorCode)))

#endif