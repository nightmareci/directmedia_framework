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

#include "main/private/prog_private.h"
#include "audio/private/audio_private.h"
#include "render/private/render_private.h"
#include "render/private/opengl.h"
#include "util/private/log_private.h"
#include "util/private/mem_private.h"
#include "main/main.h"
#include "util/str.h"
#include "util/log.h"
#include "util/nanotime.h"
#include "app/app.h"
#include "lua/lauxlib.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <inttypes.h>
#include <assert.h>

static SDL_atomic_t quit_status = { QUIT_NOT };

static char* resource_path = NULL;
static char* save_path = NULL;
static bool paths_inited = false;

static bool img_inited_flag = false;
static bool mix_inited_flag = false;
static bool window_inited_flag = false;
static bool audio_inited_flag = false;
static bool libs_inited_flag = false;

static SDL_atomic_t sync_threads_mem_barrier = { 0 };
static SDL_sem* log_filename_sem = NULL;
static SDL_sem* render_start_sem = NULL;
static SDL_sem* init_app_sem = NULL;
static SDL_sem* deinit_render_sem = NULL;
static SDL_sem* render_now_sem = NULL;
static bool sems_inited_flag = false;

#define SEM_WAIT(sem) \
do { \
	SDL_SemWait((sem)); \
	SDL_AtomicGet(&sync_threads_mem_barrier); \
	SDL_MemoryBarrierAcquire(); \
} while (false)

#define SEM_POST(sem) \
do { \
	SDL_MemoryBarrierRelease(); \
	SDL_AtomicSet(&sync_threads_mem_barrier, 0); \
	SDL_SemPost((sem)); \
} while (false)

static SDL_Thread* render_thread = NULL;
static frames_object* render_frames = NULL;
static bool render_thread_inited = false;
static uint64_t app_tick_duration;
static SDL_atomic_t app_thread_inited = { 0 };
static SDL_TLSID thread_names = 0;

static SDL_Window* window = NULL;

static SDL_SpinLock render_size_lock;
static int render_width = 0;
static int render_height = 0;

/*
 * There are some situations where the main thread must signal to the render
 * thread to reinitialize its stepper object, to ensure correct render frame
 * pacing; this is used as a boolean shared variable to do such signalling.
 */
static SDL_atomic_t render_stepper_init_flag = { 0 };

/*
 * On platforms whose pointers are 64-bit or larger, we can use pointer atomics
 * instead of spinlocks. uintptr_t support is required for this to be fully
 * portable.
 */
#if UINTPTR_MAX < UINT64_MAX
#define SPINLOCK_FOR_UINT64
#endif

#ifdef SPINLOCK_FOR_UINT64
static SDL_SpinLock render_frame_duration_lock;
static uint64_t render_frame_duration = 0u;
#else
static void* render_frame_duration = NULL;
#endif

static nanotime_step_data main_stepper;

static SDL_atomic_t prog_inited_flag = { 0 };

/*
 * Some platforms don't work properly when attempting to create an OpenGL
 * context in a non-main thread. Managing creation/destruction of the context in
 * the main thread does seem to work on all platforms, though. So, to
 * accommodate all platforms with a single solution, we can manage the lifetime
 * of the context as follows:
 *
 * 1. Ensure the render thread waits for the main thread to wake it up
 * initially, before attempting to use the context at all.
 *
 * 2. Create the context in the main thread and set no-context as current
 * in the main thread.
 *
 * 3. Release-set the shared context pointer to the now-ready-to-pass-off
 * context.
 *
 * 3. Wake the waiting render thread.
 *
 * 4. Get-acquire the shared context pointer in the render thread and make it
 * current in the render thread, while making no use of it in the main thread
 * while it's in use by the render thread.
 *
 * 5. Once some thread indicates it's quitting time, the render thread breaks
 * out of its dequeue-and-render loop, waiting for the main thread to wake it up
 * after the main thread has also broken out of its app_update loop and is no
 * longer enqueueing frame objects.
 *
 * 6. To guarantee the render thread exits when it's quitting time, the main
 * thread does one final semaphore-post-to-render, also ensuring it did a
 * release-set of quit_status before the post, to guarantee the render thread's
 * get-acquire of quit_status indicates it's time to quit.
 *
 * 7. The render thread does a release-set of the context pointer to the same
 * value it already was, for later sync-to-main-thread via get-acquire of the
 * pointer.
 *
 * 8. Now, the main thread does the wait-for-render-thread-exit.
 *
 * 9. The main thread does a get-acquire of the shared context pointer, to
 * guarantee the render thread's OpenGL operations have completed before
 * executing beyond its get-acquire.
 *
 * 10. The main thread now destroys the shared context, as now it's
 * guaranteed to no longer be in use by the render thread.
 */
static opengl_context_object main_thread_opengl_context = NULL;

static bool paths_init(const int argc, char** const argv) {
	log_printf("Initializing data file paths\n");

	bool portable_app = false;
	const char* resource_path_override = NULL;
	const char* save_path_override = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--portable-app") == 0) {
			portable_app = true;
		}
		else if (strcmp(argv[i], "--resource-path") == 0 && argc > i + 1) {
			resource_path_override = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "--save-path") == 0 && argc > i + 1) {
			save_path_override = argv[i + 1];
			i++;
		}
	}

	if (!portable_app && resource_path_override != NULL) {
		const size_t len = strlen(resource_path_override);
		const char end = len > 0u ? resource_path_override[len - 1] : '\0';
		SDL_MemoryBarrierRelease();
		if (SDL_AtomicSetPtr((void**)&resource_path, alloc_sprintf("%s%s", resource_path_override,
			#ifdef _WIN32
			end == '\\' || end == '/' ? "" : "\\"
			#else
			end == '/' ? "" : "/"
			#endif
		)), resource_path == NULL) {
			log_printf("Error creating resource path string\n");
			prog_deinit();
			return false;
		}
	}
	else {
		char* temp_string;

		if ((temp_string = SDL_GetBasePath()) == NULL) {
			log_printf("Error getting SDL base path string: %s\n", SDL_GetError());
			prog_deinit();
			return false;
		}
		SDL_MemoryBarrierRelease();
		if (SDL_AtomicSetPtr((void**)&resource_path, alloc_sprintf("%sresource%s", temp_string,
			#ifdef _WIN32
			"\\"
			#else
			"/"
			#endif
		)), resource_path == NULL) {
			log_printf("Error creating resource path string\n");
			SDL_free(temp_string);
			prog_deinit();
			return false;
		}
		SDL_free(temp_string);
	}

	if (!portable_app && save_path_override != NULL) {
		const size_t len = strlen(save_path_override);
		const char end = len > 0u ? save_path_override[len - 1] : '\0';
		SDL_MemoryBarrierRelease();
		if (SDL_AtomicSetPtr((void**)&save_path, alloc_sprintf("%s%s", save_path_override,
			#ifdef _WIN32
			end == '\\' || end == '/' ? "" : "\\"
			#else
			end == '/' ? "" : "/"
			#endif
		)), save_path == NULL) {
			log_printf("Error creating save path string\n");
			prog_deinit();
			return false;
		}
	}
	else {
		char* temp_string;

		if (portable_app) {
			if ((temp_string = SDL_GetBasePath()) == NULL) {
				log_printf("Error getting SDL base path string: %s\n", SDL_GetError());
				prog_deinit();
				return false;
			}
		}
		else if ((temp_string = SDL_GetPrefPath(app_organization, app_executable)) == NULL) {
			log_printf("Error getting SDL pref path string: %s\n", SDL_GetError());
			prog_deinit();
			return false;
		}
		SDL_MemoryBarrierRelease();
		if (SDL_AtomicSetPtr((void**)&save_path, alloc_sprintf("%s", temp_string)), save_path == NULL) {
			log_printf("Error creating the save path string\n");
			SDL_free(temp_string);
			prog_deinit();
			return false;
		}
		SDL_free(temp_string);
	}

	log_printf(
		"Resource path: %s\n"
		"Save path: %s\n",

		resource_path,
		save_path
	);

	paths_inited = true;
	log_printf("Successfully initialized data file paths\n");
	return true;
}

static void paths_deinit() {
	if (!paths_inited) {
		return;
	}

	if (save_path != NULL) {
		mem_free(save_path);
	}
	if (resource_path != NULL) {
		mem_free(resource_path);
	}

	save_path = NULL;
	resource_path = NULL;

	paths_inited = false;
}

static bool libs_init() {
	log_printf("Initializing libraries\n");

	if (
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) < 0
	) {
		log_printf("Error: %s\n", SDL_GetError());
		prog_deinit();
		return false;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&window, SDL_CreateWindow(app_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL));
	if (SDL_AtomicGetPtr((void**)&window) == NULL) {
		log_printf("Error: %s\n", SDL_GetError());
		prog_deinit();
		return false;
	}
	window_inited_flag = true;
	SDL_AtomicLock(&render_size_lock);
	render_width = 640;
	render_height = 480;
	SDL_AtomicUnlock(&render_size_lock);

	if (Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_MOD) != (MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_MOD)) {
		log_printf("Error: %s\n", Mix_GetError());
		prog_deinit();
		return false;
	}
	mix_inited_flag = true;

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		log_printf("Error: %s\n", IMG_GetError());
		prog_deinit();
		return false;
	}
	img_inited_flag = true;

	libs_inited_flag = true;
	log_printf("Successfully initialized libraries\n");

	return true;
}

static void libs_deinit() {
	if (!libs_inited_flag) {
		return;
	}

	if (img_inited_flag) {
		IMG_Quit();
	}
	if (mix_inited_flag) {
		Mix_Quit();
	}
	if (window_inited_flag) {
		SDL_DestroyWindow(window);
	}

	img_inited_flag = false;
	mix_inited_flag = false;
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&window, NULL);

	libs_inited_flag = false;
}

static bool sems_init() {
	log_printf("Initializing program semaphores\n");

	log_filename_sem = SDL_CreateSemaphore(0u);
	if (log_filename_sem == NULL) {
		log_printf("Error creating semaphore for communicating errors from the render thread to the main thread: %s\n", SDL_GetError());
		return false;
	}

	render_start_sem = SDL_CreateSemaphore(0u);
	if (render_start_sem == NULL) {
		log_printf("Error creating semaphore for halting the render thread upon start: %s\n", SDL_GetError());
		return false;
	}

	init_app_sem = SDL_CreateSemaphore(0u);
	if (init_app_sem == NULL) {
		log_printf("Error creating semaphore for halting the app thread before render_init: %s\n", SDL_GetError());
		return false;
	}

	deinit_render_sem = SDL_CreateSemaphore(0u);
	if (deinit_render_sem == NULL) {
		log_printf("Error creating semaphore for halting the render thread before render_deinit: %s\n", SDL_GetError());
		return false;
	}

	render_now_sem = SDL_CreateSemaphore(0u);
	if (render_now_sem == NULL) {
		log_printf("Error creating semaphore for waking the render thread to render frames: %s\n", SDL_GetError());
		return false;
	}

	sems_inited_flag = true;
	log_printf("Successfully initialized program semaphores\n");

	return true;
}

static void sems_deinit() {
	if (!sems_inited_flag) {
		return;
	}

	if (render_now_sem != NULL) SDL_DestroySemaphore(render_now_sem);
	if (deinit_render_sem != NULL) SDL_DestroySemaphore(deinit_render_sem);
	if (init_app_sem != NULL) SDL_DestroySemaphore(init_app_sem);
	if (render_start_sem != NULL) SDL_DestroySemaphore(render_start_sem);
	if (log_filename_sem != NULL) SDL_DestroySemaphore(log_filename_sem);

	log_filename_sem = NULL;
	render_start_sem = NULL;
	init_app_sem = NULL;
	deinit_render_sem = NULL;
	render_now_sem = NULL;

	sems_inited_flag = false;
}

static uint64_t frame_duration_get() {
	SDL_Window* const window = prog_window_get();
	const int display_index = SDL_GetWindowDisplayIndex(window);
	if (display_index < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		return NANOTIME_NSEC_PER_SEC / 60u;
	}
	SDL_DisplayMode display_mode;
	if (SDL_GetDesktopDisplayMode(display_index, &display_mode) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		return NANOTIME_NSEC_PER_SEC / 60u;
	}
	if (display_mode.refresh_rate <= 0) {
		return NANOTIME_NSEC_PER_SEC / 60u;
	}
	else {
		return NANOTIME_NSEC_PER_SEC / display_mode.refresh_rate;
	}
}

static int SDLCALL render_thread_func(void* data) {
	// Ensure quit_status in this thread is at the oldest the first value
	// written in the main thread.
	SDL_AtomicGet(&quit_status);
	SDL_MemoryBarrierAcquire();

	if (!prog_this_thread_name_set("render")) {
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(log_filename_sem);
		return EXIT_FAILURE;
	}

#ifndef STDOUT_LOG
	if (!log_filename_set("log_render.txt")) {
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(log_filename_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully set render thread's log filename (log_render.txt)\n");
#endif

	SEM_POST(log_filename_sem);

	SEM_WAIT(render_start_sem);

	log_printf("Getting window object for rendering\n");
	SDL_Window* const window = prog_window_get();
	if (window == NULL) {
		log_printf("Error getting window object for rendering\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully got window object for rendering\n");

	log_printf("Getting OpenGL context for rendering\n");
	opengl_context_object context = SDL_AtomicGetPtr((void**)&main_thread_opengl_context);
	SDL_MemoryBarrierAcquire();
	if (context == NULL) {
		log_printf("Error getting OpenGL context for rendering\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully got OpenGL context for rendering\n");

	log_printf("Making an OpenGL context current for rendering\n");
	if (!opengl_context_make_current(context)) {
		log_printf("Error making an OpenGL context current for rendering\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully made an OpenGL context current for rendering\n");

	log_printf("Setting the swap interval for OpenGL screen presents\n");
	if (SDL_GL_SetSwapInterval(0) < 0) {
		log_printf("Error setting swap interval for OpenGL screen presents\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully set the swap interval for OpenGL screen presents\n");

	log_printf("Creating the render frames object\n");
	frames_object* const frames = frames_create();
	if (frames == NULL) {
		log_printf("Error creating render frames object\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}
	log_printf("Successfully created the render frames object\n");

	if (!render_init(frames)) {
		log_printf("Error initializing the render API\n");
		SDL_AtomicSet(&quit_status, QUIT_FAILURE);
		frames_destroy(frames);
		SEM_POST(init_app_sem);
		return EXIT_FAILURE;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, frames);

	SEM_POST(init_app_sem);

	log_printf("Entering the render loop\n");

	// The renderer only renders as often as the main thread requires, or less
	// often if the main thread generates frames faster than the frame rate of
	// the render thread. All frame objects are processed by the render thread,
	// so renderer objects will be valid for drawing whenever drawing is
	// required, but only the latest-produced frame will be drawn upon each
	// post.

	// TODO: Investigate which of post/wait or always-loop produces the lowest
	// latency. Though if post/wait better ensures every app tick gets rendered
	// one-after-the-other, with none missed, when tick rate and frame rate
	// match at 60 Hz, then maybe consider offering both options.

	const uint64_t now_max = nanotime_now_max();
	nanotime_step_data stepper;
	int exit_code = EXIT_SUCCESS;

	stepper.sleep_duration = 0u;
	bool skipped = false;
	frames_status_type last_status = FRAMES_STATUS_NO_PRESENT;
	while (true) {
		if (SDL_SemWait(render_now_sem) < 0) {
			log_printf("Error waiting to render in the render thread: %s\n", SDL_GetError());
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			exit_code = EXIT_FAILURE;
			break;
		}

		if (SDL_AtomicGet(&quit_status) != QUIT_NOT) {
			break;
		}

		const frames_status_type frames_status = frames_draw_latest(frames);
		if (frames_status == FRAMES_STATUS_ERROR) {
			log_printf("Error drawing latest frame\n");
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			exit_code = EXIT_FAILURE;
			break;
		}

		const uint64_t frame_duration = frame_duration_get();
		uint64_t max_duration;
		if (SDL_AtomicGet(&app_thread_inited) && app_tick_duration > frame_duration) {
			max_duration = app_tick_duration;
		}
		else {
			max_duration = frame_duration;
		}

		// We don't need to use the skipping feature of nanotime_step, so we
		// just reinitialize the stepper object upon skips. The goal is to pace
		// out all renders, we don't need to maintain correct timing of render
		// frames by skipping sleep steps, as is desired of app ticks.
		//
		// However, we *do* want to take advantage of the accumulator
		// accounting in nanotime_step, so we don't want to always reinitialize
		// the stepper, which resets the accumulator to zero.
		if (skipped || stepper.sleep_duration != max_duration || last_status != FRAMES_STATUS_PRESENT || SDL_AtomicCAS(&render_stepper_init_flag, 1, 0)) {
			nanotime_step_init(&stepper, max_duration, now_max, nanotime_now, nanotime_sleep);
		}
		last_status = frames_status;

		const uint64_t start = stepper.sleep_point;
		skipped = !nanotime_step(&stepper);
		const uint64_t interval = nanotime_interval(start, stepper.sleep_point, now_max);
#ifdef SPINLOCK_FOR_UINT64
		SDL_AtomicLock(&render_frame_rate_lock);
		render_frame_duration = interval;
		SDL_AtomicUnlock(&render_frame_rate_lock);
#else
		SDL_AtomicSetPtr(&render_frame_duration, (void*)(uintptr_t)interval);
#endif
	}

	log_printf("Broke out of the render loop\n");

	SEM_WAIT(deinit_render_sem);

	frames_destroy(frames);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, NULL);
	render_deinit();
	opengl_context_make_current(NULL);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_opengl_context, context);
	if (exit_code == EXIT_SUCCESS) {
		log_printf("Successfully shut down the render thread\n");
	}
	else {
		log_printf("Failed to shut down the render thread\n");
	}
	return exit_code;
}

static bool render_thread_init() {
	log_printf("Starting up the render thread\n");

	assert(!render_thread_inited);

	if (!sems_init()) {
		return false;
	}

	opengl_context_object context = opengl_context_create();
	if (context == NULL) {
		log_printf("Error creating the render thread's OpenGL context\n");
		return false;
	}
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_opengl_context, context);

	SEM_POST(render_start_sem);

	log_printf("Creating the render thread\n");
	render_thread = SDL_CreateThread(render_thread_func, "render_thread", NULL);
	if (render_thread == NULL) {
		log_printf("Error creating render thread: %s\n", SDL_GetError());
		return false;
	}
	log_printf("Successfully created the render thread\n");

	SEM_WAIT(log_filename_sem);
	if (SDL_AtomicGet(&quit_status) == QUIT_FAILURE) {
		log_printf("Error setting log filename in render thread\n");
		SDL_WaitThread(render_thread, NULL);
		render_thread = NULL;
		return false;
	}

	render_thread_inited = true;
	log_printf("Successfully started up the render thread\n");

	return true;
}

static void render_thread_deinit() {
	if(!render_thread_inited) {
		return;
	}

	if (sems_inited_flag && render_thread != NULL) {
		SEM_POST(deinit_render_sem);

		if (SDL_SemPost(render_now_sem) < 0) {
			log_printf("Error posting the render thread while deinitializing the renderer: %s\n", SDL_GetError());
			abort();
		}
		int render_status;
		SDL_WaitThread(render_thread, &render_status);
		render_thread = NULL;
		if (render_status == EXIT_FAILURE) {
			log_printf("Error shutting down the renderer\n");
			abort();
		}
	}

	opengl_context_object const context = SDL_AtomicGetPtr((void**)&main_thread_opengl_context);
	SDL_MemoryBarrierAcquire();
	if (context != NULL) opengl_context_destroy(context);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_opengl_context, NULL);

	render_thread_inited = false;
}

static bool app_thread_init() {
	log_printf("Initializing the app thread\n");

	#ifndef NDEBUG
	assert(!SDL_AtomicGet(&app_thread_inited));
	SDL_MemoryBarrierAcquire();
	#endif

	SEM_WAIT(init_app_sem);
	if (SDL_AtomicGet(&quit_status) == QUIT_FAILURE) {
		log_printf("Error initializing render thread before starting the app\n");
		if (SDL_SemPost(render_now_sem) < 0) {
			log_printf("Error posting the render thread to render a frame while initializing the app: %s\n", SDL_GetError());
			return false;
		}
		SDL_WaitThread(render_thread, NULL);
		render_thread = NULL;
		return false;
	}

	// NOTE: This ensures that render_frames is valid to read nonatomically in
	// prog_update.
	SDL_AtomicGetPtr((void**)&render_frames);
	SDL_MemoryBarrierAcquire();

	log_printf("Initializing Lua scripting support\n");
	// TODO: Implement Lua scripting support for app code. For now, this just
	// tests that the Lua library is available and working.
	lua_State* const lua_state = lua_newstate(mem_lua_alloc, NULL);
	if (lua_state != NULL) {
		lua_close(lua_state);
	}
	else {
		log_printf("Error setting up Lua scripting for the app\n");
		return false;
	}
	log_printf("Successfully initialized Lua scripting support\n");

	log_printf("Initializing the app API\n");
	if (!app_init(&app_tick_duration)) {
		log_printf("Error while initializing the app\n");
		return false;
	}
	log_printf("Successfully initialized the app API\n");

	nanotime_step_init(&main_stepper, app_tick_duration, nanotime_now_max(), nanotime_now, nanotime_sleep);

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&app_thread_inited, 1);

	log_printf("Successfully initialized the app thread\n");

	return true;
}

bool prog_init(const int argc, char** const argv) {
	log_printf("Initializing the program\n");

	const int current_all_inited = SDL_AtomicGet(&prog_inited_flag);
	SDL_MemoryBarrierAcquire();
	assert(!current_all_inited);
	if (current_all_inited) {
		log_printf("Erroneously attempted to call prog_init while the program is not currently uninitialized\n");
		goto fail;
	}

	const bool is_main_thread = main_thread_is_this_thread();
	assert(is_main_thread);
	if (!is_main_thread) {
		log_printf("Erroneously attempted to call prog_init in a non-main thread\n");
		goto fail;
	}

	SDL_AtomicSet(&quit_status, QUIT_NOT);
	SDL_MemoryBarrierRelease();

	log_printf("Creating thread names TLS\n");
	thread_names = SDL_TLSCreate();
	assert(thread_names != 0);
	if (thread_names == 0) {
		log_printf("Error creating thread local storage for thread names\n");
		goto fail;
	}
	log_printf("Successfully created thread names TLS\n");

	log_printf("Setting main thread's name\n");
	{
		const bool set = prog_this_thread_name_set("main");
		assert(set);
		if (!set) {
			log_printf("Error setting main thread's name\n");
			goto fail;
		}
	}
	log_printf("Successfully set main thread's name\n");

	{
		const bool inited = paths_init(argc, argv);
		assert(inited);
		if (!inited) {
			goto fail;
		}
	}

	log_printf("Initializing thread-safe log support\n");
#ifdef STDOUT_LOG
	{
		const bool inited = log_init("stdout");
		assert(inited);
		if (!inited) {
			log_printf("Failed initializing unified thread-safe logging to stdout\n");
			goto fail;
		}
	}
	log_printf("Successfully initialized log support\n");
#else
	{
		const bool inited = log_init(NULL);
		assert(inited);
		if (!inited) {
			log_printf("Failed initializing file-per-thread logging\n");
			goto fail;
		}
	}
	log_printf("Successfully initialized thread-safe log support\n");

	log_printf("Setting log filename for the main thread (log_main.txt)\n");
	{
		const bool set = log_filename_set("log_main.txt");
		assert(set);
		if (!set) {
			log_printf("Failed setting the log filename for the main thread\n");
			goto fail;
		}
	}
	log_printf("Successfully set log filename for the main thread (log_main.txt)\n");
#endif

	{
		const bool inited = libs_init();
		assert(inited);
		if (!inited) {
			goto fail;
		}
	}

	audio_inited_flag = audio_init();
	assert(audio_inited_flag);
	if (!audio_inited_flag) {
		goto fail;
	}

	{
		const bool inited = render_thread_init();
		assert(inited);
		if (!inited) {
			goto fail;
		}
	}

	{
		const bool inited = app_thread_init();
		assert(inited);
		if (!inited) {
			goto fail;
		}
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&prog_inited_flag, 1);
	log_printf("Successfully initialized the program\n");

	return true;

	fail:
	prog_deinit();
	return false;
}

void prog_deinit() {
	assert(main_thread_is_this_thread());

	render_thread_deinit();

	sems_deinit();

	if (audio_inited_flag) {
		audio_deinit();
		audio_inited_flag = false;
	}

	paths_deinit();

#ifdef STDOUT_LOG
	if (!log_all_output_deinit()) {
		printf("Error deinitializing the unified log output\n");
		SDL_MemoryBarrierRelease();
		SDL_AtomicCAS(&quit_status, QUIT_NOT, QUIT_FAILURE);
	}
#endif

	libs_deinit();

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&prog_inited_flag, 0);

	SDL_MemoryBarrierRelease();
	SDL_AtomicCAS(&quit_status, QUIT_NOT, QUIT_SUCCESS);
}

bool prog_inited() {
	const int current_prog_inited_flag = SDL_AtomicGet(&prog_inited_flag);
	SDL_MemoryBarrierAcquire();

	return !!current_prog_inited_flag;
}

SDL_Window* prog_window_get() {
	SDL_Window* const current_window = SDL_AtomicGetPtr((void**)&window);
	SDL_MemoryBarrierAcquire();

	return current_window;
}

void prog_render_size_get(size_t* const width, size_t* const height) {
	SDL_AtomicLock(&render_size_lock);
	*width = render_width;
	*height = render_height;
	SDL_AtomicUnlock(&render_size_lock);;
}

uint64_t prog_render_frame_duration_get() {
#ifdef SPINLOCK_FOR_UINT64
	SDL_AtomicLock(&render_frame_duration_lock);
	const double frame_duration = render_frame_duration;
	SDL_AtomicUnlock(&render_frame_duration_lock);
	return frame_duration;
#else
	return (uint64_t)(uintptr_t)SDL_AtomicGetPtr(&render_frame_duration);
#endif
}

static void SDLCALL thread_name_destructor(void* name) {
	mem_free(name);
}

bool prog_this_thread_name_set(const char* const name) {
	assert(thread_names != 0);

	char* const old_name = SDL_TLSGet(thread_names);
	if (old_name != NULL) {
		mem_free(old_name);
		const bool set = SDL_TLSSet(thread_names, NULL, NULL) >= 0;
		assert(set);
		if (!set) {
			log_printf("Error unsetting the current thread's name: %s\n", SDL_GetError());
			return false;
		}
	}

	if (name == NULL) {
		return true;
	}

	char* const tls_name = alloc_sprintf("%s", name);
	assert(tls_name != NULL);
	if (tls_name == NULL) {
		log_printf("Error allocating string for setting %s thread's name\n", name);
		return false;
	}

	{
		const bool set = SDL_TLSSet(thread_names, tls_name, thread_name_destructor) >= 0;
		assert(set);
		if (!set) {
			log_printf("Error setting %s thread's name: %s\n", name, SDL_GetError());
			mem_free(tls_name);
			return false;
		}
	}

	return true;
}

const char* const prog_this_thread_name_get() {
	assert(thread_names != 0);

	return SDL_TLSGet(thread_names);
}

const char* prog_resource_path_get() {
	const char* const current_resource_path = SDL_AtomicGetPtr((void**)&resource_path);
	SDL_MemoryBarrierAcquire();
	return current_resource_path;
}

const char* prog_save_path_get() {
	const char* const current_save_path = SDL_AtomicGetPtr((void**)&save_path);
	SDL_MemoryBarrierAcquire();
	return current_save_path;
}

static bool quit_prog = false;

static int SDLCALL event_filter(void* userdata, SDL_Event* event) {
	switch (event->type) {
	case SDL_WINDOWEVENT:
		SDL_AtomicSet(&render_stepper_init_flag, 1);
		switch (event->window.event) {
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			SDL_AtomicLock(&render_size_lock);
			render_width = event->window.data1;
			render_height = event->window.data2;
			SDL_AtomicUnlock(&render_size_lock);
			break;

		default:
			break;
		}
		break;

	case SDL_QUIT:
		quit_prog = true;
		break;

	default:
		break;
	}
	return 1;
}

quit_status_type prog_update() {
	#ifndef NDEBUG
	const int prog_inited = SDL_AtomicGet(&prog_inited_flag);
	SDL_MemoryBarrierAcquire();
	assert(prog_inited);
	#endif
	assert(main_thread_is_this_thread());

	SDL_PumpEvents();
	SDL_FilterEvents(event_filter, NULL);
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	if (quit_prog) {
		log_printf("Quitting program due to a program quit request\n");
		SDL_AtomicSet(&quit_status, QUIT_SUCCESS);
		goto quit;
	}

	{
		const bool started = frames_start(render_frames);
		assert(started);
		if (!started) {
			log_printf("Quitting due to a render-frame-start error\n");
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			goto quit;
		}
	}

	bool quit_app = false;
	{
		const bool updated = app_update(&quit_app, main_stepper.sleep_point);
		assert(updated);
		if (!updated) {
			log_printf("Quitting due to an app update error\n");
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			goto quit;
		}
	}

	{
		const bool ended = frames_end(render_frames);
		assert(ended);
		if (!ended) {
			log_printf("Quitting due to a render-frame-end error\n");
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			goto quit;
		}
	}

	if (SDL_SemValue(render_now_sem) == 0) {
		SDL_SemPost(render_now_sem);
	}

#ifdef STDOUT_LOG
	{
		const bool errored = main_stepper.accumulator < main_stepper.sleep_duration && !log_all_output_dequeue(main_stepper.sleep_duration - main_stepper.accumulator);
		assert(!errored);
		if (errored) {
			printf("Quitting due to an error in outputting messages to the unified log output\n");
			SDL_AtomicSet(&quit_status, QUIT_FAILURE);
			goto quit;
		}
	}
#endif

	if (quit_app) {
		log_printf("Quitting due to an in-app quit request\n");
		SDL_AtomicSet(&quit_status, QUIT_SUCCESS);
		goto quit;
	}

	if (!nanotime_step(&main_stepper)) {
		SDL_AtomicSet(&render_stepper_init_flag, 1);
		/*
		 * This function only runs in the main thread, so static variables are
		 * allowed.
		 */
		static uint64_t skips = 0u;
		skips++;
		log_printf("Skipped %" PRIu64 " app tick sleeps so far\n", skips);
	}

	quit:
	return (quit_status_type)SDL_AtomicGet(&quit_status);
}
