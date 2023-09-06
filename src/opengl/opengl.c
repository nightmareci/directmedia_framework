/*
 * MIT License
 *
 * Copyright (c) 2023 Brandon McGriff <nightmareci@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "opengl/opengl.h"
#include "util/util.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Although it might be fine to directly pass SDL_GL_GetProcAddress to
 * gladLoadGLLoader on many platforms, the calling convention of
 * SDL_GL_GetProcAddress might not be identical to an ordinary C function with
 * no calling convention specified, which is what gladLoadGLLoader expects. So,
 * for guaranteed safety, this thunk is used.
 */
static void* get_proc_address(const char* name) {
	return SDL_GL_GetProcAddress(name);
}

bool opengl_init() {
	return !!gladLoadGLLoader(get_proc_address);
}

GLuint opengl_shader_create(const GLenum type, const GLchar* const src) {
	GLint compiled;
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled) {
		return shader;
	}
	else {
		GLint info_log_length;
		GLchar* info_log;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		info_log = mem_malloc(info_log_length);
		glGetShaderInfoLog(shader, info_log_length, NULL, info_log);
		fprintf(stderr, "Error: Could not compile %s shader. OpenGL shader info log:\n%s\n", (type == GL_VERTEX_SHADER) ? "vertex" : (type == GL_FRAGMENT_SHADER) ? "fragment" : "unknown", info_log);
		fflush(stderr);
		mem_free(info_log);
		return 0u;
	}
}

GLuint opengl_program_create(const GLchar* const vertex_src, const GLchar* const fragment_src) {
	const GLuint vertex_shader = opengl_shader_create(GL_VERTEX_SHADER, vertex_src);
	if (vertex_shader == 0u) {
		return 0u;
	}
	const GLuint fragment_shader = opengl_shader_create(GL_FRAGMENT_SHADER, fragment_src);
	if (fragment_shader == 0u) {
		return 0u;
	}

	const GLuint program = glCreateProgram();
	if (program == 0u) {
		fprintf(stderr, "Error: Could not create OpenGL program object.\n");
		fflush(stderr);
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return 0u;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glLinkProgram(program);

	glDetachShader(program, vertex_shader);
	glDetachShader(program, fragment_shader);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked) {
		return program;
	}
	else {
		GLint info_log_length;
		GLchar* info_log;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		info_log = mem_malloc(info_log_length);
		glGetProgramInfoLog(program, info_log_length, NULL, info_log);
		fprintf(stderr, "Error: Could not link shading program. OpenGL program info log:\n%s\n", info_log);
		fflush(stderr);
		mem_free(info_log);
		glDeleteProgram(program);
		return 0u;
	}
}

bool opengl_error(const char* const message) {
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		if (message != NULL) {
			fputs(message, stderr);
		}

		switch (error) {
		default:
			fprintf(stderr, "Unknown OpenGL error\n");
			break;

		case GL_INVALID_ENUM:
			fprintf(stderr, "Invalid OpenGL enum value\n");
			break;

		case GL_INVALID_VALUE:
			fprintf(stderr, "Invalid OpenGL numeric value\n");
			break;

		case GL_INVALID_OPERATION:
			fprintf(stderr, "Invalid OpenGL operation\n");
			break;

		case GL_INVALID_FRAMEBUFFER_OPERATION:
			fprintf(stderr, "Invalid OpenGL framebuffer operation\n");
			break;

		case GL_OUT_OF_MEMORY:
			fprintf(stderr, "Out of memory for OpenGL\n");
			break;
		}
		fflush(stderr);
		return true;
	}
	else {
		return false;
	}
}
