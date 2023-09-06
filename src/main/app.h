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

#include "SDL.h"
#include <stdbool.h>

/*
 * The organization that develops the application.
 */
extern const char* const app_organization;

/*
 * The name of the application's executable.
 */
extern const char* const app_executable;

/*
 * The name of the application.
 */
extern const char* const app_name;

/*
 * The version of the application.
 */
extern const char* const app_version;

/*
 * The time at which configuration of the build system was last ran.
 */
extern const char* const app_configure_time;

/*
 * The text shown for the application's window title.
 */
extern const char* const app_title;

/*
 * Initialize the application.
 */
bool app_init(const int argc, char** const argv);

/*
 * Deinitialize the application.
 */
void app_deinit();

/*
 * Returns true if the application has been initialized, otherwise false.
 */
bool app_inited();

/*
 * Returns the application's window. Returns NULL if the window isn't currently
 * initialized (app_inited() == false).
 */
SDL_Window* app_window_get();

/*
 * Creates an OpenGL context of the application's window. This function also
 * makes the created context current, so if the returned context isn't NULL,
 * OpenGL can immediately be used without the caller making the context current.
 */
SDL_GLContext app_glcontext_create();

/*
 * Destroy the OpenGL context. Only call in the same single thread where the
 * context was created in; such a usage pattern is required for full
 * portability.
 */
void app_glcontext_destroy(SDL_GLContext const context);

/*
 * Returns the current resource path.
 */
const char* const app_resource_path_get();

/*
 * Returns the current save path.
 */
const char* const app_save_path_get();
