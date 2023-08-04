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
#include "util/string_util.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <assert.h>

static bool inited = false;
static SDL_Window* window = NULL;
static char* asset_path = NULL;
static char* save_path = NULL;

bool app_init(const int argc, char** const argv) {
	assert(!inited);

	bool portable_app = false;
	const char* asset_path_override = NULL;
	const char* save_path_override = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--portable-app") == 0) {
			portable_app = true;
		}
		else if (strcmp(argv[i], "--asset-path") == 0 && argc > i + 1) {
			asset_path_override = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "--save-path") == 0 && argc > i + 1) {
			save_path_override = argv[i + 1];
			i++;
		}
	}

	if (!portable_app && asset_path_override != NULL) {
		const size_t len = strlen(asset_path_override);
		const char end = len > 0u ? asset_path_override[len - 1] : '\0';
		if ((asset_path = alloc_sprintf("%s%s", asset_path_override,
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
		if ((asset_path = alloc_sprintf("%sasset%s", temp_string,
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
			free(asset_path);
			return false;
		}
	}
	else {
		char* temp_string;

		if (portable_app) {
			if ((temp_string = SDL_GetBasePath()) == NULL) {
				free(asset_path);
				return false;
			}
		}
		else if ((temp_string = SDL_GetPrefPath(app_organization, app_executable)) == NULL) {
			free(asset_path);
			return false;
		}
		if ((save_path = alloc_sprintf("%s", temp_string)) == NULL) {
			SDL_free(temp_string);
			free(asset_path);
			return false;
		}
		SDL_free(temp_string);
	}

	printf(
		"App file cache asset path: %s\n"
		"App file cache save path: %s\n",

		asset_path,
		save_path
	);
	fflush(stdout);


	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		free(save_path);
		free(asset_path);
		return false;
	}
	else if (
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) < 0 ||
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) < 0
	) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_Quit();
		free(save_path);
		free(asset_path);
		return false;
	}
	else if ((window = SDL_CreateWindow(app_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL)) == NULL) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_Quit();
		free(save_path);
		free(asset_path);
		return false;
	}
	else if (Mix_Init(MIX_INIT_OGG | MIX_INIT_MOD) != (MIX_INIT_OGG | MIX_INIT_MOD)) {
		fprintf(stderr, "Error: %s\n", Mix_GetError());
		fflush(stderr);
		SDL_DestroyWindow(window);
		SDL_Quit();
		free(save_path);
		free(asset_path);
		return false;
	}
	else if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		fprintf(stderr, "Error: %s\n", IMG_GetError());
		fflush(stderr);
		Mix_Quit();
		SDL_DestroyWindow(window);
		SDL_Quit();
		free(save_path);
		free(asset_path);
		return false;
	}

	inited = true;
	return true;
}

void app_deinit() {
	assert(inited);

	IMG_Quit();
	Mix_Quit();
	SDL_DestroyWindow(window);
	SDL_Quit();
	free(save_path);
	free(asset_path);

	window = NULL;
	save_path = NULL;
	asset_path = NULL;

	inited = false;
}

bool app_inited() {
	return inited;
}

SDL_Window* app_window_get() {
	assert(inited);

	return window;
}

SDL_GLContext app_glcontext_create() {
	assert(window != NULL);

	int context_major_version, context_minor_version, context_profile_mask;

	SDL_GLContext context;
	if ((context = SDL_GL_CreateContext(window)) == NULL) {
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
		SDL_GL_MakeCurrent(window, NULL);
		return NULL;
	}
	else if (
		context_major_version < 3 ||
		(context_major_version == 3 && context_minor_version < 2) ||
		context_profile_mask != SDL_GL_CONTEXT_PROFILE_CORE
	) {
		fprintf(stderr, "Error: OpenGL version is %d.%d %s, OpenGL 3.2 Core or higher is required\n",
			context_major_version, context_minor_version,
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_CORE ? "Core" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY ? "Compatibility" :
			context_profile_mask == SDL_GL_CONTEXT_PROFILE_ES ? "ES" :
			"[UNKNOWN PROFILE TYPE]"
		);
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(window, NULL);
		return NULL;
	}
	else if (!opengl_init()) {
		fprintf(stderr, "Error: Failed to initialize OpenGL\n");
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(window, NULL);
		return NULL;
	}
	else if (SDL_GL_SetSwapInterval(0) < 0) {
		fprintf(stderr, "Error: %s\n", SDL_GetError());
		fflush(stderr);
		SDL_GL_DeleteContext(context);
		SDL_GL_MakeCurrent(window, NULL);
		return NULL;
	}

	return context;
}

void app_glcontext_destroy(SDL_GLContext const context) {
	assert(context != NULL && window != NULL);

	SDL_GL_MakeCurrent(window, NULL);
	SDL_GL_DeleteContext(context);
}

const char* const app_asset_path_get() {
	assert(inited);

	return (const char*)asset_path;
}

const char* const app_save_path_get() {
	assert(inited);

	return (const char*)save_path;
}
