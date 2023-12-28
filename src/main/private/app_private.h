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

#include "main/app.h"

/*
 * Initialize the application. Must only be called in the main thread.
 */
bool app_init(const int argc, char** const argv);

/*
 * Deinitialize the application. Must only be called in the main thread.
 */
void app_deinit();

/*
 * Set the name for the current thread. If name is NULL, this unsets the current
 * thread's name, back to NULL.
 */
bool app_this_thread_name_set(const char* const name);

/*
 * Get the name for the current thread. A get will return NULL if the name is
 * not currently set for the current thread.
 */
const char* const app_this_thread_name_get();

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

/*
 * Returns the application's window.
 */
SDL_Window* app_window_get();
