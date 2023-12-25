#pragma once
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

#include "SDL_video.h"
#include "render/private/glad.h"
#include <stdbool.h>

typedef SDL_GLContext opengl_context_object;

/*
 * Does necessary preparation before OpenGL can be used. Must be called before
 * any OpenGL functions (opengl_*, gl*) are called. Returns true if loading was
 * successful, otherwise false.
 */
bool opengl_init();

/*
 * Create an OpenGL context for the current application window. Must only be
 * called in the main thread.
 */
opengl_context_object opengl_context_create();

/*
 * Destroy an OpenGL context. You must ensure the context is not set as current,
 * before calling this function. Must only be called in the main thread.
 */
void opengl_context_destroy(opengl_context_object const context);

/*
 * Make an OpenGL context current in the thread calling this function. Call with
 * NULL to make no context current, such as before destroying the context.
 */
bool opengl_context_make_current(opengl_context_object const context);

/*
 * Create an OpenGL shader object of the requested type. Returns 0 if creation
 * failed.
 */
GLuint opengl_shader_create(const GLenum type, const GLchar* const src);

/*
 * Create an OpenGL shading program object with only vertex and fragment
 * shaders. Returns 0 if creation failed.
 */
GLuint opengl_program_create(const GLchar* const vertex_src, const GLchar* const fragment_src);

/*
 * Indicates if a previous OpenGL API call generated an error. This function
 * will log the particular OpenGL error, along with the provided error message
 * if the message pointer isn't NULL, and returns true; otherwise, if there's no
 * error at the point this function is called, nothing is logged, and false is
 * returned.
 */
bool opengl_error(const char* const message);
