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
#include "util/nanotime.h"
#include "util/defs.h"
#include "render/render.h"
#include "lua/lauxlib.h"
#include "game/game.h"
#include "SDL.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

static int frame_rate_get() {
	SDL_Window* const window = app_window_get();
	const int display_index = SDL_GetWindowDisplayIndex(window);
	if (display_index < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		return -1;
	}
	SDL_DisplayMode display_mode;
	if (SDL_GetDesktopDisplayMode(display_index, &display_mode) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		return -1;
	}
	return display_mode.refresh_rate;
}

static int SDLCALL event_filter(void* userdata, SDL_Event* event) {
	bool* const quit_now = userdata;
	if (event->type == SDL_QUIT) {
		*quit_now = true;
	}
	return 1;
}

int main(int argc, char** argv) {
	int exit_code = EXIT_SUCCESS;

	if (!app_init(argc, argv)) {
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	SDL_GLContext context = app_glcontext_create();
	if (context == NULL) {
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	frames_status_enum frames_status;
	frames_struct* const frames = frames_create();
	if (frames == NULL) {
		app_glcontext_destroy(context);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	if (!render_init(frames)) {
		frames_destroy(frames);
		app_glcontext_destroy(context);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	uint64_t tick_rate;
	if (!game_init(&tick_rate)) {
		render_deinit();
		frames_destroy(frames);
		app_glcontext_destroy(context);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	// TODO: Implement Lua scripting support for game code. For now, this
	// just tests that the Lua library is available and working.
	{
		lua_State* const lua_state = luaL_newstate();
		if (lua_state != NULL) {
			lua_close(lua_state);
		}
		else {
			render_deinit();
			frames_destroy(frames);
			app_glcontext_destroy(context);
			app_deinit();
			fflush(stdout);
			fflush(stderr);
			return EXIT_FAILURE;
		}
	}

	bool first_render = true;
	uint64_t last_render;
	nanotime_step_data stepper;
	nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / tick_rate, nanotime_now_max(), nanotime_now, nanotime_sleep);

	bool quit_now = false;
	while (!quit_now && (SDL_PumpEvents(), SDL_FilterEvents(event_filter, &quit_now), !quit_now)) {
		frames_status = frames_start(frames);
		if (frames_status == FRAMES_STATUS_ERROR) {
			exit_code = EXIT_FAILURE;
			break;
		}

		if (!game_update(&quit_now)) {
			exit_code = EXIT_FAILURE;
			break;
		}
		fflush(stdout);

		frames_end(frames);

		int frame_rate = frame_rate_get();
		if (frame_rate < 0) {
			exit_code = EXIT_FAILURE;
			break;
		}
		if (frame_rate == 0) {
			frame_rate = 60;
		}

		const uint64_t frame_duration = NANOTIME_NSEC_PER_SEC / frame_rate;
		uint64_t now = nanotime_now();
		if (first_render || nanotime_interval(last_render, now, nanotime_now_max()) >= frame_duration) {
			frames_status = frames_draw_latest(frames);
			if (frames_status == FRAMES_STATUS_ERROR) {
				exit_code = EXIT_FAILURE;
				break;
			}
			last_render = now;
			first_render = false;
		}

		static uint64_t skips = 0u;
		if (!nanotime_step(&stepper)) {
			skips++;
			printf("skips == %" PRIu64 "\n", skips);
			fflush(stdout);
		}
	}

	render_deinit();
	if (!frames_destroy(frames)) {
		fprintf(stderr, "Error destroying the frames object\n");
	}
	app_glcontext_destroy(context);
	app_deinit();
	fflush(stdout);
	fflush(stderr);
	return exit_code;
}
