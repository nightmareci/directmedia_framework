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

#include "game/game.h"
#include "render/render.h"
#include "util/mathematics.h"
#include "util/nanotime.h"
#include "util/mem.h"
#include "SDL.h"
#include <inttypes.h>
#include <math.h>

#define TICK_RATE UINT64_C(60)
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

bool game_init(uint64_t* const tick_rate) {
	*tick_rate = TICK_RATE;
	ticks = 0u;
	last_tick_time = nanotime_now();
	reset_average = true;

	return true;
}

bool game_update(bool* const quit_now) {
	if (!render_start()) {
		return false;
	}

	*quit_now = false;

	// TODO: Implement a higher-level input API that doesn't present any device
	// specifics to the game; e.g., only abstract inputs are made available,
	// like "menu confirm/deny", not "button A/B".
	const Uint8* const keys = SDL_GetKeyboardState(NULL);
	if (keys[SDL_SCANCODE_ESCAPE]) {
		*quit_now = true;
		return true;
	}

	if (keys[SDL_SCANCODE_SPACE]) {
		reset_average = true;
	}

	const uint64_t current_tick_time = nanotime_now();
	const double tick_rate = (double)NANOTIME_NSEC_PER_SEC / (current_tick_time - last_tick_time);
	if (!reset_average) {
		average_ticks++;
		average_duration += current_tick_time - last_tick_time;
	}
	last_tick_time = current_tick_time;
	ticks++;

	const uint8_t shade = (uint8_t)((sinf(M_PIf * fmodf(ticks / (float)TICK_RATE, 1.0f)) * 0.25f + 0.25f) * 255.0f);
	if (!render_clear(shade, shade, shade, 255u)) {
		return false;
	}

	if (!reset_average) {
		if (!render_printf("font.fnt", 1u, 8.0f, 8.0f, "\
			Ticks: %" PRIu64 "\n\n\
			Current tick rate: %.9f\n\n\
			Average tick rate: %.9f\n\n\
			Total dynamic memory in use: %.04f MiB\n\n\
			Total physical memory available: %.04f MiB\n\n\
			Test text:\n%s",

			ticks,
			tick_rate,
			average_ticks / (average_duration / (double)NANOTIME_NSEC_PER_SEC),
			mem_total() / 1048576.0,
			mem_left() / 1048576.0,
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
			Total dynamic memory in use: %.04f MiB\n\n\
			Total physical memory available: %.04f MiB\n\n\
			Test text:\n%s",
			
			ticks,
			tick_rate,
			mem_total() / 1048576.0,
			mem_left() / 1048576.0,
			text
		)) {
			return false;
		}
	}

	sprite_type sprites[] = {
		{
			.src = { 0.0f, 0.0f, 16.0f, 16.0f },
			.dst = { 16.0f, 16.0f, 120.0f, 120.0f }
		},
		{
			.src = { 0.0f, 0.0f, 16.0f, 16.0f },
			.dst = { 16.0f + 120.0f - 30.0f, 16.0f + 120.0f - 30.0f, 60.0f, 60.0f }
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
