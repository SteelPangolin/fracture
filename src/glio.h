#ifndef TEXTUREIO_H
#define TEXTUREIO_H

#include <stdlib.h>

#include <OpenGL/OpenGL.h>

GLuint createTextureFromPath(CGLContextObj cgl_ctx, size_t* w, size_t *h, char* pathBytes);
GLuint createEmptyTexture(CGLContextObj cgl_ctx, GLenum format, size_t w, size_t h);
void saveTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);
void saveFloatTexture(CGLContextObj cgl_ctx, GLuint tex, size_t w, size_t h, char* pathBytes);

GLuint loadProgram(CGLContextObj cgl_ctx, char* vertProgPath, char* fragProgPath);
GLuint loadShader(CGLContextObj cgl_ctx, GLenum shaderType, char* filePath);

#endif