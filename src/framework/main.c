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

#include "framework/app.h"
#include "game/game.h"
#include "game/present.h"
#include "framework/nanotime.h"
#include "framework/defs.h"
#include "SDL.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

static SDL_atomic_t quit_now;

typedef struct shared_data_struct {
	SDL_atomic_t in_use;
	commands_struct* commands;
} shared_data_struct;

static shared_data_struct shared_data[3];
static SDL_sem* present_sem = NULL;
static shared_data_struct* shared_current = NULL;

int refresh_rate_get(SDL_Window* const window) {
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

int SDLCALL present(void* data) {
	bool inited = false;
	int status = 1;
	SDL_Window* const window = app_window_get();
	SDL_GLContext const context = app_context_create();

	if (context == NULL) {
		SDL_AtomicSet(&quit_now, 1);
		return 0;
	}

	nanotime_step_data stepper;
	int refresh_rate = refresh_rate_get(window);
	if (refresh_rate < 0) {
		SDL_AtomicSet(&quit_now, 1);
		return 0;
	}

	nanotime_step_init(&stepper, refresh_rate > 0 ? NANOTIME_NSEC_PER_SEC / refresh_rate : NANOTIME_NSEC_PER_SEC / 60, nanotime_now, nanotime_sleep);
	while (true) {
		if (SDL_SemWait(present_sem) < 0) {
			fprintf(stderr, "Error: %s\n", SDL_GetError());
			fflush(stderr);
			status = 0;
			SDL_AtomicSet(&quit_now, 1);
			break;
		}
		const uint64_t sleep_point = nanotime_now();
		if (SDL_AtomicGet(&quit_now)) {
			break;
		}

		// MEMACQ: shared_current
		shared_data_struct* const current = SDL_AtomicGetPtr((void**)&shared_current);
		SDL_AtomicSet(&current->in_use, 1);

		int refresh_rate = refresh_rate_get(window);
		if (refresh_rate < 0) {
			status = 0;
			SDL_AtomicSet(&current->in_use, 0);
			SDL_AtomicSet(&quit_now, 1);
			break;
		}

		// TODO: Implement configuration option for setting the game's frame rate.
		const uint64_t present_frame = refresh_rate > 0 ? NANOTIME_NSEC_PER_SEC / refresh_rate : NANOTIME_NSEC_PER_SEC / 60;
		if (present_frame != stepper.sleep_duration) {
			stepper.sleep_duration = present_frame;
			stepper.overhead_duration = 0u;
			stepper.sleep_point = sleep_point;
		}

		if (!inited && !(inited = present_init())) {
			status = 0;
			SDL_AtomicSet(&current->in_use, 0);
			SDL_AtomicSet(&quit_now, 1);
			break;
		}
		if (!commands_flush(current->commands)) {
			status = 0;
			SDL_AtomicSet(&current->in_use, 0);
			SDL_AtomicSet(&quit_now, 1);
			break;
		}
		SDL_GL_SwapWindow(window);

		// MEMREL: shared_data[i].in_use
		SDL_AtomicSet(&current->in_use, 0);

#ifdef NDEBUG
		nanotime_step(&stepper);
#endif
	}

	present_deinit();
	app_context_destroy(context);

	return status;
}

int SDLCALL event_filter(void* userdata, SDL_Event* event) {
	if (event->type == SDL_QUIT) {
		SDL_AtomicSet(&quit_now, 1);
	}
	return 1;
}

int main(int argc, char** argv) {
	if (!app_init(argc, argv)) {
		return EXIT_FAILURE;
	}

	int exit_code = EXIT_SUCCESS;

	SDL_AtomicSet(&quit_now, 0);

	for (size_t i = 0u; i < lengthof(shared_data); i++) {
		SDL_AtomicSet(&shared_data[i].in_use, 0);
		if ((shared_data[i].commands = commands_create()) == NULL) {
			for (size_t j = 0u; j < i; j++) {
				commands_destroy(shared_data[j].commands);
			}
			return EXIT_FAILURE;
		}
	}

	present_sem = SDL_CreateSemaphore(0u);
	if (present_sem == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		app_deinit();
		return EXIT_FAILURE;
	}

	shared_current = NULL;

	SDL_Thread* const present_thread = SDL_CreateThread(&present, "present_thread", NULL);
	if (present_thread == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_DestroySemaphore(present_sem);
		app_deinit();
		return EXIT_FAILURE;
	}

	if (!game_init()) {
		fflush(stderr);
		SDL_AtomicSet(&quit_now, 1);
		SDL_WaitThread(present_thread, NULL);
		SDL_DestroySemaphore(present_sem);
		app_deinit();
		return EXIT_FAILURE;
	}

	nanotime_step_data stepper;
	nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / TICK_RATE, nanotime_now, nanotime_sleep);

	while (SDL_PumpEvents(), SDL_FilterEvents(event_filter, NULL), !SDL_AtomicGet(&quit_now)) {
		shared_data_struct* const current = SDL_AtomicGetPtr((void**)&shared_current);

		size_t i;
		for (i = 0u; i < lengthof(shared_data); i++) {
			/*
			 * shared_data must contain at least three elements, because:
			 * 1. If shared_current == &shared_data[i], it's either the next to be used and isn't in use, or is currently in use; always use a shared_data element not equal to shared_current.
			 * 2. If shared_current != &shared_data[i], it's not the next to be used, but might be in use; always use a shared_data element that's not in use.
			 * At least one of the three elements is always guaranteed to not be in shared_current nor in use, because at most two elements are unavailable for a logic update.
			 * The acquisition memory barrier is here to be sure the selected command queue is no longer in use by the present thread when the game update begins.
			 */
			// MEMACQ: shared_data[i].in_use
			const bool break_now = &shared_data[i] != current && !SDL_AtomicGet(&shared_data[i].in_use);
			SDL_MemoryBarrierAcquire();
			if (break_now) {
				if (!commands_empty(shared_data[i].commands)) {
					fprintf(stderr, "Error: Failed to empty render command queue\n");
					fflush(stderr);
					exit_code = EXIT_FAILURE;
				}
				break;
			}
		}
		if (exit_code == EXIT_FAILURE) {
			break;
		}

		if (!game_update(shared_data[i].commands)) {
			fflush(stdout);
			exit_code = EXIT_FAILURE;
			break;
		}
		fflush(stdout);

		// MEMREL: shared_current
		SDL_AtomicSetPtr((void**)&shared_current, &shared_data[i]);

		if (SDL_SemValue(present_sem) == 0u && SDL_SemPost(present_sem) < 0) {
			fprintf(stderr, "Error: Failed to post the shared present data semaphore\n");
			fflush(stderr);
			exit_code = EXIT_FAILURE;
			break;
		}

		static uint64_t skips = 0u;
		if (!nanotime_step(&stepper)) {
			skips++;
			printf("skips == %" PRIu64 "\n", skips);
			fflush(stdout);
		}
	}

	SDL_AtomicSet(&quit_now, 1);

	game_deinit();

	if (SDL_SemPost(present_sem) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		exit_code = EXIT_FAILURE;
	}

	int status;
	SDL_WaitThread(present_thread, &status);
	if (status == 0) {
		exit_code = EXIT_FAILURE;
	}

	SDL_DestroySemaphore(present_sem);

	app_deinit();
	return exit_code;
}
