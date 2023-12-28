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
#include "main/private/app_private.h"
#include "util/private/mem_private.h"
#include "util/private/log_private.h"
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
	if (!set) {
		abort();
	}

	return main_thread_id;
}

bool this_thread_is_main_thread() {
	return SDL_ThreadID() == main_thread_id_get();
}

int main(int argc, char** argv) {
	/*
	 * From glancing at the SDL source code, it appears calling SDL_ThreadID,
	 * SDL_MemoryBarrierRelease, SDL_AtomicSet, SDL_AtomicGet, and
	 * SDL_MemoryBarrierAcquire before SDL_Init/after SDL_Quit is valid. And
	 * such operations can't fail, so it's safe for them to execute first thing.
	 *
	 * Some of the code must only be executed in the main thread, so it's
	 * critical that it be possible for all the code to check if it's being
	 * executed in the main thread as early as possible.
	 */
	main_thread_id = SDL_ThreadID();
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&main_thread_id_set, 1);

	if (!mem_init()) {
		return EXIT_FAILURE;
	}

	log_printf("Started program\n");

	/*
	 * Because so much of the code depends on SDL, SDL_Init needs to be called
	 * as early as possible.
	 */
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	if (!app_init(argc, argv)) {
		return EXIT_FAILURE;
	}

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
	log_printf("Shut down program\n");
	SDL_Quit();

	if (!mem_deinit()) {
		quit_status = QUIT_FAILURE;
	}

	return quit_status == QUIT_SUCCESS ?
		EXIT_SUCCESS :
		EXIT_FAILURE;
}
