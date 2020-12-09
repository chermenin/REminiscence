
/*
 * REminiscence / REinforced
 * Copyright (C) 2020 Alex Chermenin (alex@chermenin.ru)
 */

#include "file.h"
#include "fs.h"
#include "shaders.h"
#include "util.h"

#ifndef __APPLE__

PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;

bool initGLExtensions() {
	glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
	glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
	glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
	glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)SDL_GL_GetProcAddress("glValidateProgram");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
	glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");

	return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv && 
		glGetShaderInfoLog && glDeleteShader && glAttachShader && glCreateProgram &&
		glLinkProgram && glValidateProgram && glGetProgramiv && glGetProgramInfoLog &&
		glUseProgram;
}

#else

bool initGLExtensions() {
	return true;
}

#endif

const char *getFileExtension(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	return dot + 1;
}



GLuint compileShader(const char* source, GLuint shaderType, const int *len) {
	GLuint result = glCreateShader(shaderType);
	glShaderSource(result, 1, &source, len);
	glCompileShader(result);

	GLint shaderCompiled = GL_FALSE;
	glGetShaderiv(result, GL_COMPILE_STATUS, &shaderCompiled);
	if(shaderCompiled != GL_TRUE) {
		debug(DBG_SHADER, "Compile error: %s", result);

		GLint logLength;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0)
		{
			GLchar *log = (GLchar*)malloc(logLength);
			glGetShaderInfoLog(result, logLength, &logLength, log);
			debug(DBG_SHADER, "Compile log: %s", log);
			free(log);
		}
		glDeleteShader(result);
		result = 0;
	} else {
		debug(DBG_SHADER, "Compile successed. Shader id = %d", result);
	}

	return result;
}

GLuint Shaders::compileProgram() const {
	GLuint programId = 0;
	GLuint vtxShaderId, fragShaderId;

	if (_filesCount > 0) {
		debug(DBG_SHADER, "Found %d shader files", _filesCount);
		programId = glCreateProgram();

		for (int i = 0; i < _filesCount; i++) {
			const char *path = _fs->getFileName(i);
			File f;
			if (f.open(path, "rb", _fs)) {
				debug(DBG_SHADER, "Load shaders file: %s", path);
				int len = f.size();
				char *_source = (char *)malloc(len);
				if (!_source) {
					error("Unable to allocate shader buffer");
				} else {
					f.read(_source, len);
				}
				GLuint shaderType;
				const char* ext = getFileExtension(path);
				if (strcasecmp(ext, "vertex") == 0) {
					shaderType = GL_VERTEX_SHADER;
				} else if (strcasecmp(ext, "fragment") == 0) {
					shaderType = GL_FRAGMENT_SHADER;
				} else {
					error("Unknown shader: %s", ext);
				}
				int shaderId = compileShader(_source, shaderType, &len);
				if (shaderId) {
					glAttachShader(programId, shaderId);
					// glDeleteShader(shaderId);
				}
			}
		}

		glLinkProgram(programId);
		glValidateProgram(programId);

		// Check the status of the compile/link
		GLint logLen;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLen);
		if(logLen > 0) {
			char* log = (char*) malloc(logLen * sizeof(char));
			glGetProgramInfoLog(programId, logLen, &logLen, log);
			debug(DBG_SHADER, "Shader program log:\n%s", log);
			free(log);
		}
	}

	return programId;
}

Shaders::Shaders(FileSystem *fs) : _fs(fs) {
	_initExtensions = initGLExtensions();
	_filesCount = fs->filesCount();
}
