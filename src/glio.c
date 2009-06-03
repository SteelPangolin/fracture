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

texInfo* createTextureFromPath(CGLContextObj cgl_ctx, char* pathBytes)
{
    texInfo* t = calloc(1, sizeof(texInfo));
    
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    CGImageSourceRef imgSrc = CGImageSourceCreateWithURL(url, NULL);
    CGImageRef img = CGImageSourceCreateImageAtIndex(imgSrc, 0, NULL);
    t->w  = CGImageGetWidth(img);
    t->h = CGImageGetHeight(img);
    CGRect rect = {{0, 0}, {t->w, t->h}};
    void* data = calloc(t->w * 4, t->h);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef cgCtx = CGBitmapContextCreate(
        data, t->w, t->h, 8, t->w * 4, colorspace,
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
    CGContextDrawImage(cgCtx, rect, img);
    
    glGenTextures(1, &(t->tex));
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->tex);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, t->w);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, t->w, t->h,
        0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    CHK_OGL;
    
    CFRelease(url);
    CGImageRelease(img);
    CFRelease(imgSrc);
    CGColorSpaceRelease(colorspace);
    CGContextRelease(cgCtx);
    free(data);
    
    t->aW = t->w;
    t->aH = t->h;
    t->aC = 4;
    
    return t;
}

texInfo* createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h)
{
    texInfo* t = calloc(1, sizeof(texInfo));
    
    t->w = w;
    t->h = h;
    
    glGenTextures(1, &(t->tex));
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->tex);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_RECTANGLE_ARB, 0, format, t->w, t->h,
        0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    CHK_OGL;
    
    return t;
}

void saveTexture(CGLContextObj cgl_ctx, texInfo* t, char* pathBytes)
{
    void* texDataBase = malloc(4 * t->w * t->h);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->tex);
    glGetTexImage(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV, texDataBase);
    CHK_OGL;
    
    CFMutableDataRef cfData = CFDataCreateMutable(NULL, t->w * t->h * 4);
    CFDataSetLength(cfData, t->w * t->h * 4);
    
    size_t texPixelLen = 4;
    size_t imgPixelLen = t->aC;
    
    size_t texRowSkip = texPixelLen * (t->w - t->aW);
    
    void* imgDataBase = CFDataGetMutableBytePtr(cfData);
    void* imgDataPtr = imgDataBase;
    void* texDataPtr = texDataBase;
    size_t i, j;
    for (j = 0; j < t->aH; j++)
    {
        for (i = 0; i < t->aW; i++)
        {
            memcpy(imgDataPtr, texDataPtr, imgPixelLen);
            imgDataPtr += imgPixelLen;
            texDataPtr += texPixelLen;
        }
        texDataPtr += texRowSkip;
    }
    
    CGColorSpaceRef colorspace;
    CGBitmapInfo bitmapInfo;
    char* errorString;
    switch (t->aC)
    {
    case 1:
        colorspace = CGColorSpaceCreateDeviceGray();
        bitmapInfo = kCGBitmapByteOrderDefault;
        break;
    case 4:
        colorspace = CGColorSpaceCreateDeviceRGB();
        bitmapInfo = kCGBitmapByteOrder32Host | kCGImageAlphaFirst;
        break;
    default:
        asprintf(&errorString, "%d", t->aC);
        ERR("unsupported number of channels", errorString);
    }
    
    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(cfData);
    CGImageRef resultImg = CGImageCreate(
        t->aW, t->aH, 8, t->aC * 8, t->aC * t->aW, colorspace, bitmapInfo,
        dataProvider, NULL, false, kCGRenderingIntentDefault);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    CGImageDestinationRef imgDst = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(imgDst, resultImg, NULL);
    CGImageDestinationFinalize(imgDst);
    
    printf("wrote texture as PNG: %s (%d x %d, %d channels)\n", pathBytes, t->aW, t->aH, t->aC);
    
    free(texDataBase);
    CFRelease(cfData);
    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorspace);
    CGImageRelease(resultImg);
    CFRelease(url);
    CFRelease(imgDst);
}

void saveFloatTexture(CGLContextObj cgl_ctx, texInfo* t, char* pathBytes)
{
    void* texDataBase = malloc(sizeof(GLfloat) * 4 * t->w * t->h);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, t->tex);
    glGetTexImage(
        GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
        GL_FLOAT, texDataBase);
    CHK_OGL;
    
    size_t cookedImgByteCount =
          sizeof(struct floatImageHeader)
        + t->aW * t->aH * t->aC * sizeof(GLfloat);
    CFMutableDataRef cfData = CFDataCreateMutable(NULL, cookedImgByteCount);
    CFDataSetLength(cfData, cookedImgByteCount);
    void* imgBase = CFDataGetMutableBytePtr(cfData);
    
    struct floatImageHeader* imgHeaderBase = (struct floatImageHeader*)imgBase;
    imgHeaderBase->sig[0] = '2'; /* little-endian signature */
    imgHeaderBase->sig[1] = '3';
    imgHeaderBase->sig[2] = 'l';
    imgHeaderBase->sig[3] = 'f';
    imgHeaderBase->numChannels = t->aC;
    imgHeaderBase->w = t->aW;
    imgHeaderBase->h = t->aH;
    
    size_t texPixelLen = sizeof(GLfloat) * 4;
    size_t imgPixelLen = sizeof(GLfloat) * t->aC;
    
    size_t texRowSkip = texPixelLen * (t->w - t->aW);
    
    void* imgDataBase = imgBase + sizeof(struct floatImageHeader);
    void* imgDataPtr = imgDataBase;
    void* texDataPtr = texDataBase;
    size_t i, j;
    for (j = 0; j < t->aH; j++)
    {
        for (i = 0; i < t->aW; i++)
        {
            memcpy(imgDataPtr, texDataPtr, imgPixelLen);
            imgDataPtr += imgPixelLen;
            texDataPtr += texPixelLen;
        }
        texDataPtr += texRowSkip;
    }
    
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (unsigned char*)pathBytes, strlen(pathBytes), false);
    Boolean success;
    SInt32 errorCode;
    success = CFURLWriteDataAndPropertiesToResource(url, cfData, NULL, &errorCode);
    CHK_CFURL(success, errorCode);
    
    printf("wrote texture as float dump: %s (%d x %d, %d channels)\n", pathBytes, t->aW, t->aH, t->aC);
    
    free(texDataBase);
    CFRelease(cfData);
    CFRelease(url);
}

void releaseTexture(CGLContextObj cgl_ctx, texInfo* t)
{
    glDeleteTextures(1, &(t->tex));
    free(t);
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