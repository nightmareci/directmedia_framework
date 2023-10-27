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
#include "opengl/opengl.h"
#include "util/text.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <assert.h>

static bool inited = false;
/*
 * The window pointer is accessed in threads other than the thread that set it,
 * so it must be accessed atomically, via SDL_AtomicGetPtr/SDL_AtomicSetPtr with
 * corresponding SDL_MemoryBarrierAcquire/SDL_MemoryBarrierRelease.
 */
static SDL_Window* window = NULL;
static char* resource_path = NULL;
static char* save_path = NULL;

bool app_init(const int argc, char** const argv) {
	assert(!inited);

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
		if ((resource_path = alloc_sprintf("%s%s", resource_path_override,
			#ifdef _WIN32
			end == '\\' || end == '/' ? "" : "\\"
			#else
			end == '/' ? "" : "/"
			#endif
		)) == NULL) {
			return false;
		}
	}
	else {
		char* temp_string;

		if ((temp_string = SDL_GetBasePath()) == NULL) {
			return false;
		}
		if ((resource_path = alloc_sprintf("%sresource%s", temp_string,
			#ifdef _WIN32
			"\\"
			#else
			"/"
			#endif
		)) == NULL) {
			SDL_free(temp_string);
			return false;
		}
		SDL_free(temp_string);
	}

	if (!portable_app && save_path_override != NULL) {
		const size_t len = strlen(save_path_override);
		const char end = len > 0u ? save_path_override[len - 1] : '\0';
		if ((save_path = alloc_sprintf("%s%s", save_path_override,
			#ifdef _WIN32
			end == '\\' || end == '/' ? "" : "\\"
			#else
			end == '/' ? "" : "/"
			#endif
		)) == NULL) {
			mem_free(resource_path);
			return false;
		}
	}
	else {
		char* temp_string;

		if (portable_app) {
			if ((temp_string = SDL_GetBasePath()) == NULL) {
				mem_free(resource_path);
				return false;
			}
		}
		else if ((temp_string = SDL_GetPrefPath(app_organization, app_executable)) == NULL) {
			mem_free(resource_path);
			return false;
		}
		if ((save_path = alloc_sprintf("%s", temp_string)) == NULL) {
			SDL_free(temp_string);
			mem_free(resource_path);
			return false;
		}
		SDL_free(temp_string);
	}

	printf(
		"App data cache resource path: %s\n"
		"App data cache save path: %s\n",

		resource_path,
		save_path
	);
	fflush(stdout);

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		mem_free(save_path);
		mem_free(resource_path);
		return false;
	}
	else if (
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) < 0
	) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_Quit();
		mem_free(save_path);
		mem_free(resource_path);
		return false;
	}
	{
		SDL_MemoryBarrierRelease();
		SDL_AtomicSetPtr((void**)&window, SDL_CreateWindow(app_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL));
		if (SDL_AtomicGetPtr((void**)&window) == NULL) {
			fprintf(stderr, "Error: %s\n", SDL_GetError());
			fflush(stderr);
			SDL_Quit();
			mem_free(save_path);
			mem_free(resource_path);
			return false;
		}
	}
	if (Mix_Init(MIX_INIT_OGG | MIX_INIT_MOD) != (MIX_INIT_OGG | MIX_INIT_MOD)) {
		fprintf(stderr, "Error: %s\n", Mix_GetError());
		fflush(stderr);
		SDL_DestroyWindow(SDL_AtomicGetPtr((void**)&window));
		SDL_Quit();
		mem_free(save_path);
		mem_free(resource_path);
		return false;
	}
	else if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Error: %s\n", IMG_GetError());
		fflush(stderr);
		Mix_Quit();
		SDL_DestroyWindow(SDL_AtomicGetPtr((void**)&window));
		SDL_Quit();
		mem_free(save_path);
		mem_free(resource_path);
		return false;
	}

	inited = true;
	return true;
}

void app_deinit() {
	assert(inited);

	IMG_Quit();
	Mix_Quit();
	SDL_DestroyWindow(SDL_AtomicGetPtr((void**)&window));
	SDL_Quit();
	mem_free(save_path);
	mem_free(resource_path);

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&window, NULL);
	save_path = NULL;
	resource_path = NULL;

	inited = false;
}

bool app_inited() {
	return inited;
}

SDL_Window* app_window_get() {
	assert(inited);

	SDL_Window* const current_window = SDL_AtomicGetPtr((void**)&window);
	SDL_MemoryBarrierAcquire();

	return current_window;
}

SDL_GLContext app_glcontext_create() {
	SDL_Window* const current_window = SDL_AtomicGetPtr((void**)&window);
	SDL_MemoryBarrierAcquire();

	#ifndef NDEBUG
	assert(current_window != NULL);
	#endif

	int context_major_version, context_minor_version, context_profile_mask;

	SDL_GLContext context;
	if ((context = SDL_GL_CreateContext(current_window)) == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		return NULL;
	}
	else if (
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &context_major_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &context_minor_version) < 0 ||
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &context_profile_mask) < 0
	) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(current_window, NULL);
		return NULL;
	}
	else if (
		context_major_version < 3 ||
		(context_major_version == 3 && context_minor_version < 2) ||
		context_profile_mask != SDL_GL_CONTEXT_PROFILE_CORE
	) {
		fprintf(stderr, "Error: OpenGL version is %d.%d %s, OpenGL 3.3 Core or higher is required\n",
			context_major_version, context_minor_version,
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_CORE ? "Core" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY ? "Compatibility" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_ES ? "ES" :
			"[UNKNOWN PROFILE TYPE]"
		);
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(current_window, NULL);
		return NULL;
	}
	else if (!opengl_init()) {
		fprintf(stderr, "Error: Failed to initialize OpenGL\n");
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(current_window, NULL);
		return NULL;
	}
	else if (SDL_GL_SetSwapInterval(0) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(current_window, NULL);
		return NULL;
	}

	return context;
}

void app_glcontext_destroy(SDL_GLContext const context) {
	SDL_Window* const current_window = SDL_AtomicGetPtr((void**)&window);
	SDL_MemoryBarrierAcquire();

	#ifndef NDEBUG
	assert(context != NULL && current_window != NULL);
	#endif

	SDL_GL_MakeCurrent(current_window, NULL);
	SDL_GL_DeleteContext(context);
}

const char* const app_resource_path_get() {
	assert(inited);

	return (const char*)resource_path;
}

const char* const app_save_path_get() {
	assert(inited);

	return (const char*)save_path;
}
