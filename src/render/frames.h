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

#include <stdbool.h>

typedef struct frames_object frames_object;

typedef bool (* command_update_func)(void* const state);
typedef bool (* command_draw_func)(void* const state);
typedef void (* command_destroy_func)(void* const state);

typedef struct command_funcs {
	command_update_func update;
	command_draw_func draw;
	command_destroy_func destroy;
} command_funcs;

typedef enum frames_status_type {
	FRAMES_STATUS_SUCCESS,
	FRAMES_STATUS_NO_START,
	FRAMES_STATUS_NO_END,
	FRAMES_STATUS_NO_FRAMES,
	FRAMES_STATUS_ERROR
} frames_status_type;

frames_object* frames_create();

bool frames_destroy(frames_object* const frames);

frames_status_type frames_start(frames_object* const frames);

bool frames_end(frames_object* const frames);

bool frames_enqueue_command(
	frames_object* const frames,
	const command_funcs* const funcs,
	void* const state
);

frames_status_type frames_draw_latest(frames_object* const frames);
