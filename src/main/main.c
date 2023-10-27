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
#include "util/mem.h"
#include "render/render.h"
#include "lua/lauxlib.h"
#include "game/game.h"
#include "SDL.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

static SDL_atomic_t quit_now = { 0 };

static SDL_atomic_t sync_threads = { 0 };
static SDL_sem* sem_render_start = NULL;
static SDL_sem* sem_init_game = NULL;
static SDL_sem* sem_deinit_render = NULL;
static SDL_sem* sem_render_after_tick = NULL;

#define SEM_WAIT(sem) \
do { \
	SDL_SemWait((sem)); \
	SDL_AtomicGet(&sync_threads); \
	SDL_MemoryBarrierAcquire(); \
} while (false)

#define SEM_POST(sem) \
do { \
	SDL_MemoryBarrierRelease(); \
	SDL_AtomicSet(&sync_threads, 0); \
	SDL_SemPost((sem)); \
} while (false)

static frames_object* render_frames = NULL;

/*
 * Some platforms don't work properly when attempting to create an OpenGL
 * context in a non-main thread. Managing creation/destruction of the context in
 * the main thread does seem to work on all platforms, though. So, to
 * accommodate all platforms with a single solution, we can manage the lifetime
 * of the context as follows:
 *
 * 1. Ensure the render thread waits for the main thread to wake it up
 * initially, before attempting to use the glcontext at all.
 *
 * 2. Create the glcontext in the main thread and set no-glcontext as current
 * in the main thread.
 *
 * 3. Release-set the shared glcontext pointer to the now-ready-to-pass-off
 * glcontext.
 *
 * 3. Wake the waiting render thread.
 *
 * 4. Get-acquire the shared glcontext pointer in the render thread and make it
 * current in the render thread, while making no use of it in the main thread
 * while it's in use by the render thread.
 *
 * 5. Once quit_now is set to true by any thread, the render thread breaks out
 * of its dequeue-and-render loop, waiting for the main thread to wake it up
 * after the main thread has also broken out of its game_update loop and is no
 * longer enqueueing frame objects.
 *
 * 6. To guarantee the render thread exits when quit_now has been set true by
 * the main thread, the main thread does one final semaphore-post-to-render,
 * also ensuring it did a release-set of quit_now before the post, to guarantee
 * the render thread's get-acquire of quit_now is read as true.
 *
 * 7. The render thread does a release-set of the glcontext pointer to the same
 * value it already was, for later sync-to-main-thread via get-acquire of the
 * pointer.
 *
 * 8. Now, the main thread does the wait-for-render-thread-exit.
 *
 * 9. The main thread does a get-acquire of the shared glcontext pointer, to
 * guarantee the render thread's OpenGL operations have completed before
 * executing beyond its get-acquire.
 *
 * 10. The main thread now destroys the shared glcontext, as now it's
 * guaranteed to no longer be in use by the render thread.
 */
static SDL_GLContext main_thread_glcontext = NULL;

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

/*
 * TODO: Implement a way for non-main threads to get messages to stdout/stderr
 * in the main thread, and use that for printing error messages from those
 * threads.
 */

int SDLCALL render_thread_function(void* data) {
	SEM_WAIT(sem_render_start);

	SDL_Window* const window = app_window_get();
	if (window == NULL) {
		return EXIT_FAILURE;
	}

	SDL_GLContext glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
	SDL_MemoryBarrierAcquire();
	if (glcontext == NULL) {
		return EXIT_FAILURE;
	}

	if (SDL_GL_MakeCurrent(window, glcontext) < 0) {
		return EXIT_FAILURE;
	}

	if (SDL_GL_SetSwapInterval(0) < 0) {
		return EXIT_FAILURE;
	}

	frames_object* const frames = frames_create();
	if (frames == NULL) {
		return EXIT_FAILURE;
	}

	if (!render_init(frames)) {
		frames_destroy(frames);
		return EXIT_FAILURE;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, frames);

	SEM_POST(sem_init_game);

	// The renderer only renders as often as the main thread requires, or less
	// often if the main thread generates frames faster than the frame rate of
	// the render thread. All frame objects are processed by the render thread,
	// but only the latest-produced frame will be used for the latest present.
	bool first_render = true;
	uint64_t last_time;
	const uint64_t now_max = nanotime_now_max();
	nanotime_step_data stepper;
	int exit_code = EXIT_SUCCESS;
	while (true) {
		SDL_SemWait(sem_render_after_tick);

		const bool quit = !!SDL_AtomicGet(&quit_now);
		SDL_MemoryBarrierAcquire();
		if (quit) {
			break;
		}

		int frame_rate = frame_rate_get();
		if (frame_rate < 0) {
			SDL_AtomicSet(&quit_now, 1);
			exit_code = EXIT_FAILURE;
			break;
		}
		if (frame_rate == 0) {
			frame_rate = 60;
		}
		const uint64_t frame_duration = NANOTIME_NSEC_PER_SEC / frame_rate;

		uint64_t current_time = nanotime_now();
		if (first_render) {
			if (frames_draw_latest(frames) == FRAMES_STATUS_ERROR) {
				exit_code = EXIT_FAILURE;
				break;
			}
			nanotime_step_init(&stepper, frame_duration, now_max, nanotime_now, nanotime_sleep);
			first_render = false;
			last_time = current_time;
			stepper.sleep_point = current_time;
		}
		else if (nanotime_interval(last_time, current_time, now_max) >= frame_duration) {
			if (frames_draw_latest(frames) == FRAMES_STATUS_ERROR) {
				exit_code = EXIT_FAILURE;
				break;
			}
			last_time = current_time;
			stepper.sleep_point = current_time;
		}
		stepper.sleep_duration = frame_duration;
		nanotime_step(&stepper);
	}

	SEM_WAIT(sem_deinit_render);

	frames_destroy(frames);
	render_deinit();
	SDL_GL_MakeCurrent(window, NULL);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_glcontext, glcontext);
	return exit_code;
}

static int SDLCALL event_filter(void* userdata, SDL_Event* event) {
	if (event->type == SDL_QUIT) {
		SDL_AtomicSet(&quit_now, 1);
	}
	return 1;
}

int main(int argc, char** argv) {
	if (!mem_init()) {
		return EXIT_FAILURE;
	}

	int exit_code = EXIT_SUCCESS;

	if (!app_init(argc, argv)) {
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	SDL_Window* const window = app_window_get();
	if (window == NULL) {
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	sem_render_start = SDL_CreateSemaphore(0u);
	if (sem_render_start == NULL) {
		fprintf(stderr, "Error creating semaphore for halting the render thread upon start: %s\n", SDL_GetError());
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	sem_init_game = SDL_CreateSemaphore(0u);
	if (sem_init_game == NULL) {
		fprintf(stderr, "Error creating semaphore for halting the game thread before render_init: %s\n", SDL_GetError());
		SDL_DestroySemaphore(sem_render_start);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	sem_deinit_render = SDL_CreateSemaphore(0u);
	if (sem_deinit_render == NULL) {
		fprintf(stderr, "Error creating semaphore for halting the render thread before render_deinit: %s\n", SDL_GetError());
		SDL_DestroySemaphore(sem_init_game);
		SDL_DestroySemaphore(sem_render_start);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	sem_render_after_tick = SDL_CreateSemaphore(0u);
	if (sem_render_after_tick == NULL) {
		fprintf(stderr, "Error creating semaphore for halting the render thread before render_deinit: %s\n", SDL_GetError());
		SDL_DestroySemaphore(sem_deinit_render);
		SDL_DestroySemaphore(sem_init_game);
		SDL_DestroySemaphore(sem_render_start);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	SDL_GLContext glcontext = app_glcontext_create();
	if (glcontext == NULL) {
		SDL_DestroySemaphore(sem_render_after_tick);
		SDL_DestroySemaphore(sem_deinit_render);
		SDL_DestroySemaphore(sem_init_game);
		SDL_DestroySemaphore(sem_render_start);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}
	SDL_GL_MakeCurrent(window, NULL);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_glcontext, glcontext);

	SEM_POST(sem_render_start);

	SDL_Thread* const render_thread = SDL_CreateThread(render_thread_function, "render_thread", NULL);
	if (render_thread == NULL) {
		fprintf(stderr, "Error creating render thread: %s\n", SDL_GetError());
		SDL_DestroySemaphore(sem_render_after_tick);
		SDL_DestroySemaphore(sem_deinit_render);
		SDL_DestroySemaphore(sem_init_game);
		SDL_DestroySemaphore(sem_render_start);
		app_glcontext_destroy(glcontext);
		app_deinit();
		fflush(stdout);
		fflush(stderr);
		return EXIT_FAILURE;
	}

	SEM_WAIT(sem_init_game);

	frames_object* const frames = SDL_AtomicGetPtr((void**)&render_frames);
	SDL_MemoryBarrierAcquire();

	uint64_t tick_rate;
	if (!game_init(&tick_rate)) {
		SDL_AtomicSet(&quit_now, 1);
		SDL_WaitThread(render_thread, NULL);
		SDL_DestroySemaphore(sem_render_after_tick);
		SDL_DestroySemaphore(sem_deinit_render);
		SDL_DestroySemaphore(sem_init_game);
		SDL_DestroySemaphore(sem_render_start);
		glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
		SDL_MemoryBarrierAcquire();
		app_glcontext_destroy(glcontext);
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
			SDL_AtomicSet(&quit_now, 1);
			SDL_WaitThread(render_thread, NULL);
			SDL_DestroySemaphore(sem_render_after_tick);
			SDL_DestroySemaphore(sem_deinit_render);
			SDL_DestroySemaphore(sem_init_game);
			SDL_DestroySemaphore(sem_render_start);
			glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
			SDL_MemoryBarrierAcquire();
			app_glcontext_destroy(glcontext);
			app_deinit();
			fflush(stdout);
			fflush(stderr);
			return EXIT_FAILURE;
		}
	}

	nanotime_step_data stepper;
	nanotime_step_init(&stepper, NANOTIME_NSEC_PER_SEC / tick_rate, nanotime_now_max(), nanotime_now, nanotime_sleep);

	while ((SDL_PumpEvents(), SDL_FilterEvents(event_filter, NULL), SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT), !SDL_AtomicGet(&quit_now))) {
		if (frames_start(frames) == FRAMES_STATUS_ERROR) {
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			fflush(stdout);
			fflush(stderr);
			exit_code = EXIT_FAILURE;
			break;
		}

		bool quit_game = false;
		if (!game_update(&quit_game)) {
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			fflush(stdout);
			fflush(stderr);
			exit_code = EXIT_FAILURE;
			break;
		}

		if (!frames_end(frames)) {
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			fflush(stdout);
			fflush(stderr);
			exit_code = EXIT_FAILURE;
			break;
		}

		SDL_SemPost(sem_render_after_tick);

		fflush(stdout);
		fflush(stderr);

		if (quit_game) {
			SDL_MemoryBarrierRelease();
			SDL_AtomicSet(&quit_now, 1);
			fflush(stdout);
			fflush(stderr);
			break;
		}

		static uint64_t skips = 0u;
		if (!nanotime_step(&stepper)) {
			skips++;
			printf("skips == %" PRIu64 "\n", skips);
			fflush(stdout);
		}
	}

	SEM_POST(sem_deinit_render);

	SDL_SemPost(sem_render_after_tick);
	int render_status;
	SDL_WaitThread(render_thread, &render_status);
	SDL_DestroySemaphore(sem_render_after_tick);
	SDL_DestroySemaphore(sem_deinit_render);
	SDL_DestroySemaphore(sem_init_game);
	SDL_DestroySemaphore(sem_render_start);
	glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
	SDL_MemoryBarrierAcquire();
	app_glcontext_destroy(glcontext);
	app_deinit();
	fflush(stdout);
	fflush(stderr);

	if (exit_code == EXIT_SUCCESS && render_status == EXIT_SUCCESS) {
		return EXIT_SUCCESS;
	}
	else {
		return EXIT_FAILURE;
	}
}
