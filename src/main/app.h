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
#include <stdlib.h>
#include <stdbool.h>

typedef enum quit_status_type {
	QUIT_NOT,
	QUIT_SUCCESS,
	QUIT_FAILURE
} quit_status_type;

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
 * The text shown in the application's window title.
 */
extern const char* const app_title;

/*
 * Initialize the application. Must only be called in the main thread.
 */
bool app_init(const int argc, char** const argv);

/*
 * Deinitialize the application. Must only be called in the main thread.
 */
void app_deinit();

/*
 * Returns true if the application is fully initialized, otherwise false.
 */
bool app_inited();

/*
 * Returns the application's window.
 */
SDL_Window* app_window_get();

/*
 * Returns the size of the render output pixel rectangle the game renders into,
 * which may differ from other graphical dimensions, such as the total size of
 * the window.
 */
void app_render_size_get(size_t* const width, size_t* const height);

/*
 * Returns the current rendering frame rate. The current rendering frame rate is
 * variable, and not completely tied to display refresh rate, but is maintained
 * to around up to the display refresh rate, or around the rate of game updates
 * if game updates are slower paced than display refresh rate. Rendering frame
 * rate is independent of game update rate.
 */
double app_render_frame_rate_get();

/*
 * Creates an OpenGL context of the application's window. Must only be called in
 * the main thread.
 */
SDL_GLContext app_glcontext_create();

/*
 * Destroy an OpenGL context. Must only be called in the main thread.
 */
void app_glcontext_destroy(SDL_GLContext const glcontext);

/*
 * Returns the current resource path.
 */
const char* app_resource_path_get();

/*
 * Returns the current save path.
 */
const char* app_save_path_get();

/*
 * Does one step of updating the application's main thread tasks (reading input
 * and updating the game).
 *
 * This function should be called in a loop, continuing so long as this function
 * returns QUIT_NOT, breaking out when this function returns QUIT_SUCCESS or
 * QUIT_FAILURE, returning EXIT_SUCCESS or EXIT_FAILURE to the operating system
 * corresponding to the returned quit status.
 *
 * This function must only be called in the main thread, between app_init and
 * app_deinit.
 */
quit_status_type app_update();
