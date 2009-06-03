#ifndef TEXTUREIO_H
#define TEXTUREIO_H

#include <stdlib.h>

#include <OpenGL/OpenGL.h>

typedef struct texInfo {
    GLuint tex;
    size_t w;
    size_t h;
    size_t aW;
    size_t aH;
    size_t aC;
} texInfo;

texInfo* createTextureFromPath(CGLContextObj cgl_ctx, char* pathBytes);
texInfo* createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h);
void saveTexture(CGLContextObj cgl_ctx, texInfo* t, char* pathBytes);
void saveFloatTexture(CGLContextObj cgl_ctx, texInfo* t, char* pathBytes);

GLuint loadProgram(CGLContextObj cgl_ctx, char* vertProgPath, char* fragProgPath);
GLuint loadShader(CGLContextObj cgl_ctx, GLenum shaderType, char* filePath);

#endif