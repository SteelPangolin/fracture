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

#include "glio.h"

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
    
    printf("wrote texture as PNG: %s (%d x %d)\n", pathBytes, w, h);
    
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
    
    printf("wrote texture as float dump: %s (%d x %d)\n", pathBytes, w, h);
    
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