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
#include "main/main.h"
#include "opengl/opengl.h"
#include "util/text.h"
#include "util/log.h"
#include "util/nanotime.h"
#include "util/mem.h"
#include "render/render.h"
#include "game/game.h"
#include "lua/lauxlib.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <inttypes.h>
#include <assert.h>

static SDL_atomic_t quit_now = { QUIT_NOT };

static char* resource_path = NULL;
static char* save_path = NULL;
static bool paths_inited = false;

static bool img_inited = false;
static bool mix_inited = false;
static bool window_inited = false;
static bool sdl_inited = false;
static bool libs_inited = false;

static SDL_atomic_t sync_threads = { 0 };
static SDL_sem* sem_log_filename = NULL;
static SDL_sem* sem_render_start = NULL;
static SDL_sem* sem_init_game = NULL;
static SDL_sem* sem_deinit_render = NULL;
static SDL_sem* sem_render_after_tick = NULL;
static bool sems_inited = false;

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

static SDL_Thread* render_thread = NULL;
static frames_object* render_frames = NULL;
static bool render_thread_inited = false;
static uint64_t game_tick_duration;
static SDL_atomic_t game_thread_inited = { 0 };

static SDL_Window* window = NULL;

static SDL_SpinLock render_size_lock;
static int render_width = 0;
static int render_height = 0;

static SDL_SpinLock render_frame_rate_lock;
static double render_frame_rate = -1.0;

static nanotime_step_data main_stepper;

static SDL_atomic_t all_inited = { 0 };

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
 * 5. Once some thread indicates it's quitting time, the render thread breaks
 * out of its dequeue-and-render loop, waiting for the main thread to wake it up
 * after the main thread has also broken out of its game_update loop and is no
 * longer enqueueing frame objects.
 *
 * 6. To guarantee the render thread exits when it's quitting time, the main
 * thread does one final semaphore-post-to-render, also ensuring it did a
 * release-set of quit_now before the post, to guarantee the render thread's
 * get-acquire of quit_now indicates it's time to quit.
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

static bool paths_init(const int argc, char** const argv) {
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
			// TODO
			//log_printf("Error creating resource path string\n");
			app_deinit();
			return false;
		}
	}
	else {
		char* temp_string;

		if ((temp_string = SDL_GetBasePath()) == NULL) {
			// TODO
			//log_printf("Error getting SDL base path string: %s\n", SDL_GetError());
			app_deinit();
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
			// TODO
			//log_printf("Error creating resource path string\n");
			SDL_free(temp_string);
			app_deinit();
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
			// TODO
			//log_printf("Error creating save path string\n");
			app_deinit();
			return false;
		}
	}
	else {
		char* temp_string;

		if (portable_app) {
			if ((temp_string = SDL_GetBasePath()) == NULL) {
				// TODO
				//log_printf("Error getting SDL base path string: %s\n", SDL_GetError());
				app_deinit();
				return false;
			}
		}
		else if ((temp_string = SDL_GetPrefPath(app_organization, app_executable)) == NULL) {
			// TODO
			//log_printf("Error getting SDL pref path string: %s\n", SDL_GetError());
			app_deinit();
			return false;
		}
		SDL_MemoryBarrierRelease();
		if (SDL_AtomicSetPtr((void**)&save_path, alloc_sprintf("%s", temp_string)), save_path == NULL) {
			// TODO
			//log_printf("Error creating the save path string\n");
			SDL_free(temp_string);
			app_deinit();
			return false;
		}
		SDL_free(temp_string);
	}

	printf(
		"Resource path: %s\n"
		"Save path: %s\n",

		resource_path,
		save_path
	);
	fflush(stdout);

	paths_inited = true;
	return true;
}

static void paths_deinit() {
	if (save_path != NULL) mem_free(save_path);
	if (resource_path != NULL) mem_free(resource_path);

	save_path = NULL;
	resource_path = NULL;

	paths_inited = false;
}

static bool libs_init() {
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		app_deinit();
		return false;
	}
	sdl_inited = true;

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
		app_deinit();
		return false;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&window, SDL_CreateWindow(app_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL));
	if (SDL_AtomicGetPtr((void**)&window) == NULL) {
		log_printf("Error: %s\n", SDL_GetError());
		app_deinit();
		return false;
	}
	window_inited = true;
	SDL_AtomicLock(&render_size_lock);
	render_width = 640;
	render_height = 480;
	SDL_AtomicUnlock(&render_size_lock);

	if (Mix_Init(MIX_INIT_OGG | MIX_INIT_MOD) != (MIX_INIT_OGG | MIX_INIT_MOD)) {
		log_printf("Error: %s\n", Mix_GetError());
		app_deinit();
		return false;
	}
	mix_inited = true;

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		log_printf("Error: %s\n", IMG_GetError());
		app_deinit();
		return false;
	}
	img_inited = true;

	libs_inited = true;
	return true;
}

static void libs_deinit() {
	if (img_inited) IMG_Quit();
	if (mix_inited) Mix_Quit();
	if (window_inited) SDL_DestroyWindow(window);
	if (sdl_inited) SDL_Quit();

	img_inited = false;
	mix_inited = false;
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&window, NULL);
	sdl_inited = false;

	libs_inited = false;
}

static bool sems_init() {
	sem_log_filename = SDL_CreateSemaphore(0u);
	if (sem_log_filename == NULL) {
		log_printf("Error creating semaphore for communicating errors from the render thread to the main thread: %s\n", SDL_GetError());
		return false;
	}

	sem_render_start = SDL_CreateSemaphore(0u);
	if (sem_render_start == NULL) {
		log_printf("Error creating semaphore for halting the render thread upon start: %s\n", SDL_GetError());
		return false;
	}

	sem_init_game = SDL_CreateSemaphore(0u);
	if (sem_init_game == NULL) {
		log_printf("Error creating semaphore for halting the game thread before render_init: %s\n", SDL_GetError());
		return false;
	}

	sem_deinit_render = SDL_CreateSemaphore(0u);
	if (sem_deinit_render == NULL) {
		log_printf("Error creating semaphore for halting the render thread before render_deinit: %s\n", SDL_GetError());
		return false;
	}

	sem_render_after_tick = SDL_CreateSemaphore(0u);
	if (sem_render_after_tick == NULL) {
		log_printf("Error creating semaphore for halting the render thread before render_deinit: %s\n", SDL_GetError());
		return false;
	}

	sems_inited = true;
	return true;
}

static void sems_deinit() {
	if (sem_render_after_tick != NULL) SDL_DestroySemaphore(sem_render_after_tick);
	if (sem_deinit_render != NULL) SDL_DestroySemaphore(sem_deinit_render);
	if (sem_init_game != NULL) SDL_DestroySemaphore(sem_init_game);
	if (sem_render_start != NULL) SDL_DestroySemaphore(sem_render_start);
	if (sem_log_filename != NULL) SDL_DestroySemaphore(sem_log_filename);

	sem_log_filename = NULL;
	sem_render_start = NULL;
	sem_init_game = NULL;
	sem_deinit_render = NULL;
	sem_render_after_tick = NULL;

	sems_inited = false;
}

static uint64_t frame_duration_get() {
	SDL_Window* const window = app_window_get();
	const int display_index = SDL_GetWindowDisplayIndex(window);
	if (display_index < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		return 0u;
	}
	SDL_DisplayMode display_mode;
	if (SDL_GetDesktopDisplayMode(display_index, &display_mode) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		return 0u;
	}
	if (display_mode.refresh_rate <= 0) {
		return NANOTIME_NSEC_PER_SEC / 60u;
	}
	else {
		return NANOTIME_NSEC_PER_SEC / display_mode.refresh_rate;
	}
}

static int SDLCALL render_thread_func(void* data) {
	// Ensure quit_now in this thread is at the oldest the first value written
	// in the main thread.
	SDL_AtomicGet(&quit_now);
	SDL_MemoryBarrierAcquire();

	if (!log_filename_set("log_render.txt")) {
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_log_filename);
		return EXIT_FAILURE;
	}
	log_printf("Started render thread\n");
	SEM_POST(sem_log_filename);

	SEM_WAIT(sem_render_start);

	SDL_Window* const window = app_window_get();
	if (window == NULL) {
		log_printf("Error getting window in render thread\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	SDL_GLContext glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
	SDL_MemoryBarrierAcquire();
	if (glcontext == NULL) {
		log_printf("Error getting glcontext in render thread\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	if (SDL_GL_MakeCurrent(window, glcontext) < 0) {
		log_printf("Error making an OpenGL context current in render thread\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	if (SDL_GL_SetSwapInterval(0) < 0) {
		log_printf("Error setting swap interval in render thread\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	frames_object* const frames = frames_create();
	if (frames == NULL) {
		log_printf("Error creating frames object in render thread\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	if (!render_init(frames)) {
		log_printf("Error initializing the render API\n");
		SDL_AtomicSet(&quit_now, QUIT_FAILURE);
		frames_destroy(frames);
		SEM_POST(sem_init_game);
		return EXIT_FAILURE;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, frames);

	SEM_POST(sem_init_game);

	// The renderer only renders as often as the main thread requires, or less
	// often if the main thread generates frames faster than the frame rate of
	// the render thread. All frame objects are processed by the render thread,
	// so renderer objects will be valid for drawing whenever drawing is
	// required, but only the latest-produced frame will be drawn upon each
	// post.
	bool first_render = true;
	const uint64_t now_max = nanotime_now_max();
	nanotime_step_data stepper;
	int exit_code = EXIT_SUCCESS;

	// TODO: Investigate which of post/wait or always-loop produces the lowest
	// latency. Though if post/wait better ensures every game tick gets rendered
	// one-after-the-other, with none missed, when tick rate and frame rate
	// match at 60 Hz, then maybe consider offering both options.

	nanotime_step_init(&stepper, frame_duration_get(), now_max, nanotime_now, nanotime_sleep);
	while (true) {
		if (SDL_SemWait(sem_render_after_tick) < 0) {
			log_printf("Error waiting to render in the render thread: %s\n", SDL_GetError());
			SDL_AtomicSet(&quit_now, QUIT_FAILURE);
			exit_code = EXIT_FAILURE;
			break;
		}

		if (SDL_AtomicGet(&quit_now) != QUIT_NOT) {
			break;
		}

		const frames_status_type status = frames_draw_latest(frames);

		if (status == FRAMES_STATUS_ERROR) {
			log_printf("Error drawing latest frame in render thread\n");
			SDL_AtomicSet(&quit_now, QUIT_FAILURE);
			exit_code = EXIT_FAILURE;
			break;
		}

		if (status == FRAMES_STATUS_PRESENT) {
			if (first_render) {
				stepper.sleep_point = nanotime_now();
				stepper.accumulator = 0u;
				first_render = false;
			}
			const uint64_t last_time = stepper.sleep_point;
			const uint64_t frame_duration = frame_duration_get();
			if (!SDL_AtomicGet(&game_thread_inited) || frame_duration > game_tick_duration) {
				stepper.sleep_duration = frame_duration;
			}
			else {
				stepper.sleep_duration = game_tick_duration;
			}
			nanotime_step(&stepper);

			SDL_AtomicLock(&render_frame_rate_lock);
			render_frame_rate = (double)NANOTIME_NSEC_PER_SEC / nanotime_interval(last_time, stepper.sleep_point, now_max);
			SDL_AtomicUnlock(&render_frame_rate_lock);
		}
	}

	SEM_WAIT(sem_deinit_render);

	frames_destroy(frames);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, NULL);
	render_deinit();
	SDL_GL_MakeCurrent(window, NULL);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_glcontext, glcontext);
	if (exit_code == EXIT_SUCCESS) {
		log_printf("Successfully shut down render thread\n");
	}
	else {
		log_printf("Failed to validly shut down render thread\n");
	}
	return exit_code;
}

static bool render_thread_init() {
	assert(!render_thread_inited);

	if (!sems_init()) {
		return false;
	}

	SDL_GLContext glcontext = app_glcontext_create();
	if (glcontext == NULL) {
		return false;
	}
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_glcontext, glcontext);

	SEM_POST(sem_render_start);

	render_thread = SDL_CreateThread(render_thread_func, "render_thread", NULL);
	if (render_thread == NULL) {
		log_printf("Error creating render thread: %s\n", SDL_GetError());
		return false;
	}

	SEM_WAIT(sem_log_filename);
	if (SDL_AtomicGet(&quit_now) == QUIT_FAILURE) {
		log_printf("Error setting log filename in render thread\n");
		SDL_WaitThread(render_thread, NULL);
		render_thread = NULL;
		return false;
	}

	render_thread_inited = true;
	return true;
}

static void render_thread_deinit() {
	assert(render_thread_inited);

	if (sems_inited && render_thread != NULL) {
		SEM_POST(sem_deinit_render);

		if (SDL_SemPost(sem_render_after_tick) < 0) {
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

	SDL_GLContext const glcontext = SDL_AtomicGetPtr((void**)&main_thread_glcontext);
	SDL_MemoryBarrierAcquire();
	if (glcontext != NULL) app_glcontext_destroy(glcontext);
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&main_thread_glcontext, NULL);

	render_thread_inited = false;
}

static bool game_thread_init() {
	#ifndef NDEBUG
	assert(!SDL_AtomicGet(&game_thread_inited));
	SDL_MemoryBarrierAcquire();
	#endif

	SEM_WAIT(sem_init_game);
	if (SDL_AtomicGet(&quit_now) == QUIT_FAILURE) {
		log_printf("Error initializing render thread before starting the game\n");
		if (SDL_SemPost(sem_render_after_tick) < 0) {
			log_printf("Error posting the render thread while initializing the game: %s\n", SDL_GetError());
			return false;
		}
		SDL_WaitThread(render_thread, NULL);
		render_thread = NULL;
		return false;
	}

	// NOTE: This ensures that render_frames is valid to read nonatomically in
	// app_update.
	SDL_AtomicGetPtr((void**)&render_frames);
	SDL_MemoryBarrierAcquire();

	// TODO: Implement Lua scripting support for game code. For now, this
	// just tests that the Lua library is available and working.
	lua_State* const lua_state = lua_newstate(mem_lua_alloc, NULL);
	if (lua_state != NULL) {
		lua_close(lua_state);
	}
	else {
		log_printf("Error setting up Lua scripting for the game\n");
		return false;
	}

	if (!game_init(&game_tick_duration)) {
		log_printf("Error while initializing the game\n");
		return false;
	}

	nanotime_step_init(&main_stepper, game_tick_duration, nanotime_now_max(), nanotime_now, nanotime_sleep);

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&game_thread_inited, 1);
	return true;
}

bool app_init(const int argc, char** const argv) {
	#ifndef NDEBUG
	const int current_all_inited = SDL_AtomicGet(&all_inited);
	SDL_MemoryBarrierAcquire();
	assert(!current_all_inited);
	#endif
	assert(SDL_ThreadID() == main_thread_id_get());

	SDL_AtomicSet(&quit_now, QUIT_NOT);
	SDL_MemoryBarrierRelease();

	printf("Starting up\n");
	fflush(stdout);

	if (!paths_init(argc, argv)) {
		goto fail;
	}

	if (!log_init(NULL)) {
		goto fail;
	}

	if (!log_filename_set("log_main.txt")) {
		goto fail;
	}

	if (!libs_init()) {
		log_printf("Failed initializing app's libs\n");
		goto fail;
	}

	if (!render_thread_init()) {
		log_printf("Failed initializing render thread\n");
		goto fail;
	}

	if (!game_thread_init()) {
		log_printf("Failed initializing game thread\n");
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&all_inited, 1);
	log_printf("Successfully initialized app\n");
	return true;

	fail:
	app_deinit();
	return false;
}

void app_deinit() {
	assert(SDL_ThreadID() == main_thread_id_get());

	log_printf("Starting app deinitialization\n");

	SDL_MemoryBarrierRelease();
	SDL_AtomicCAS(&quit_now, QUIT_NOT, QUIT_SUCCESS);

	render_thread_deinit();

	sems_deinit();

	paths_deinit();

	libs_deinit();

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&all_inited, 0);

	printf("Shutting down\n");
	fflush(stdout);
}

bool app_inited() {
	const int current_all_inited = SDL_AtomicGet(&all_inited);
	SDL_MemoryBarrierAcquire();

	return !!current_all_inited;
}

SDL_Window* app_window_get() {
	SDL_Window* const current_window = SDL_AtomicGetPtr((void**)&window);
	SDL_MemoryBarrierAcquire();

	return current_window;
}

void app_render_size_get(size_t* const width, size_t* const height) {
	SDL_AtomicLock(&render_size_lock);
	*width = render_width;
	*height = render_height;
	SDL_AtomicUnlock(&render_size_lock);;
}

double app_render_frame_rate_get() {
	SDL_AtomicLock(&render_frame_rate_lock);
	const double frame_rate = render_frame_rate;
	SDL_AtomicUnlock(&render_frame_rate_lock);
	return frame_rate;
}

SDL_GLContext app_glcontext_create() {
	assert(SDL_ThreadID() == main_thread_id_get());

	SDL_Window* const current_window = app_window_get();
	assert(current_window != NULL);

	int context_major_version, context_minor_version, context_profile_mask;

	SDL_GLContext glcontext;
	if ((glcontext = SDL_GL_CreateContext(current_window)) == NULL) {
		log_printf("Error: %s\n", SDL_GetError());
		return NULL;
	}
	else if (
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &context_major_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &context_minor_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &context_profile_mask) < 0
	) {
		log_printf("Error: %s\n", SDL_GetError());
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (
		context_major_version < 3 ||
		(context_major_version == 3 && context_minor_version < 3) ||
		context_profile_mask != SDL_GL_CONTEXT_PROFILE_CORE
	) {
		log_printf("Error: OpenGL version is %d.%d %s, OpenGL 3.3 Core or higher is required\n",
			context_major_version, context_minor_version,
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_CORE ? "Core" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY ? "Compatibility" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_ES ? "ES" :
			"[UNKNOWN PROFILE TYPE]"
		);
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (!opengl_init()) {
		log_printf("Error: Failed to initialize OpenGL\n");
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	else if (SDL_GL_SetSwapInterval(0) < 0) {
		log_printf("Error: %s\n", SDL_GetError());
		SDL_GL_MakeCurrent(current_window, NULL);
		SDL_GL_DeleteContext(glcontext);
		return NULL;
	}
	SDL_GL_MakeCurrent(current_window, NULL);

	return glcontext;
}

void app_glcontext_destroy(SDL_GLContext const glcontext) {
	assert(SDL_ThreadID() == main_thread_id_get());
	assert(glcontext != NULL);

	SDL_GL_DeleteContext(glcontext);
}

const char* app_resource_path_get() {
	const char* const current_resource_path = SDL_AtomicGetPtr((void**)&resource_path);
	SDL_MemoryBarrierAcquire();
	return current_resource_path;
}

const char* app_save_path_get() {
	const char* const current_save_path = SDL_AtomicGetPtr((void**)&save_path);
	SDL_MemoryBarrierAcquire();
	return current_save_path;
}

static bool quit_app = false;

static int SDLCALL event_filter(void* userdata, SDL_Event* event) {
	switch (event->type) {
	case SDL_WINDOWEVENT:
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
		quit_app = true;
		break;

	default:
		break;
	}
	return 1;
}

quit_status_type app_update() {
	#ifndef NDEBUG
	const int current_all_inited = SDL_AtomicGet(&all_inited);
	SDL_MemoryBarrierAcquire();
	assert(current_all_inited);
	#endif
	assert(SDL_ThreadID() == main_thread_id_get());

	SDL_PumpEvents();
	SDL_FilterEvents(event_filter, NULL);
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	if (quit_app) {
		log_printf("Quitting app due to user app quit request\n");
		return QUIT_SUCCESS;
	}

	bool quit_game = false;
	if (!frames_start(render_frames)) {
		log_printf("Quitting due to render-frame-start error\n");
		return QUIT_FAILURE;
	}

	if (!game_update(&quit_game, main_stepper.sleep_point)) {
		log_printf("Quitting due to game update error\n");
		return QUIT_FAILURE;
	}

	if (!frames_end(render_frames)) {
		log_printf("Quitting due to render-frame-end error\n");
		return QUIT_FAILURE;
	}

	if (SDL_SemValue(sem_render_after_tick) == 0) {
		SDL_SemPost(sem_render_after_tick);
	}

	if (quit_game) {
		log_printf("Quitting due to in-game quit request\n");
		return QUIT_SUCCESS;
	}

	static uint64_t skips = 0u;
	if (!nanotime_step(&main_stepper)) {
		skips++;
		// This function only runs in the main thread, so stdout access is
		// allowed here, along with nonatomic static variables.
		printf("skips == %" PRIu64 "\n", skips);
		fflush(stdout);
	}

	return (quit_status_type)SDL_AtomicGet(&quit_now);
}
