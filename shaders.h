
/*
 * REminiscence / REinforced
 * Copyright (C) 2020 Alex Chermenin (alex@chermenin.ru)
 */

#ifndef SHADERS_H__
#define SHADERS_H__

#include "intern.h"
#include <SDL.h>
#include <SDL_image.h>

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#include <OpenGL/OpenGL.h>
#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#endif //!ESSENTIAL_GL_PRACTICES_SUPPORT_GL3
#else
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#endif

struct FileName;
struct File;
struct FileSystem;
struct SystemStub;

void useProgram(GLuint programId);

struct Shaders {

	FileSystem *_fs;
	int _filesCount;
	bool _initExtensions;
	int _programId;

	Shaders(FileSystem *fs);
	GLuint compileProgram() const;
};

#endif // SHADERS_H__
