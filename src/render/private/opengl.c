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

#include "render/private/opengl.h"
#include "main/private/prog_private.h"
#include "main/main.h"
#include "util/log.h"
#include "util/mem.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

opengl_context_object opengl_context_create() {
	log_printf("Creating an OpenGL context\n");

	assert(main_thread_is_this_thread());

	SDL_Window* const current_window = prog_window_get();
	assert(current_window != NULL);

	int context_major_version, context_minor_version, context_profile_mask;

	SDL_GLContext glcontext;
	if ((glcontext = SDL_GL_CreateContext(current_window)) == NULL) {
		log_printf("Error: %s\n", SDL_GetError());
		return NULL;
	}
	else if (
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &context_major_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &context_minor_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &context_profile_mask) < 0
	) {
		log_printf("Error: %s\n", SDL_GetError());
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (
		context_major_version < 3 ||
		(context_major_version == 3 && context_minor_version < 3) ||
		context_profile_mask != SDL_GL_CONTEXT_PROFILE_CORE
	) {
		log_printf("Error: OpenGL version is %d.%d %s, OpenGL 3.3 Core or higher is required\n",
			context_major_version, context_minor_version,
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_CORE ? "Core" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY ? "Compatibility" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_ES ? "ES" :
			"[UNKNOWN PROFILE TYPE]"
		);
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (!opengl_init()) {
		log_printf("Error: Failed to initialize OpenGL\n");
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (SDL_GL_SetSwapInterval(0) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	SDL_GL_MakeCurrent(current_window, NULL);

	log_printf("Successfully created an OpenGL context\n");

	return glcontext;
}

void opengl_context_destroy(opengl_context_object const context) {
	assert(main_thread_is_this_thread());
	assert(context != NULL);

	SDL_GL_DeleteContext(context);
}

bool opengl_context_make_current(opengl_context_object const context) {
	SDL_Window* const window = prog_window_get();

	if (SDL_GL_MakeCurrent(window, context) < 0) {
		log_printf("Error making an OpenGL context current in render thread\n");
		return false;
	}

	return true;
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
		log_printf(
			"Error compiling %s shader. OpenGL shader info log:\n%s\n",
			(type == GL_VERTEX_SHADER) ?
				"vertex" :
			(type == GL_FRAGMENT_SHADER) ?
				"fragment" :
				"unknown",
			info_log
		);
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
		log_printf("Error creating OpenGL program object.\n");
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
		log_printf("Error linking shading program. OpenGL program info log:\n%s\n", info_log);
		mem_free(info_log);
		glDeleteProgram(program);
		return 0u;
	}
}

bool opengl_error(const char* const format, ...) {
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		if (format != NULL) {
			va_list args;
			va_start(args, format);
			log_vprintf(format, args);
			va_end(args);
		}

		switch (error) {
		default:
			log_printf("Unknown OpenGL error\n");
			break;

		case GL_INVALID_ENUM:
			log_printf("Invalid OpenGL enum value\n");
			break;

		case GL_INVALID_VALUE:
			log_printf("Invalid OpenGL numeric value\n");
			break;

		case GL_INVALID_OPERATION:
			log_printf("Invalid OpenGL operation\n");
			break;

		case GL_INVALID_FRAMEBUFFER_OPERATION:
			log_printf("Invalid OpenGL framebuffer operation\n");
			break;

		case GL_OUT_OF_MEMORY:
			log_printf("Out of memory for OpenGL\n");
			break;
		}
		return true;
	}
	else {
		return false;
	}
}
