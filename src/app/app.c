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

#include "app/app.h"
#include "input/action.h"
#include "render/render.h"
#include "main/prog.h"
#include "util/maths.h"
#include "util/nanotime.h"
#include "util/mem.h"
#include <inttypes.h>

#define TICK_RATE UINT64_C(60)
#define TICK_DURATION (NANOTIME_NSEC_PER_SEC / TICK_RATE)
static uint64_t ticks;
static bool reset_average;
static uint64_t average_ticks;
static uint64_t average_duration;
static uint64_t last_tick_time;

static const char* const text = "\
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor\n\
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis\n\
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.\n\
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu\n\
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in\n\
culpa qui officia deserunt mollit anim id est laborum.\
";

bool app_init(uint64_t* const tick_duration) {
	*tick_duration = TICK_DURATION;
	ticks = 0u;
	last_tick_time = nanotime_now();
	reset_average = true;

	return true;
}

bool app_update(bool* const quit_now, const uint64_t current_time) {
	*quit_now = false;

	if (action_bool_get(ACTION_SET_BASIC_MENU, ACTION_SET_BASIC_MENU_NEGATIVE)) {
		*quit_now = true;
		return true;
	}

	if (action_bool_get(ACTION_SET_BASIC_MENU, ACTION_SET_BASIC_MENU_POSITIVE)) {
		reset_average = true;
	}

	if (!render_start(640.0f, 480.0f)) {
		return false;
	}

	const uint64_t current_tick_duration = nanotime_interval(last_tick_time, current_time, nanotime_now_max());
	const double current_tick_rate = (double)NANOTIME_NSEC_PER_SEC / current_tick_duration;
	if (!reset_average) {
		average_ticks++;
		average_duration += current_tick_duration;
	}
	last_tick_time = current_time;
	ticks++;

	const float shade = sinf(MATHS_PIf * fmodf(ticks / (float)TICK_RATE, 1.0f)) * 0.25f + 0.25f;
	if (!render_clear(shade, shade, shade, 1.0f)) {
		return false;
	}

	if (!reset_average) {
		if (!render_printf("font.fnt", 1u, 8.0f, 8.0f, "\
			Ticks: %" PRIu64 "\n\n\
			Current tick rate: %.9f\n\n\
			Average tick rate: %.9f\n\n\
			Current render frame rate: %.9f\n\n\
			Total dynamic memory in use: %.04f MiB\n\n\
			Total physical memory available: %.04f MiB\n\n\
			Test text:\n%s",

			ticks,
			current_tick_rate,
			average_ticks / (average_duration / (double)NANOTIME_NSEC_PER_SEC),
			prog_render_frame_rate_get(),
			mem_total() / (double)BYTES_PER_MEBIBYTE,
			mem_left() / (double)BYTES_PER_MEBIBYTE,
			text
		)) {
			return false;
		}
	}
	else {
		reset_average = false;
		average_ticks = 0u;
		average_duration = 0u;
		if (!render_printf("font.fnt", 1u, 8.0f, 8.0f, "\
			Ticks: %" PRIu64 "\n\n\
			Current tick rate: %.9f\n\n\
			Average tick rate: N/A\n\n\
			Current render frame rate: %.9f\n\n\
			Total dynamic memory in use: %.04f MiB\n\n\
			Total physical memory available: %.04f MiB\n\n\
			Test text:\n%s",
			
			ticks,
			current_tick_rate,
			prog_render_frame_rate_get(),
			mem_total() / (double)BYTES_PER_MEBIBYTE,
			mem_left() / (double)BYTES_PER_MEBIBYTE,
			text
		)) {
			return false;
		}
	}

	sprite_type sprites[] = {
		{
			.src = { 0.0f, 0.0f, 16.0f, 16.0f },
			.dst = { 0.0f, 0.0f, 16.0f, 16.0f }
		},
		{
			.src = { 0.0f, 0.0f, 16.0f, 16.0f },
			.dst = { 640.0f - 16.0f, 480.0f - 16.0f, 16.0f, 16.0f }
		}
	};

	if (!render_sprites("sprite.png", 0u, 2u, sprites)) {
		return false;
	}

	if (!render_end()) {
		return false;
	}

	return true;
}
