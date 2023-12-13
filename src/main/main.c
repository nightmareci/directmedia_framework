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

#include "main/main.h"
#include "main/app.h"
#include "util/mem.h"
#include "game/game.h"
#include "SDL.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

static SDL_atomic_t main_thread_id_set = { 0 };
static SDL_threadID main_thread_id;
SDL_threadID main_thread_id_get() {
	const int set = SDL_AtomicGet(&main_thread_id_set);
	SDL_MemoryBarrierAcquire();
	assert(set);

	return main_thread_id;
}

int main(int argc, char** argv) {
	// From glancing at the SDL source code, it appears calling SDL_ThreadID
	// before SDL_Init/after SDL_Quit is valid.
	main_thread_id = SDL_ThreadID();
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&main_thread_id_set, 1);

	if (!mem_init()) {
		return EXIT_FAILURE;
	}

	if (!app_init(argc, argv)) {
		return EXIT_FAILURE;
	}

#define REALTIME

#ifdef REALTIME
	SDL_SetHint(SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL, "1");
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#endif

	quit_status_type quit_status;
	for (
		quit_status = app_update();
		quit_status == QUIT_NOT;
		quit_status = app_update()
	) {
	}

#ifdef REALTIME
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
	SDL_SetHint(SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL, "0");
#endif

	app_deinit();

	switch (quit_status) {
	case QUIT_SUCCESS:
		return EXIT_SUCCESS;

	default:
	case QUIT_FAILURE:
		return EXIT_FAILURE;
	}
}
