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

#include "SDL_thread.h"
#include "SDL_rwops.h"
#include "main/app.h"
#include "util/str.h"
#include "util/mem.h"
#include "main/main.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

// TODO: Implement the all_output feature.

static SDL_atomic_t sync_init = { 0 };
static SDL_TLSID output_files = 0;

static void SDLCALL file_destroy(void* data) {
	assert(data != NULL);

	SDL_RWops* const file = data;
	SDL_RWclose(file);
}

static SDL_RWops* file_get() {
	assert(output_files != 0);

	SDL_RWops* const file = SDL_TLSGet(output_files);
	return file;
}

bool log_init(const char* const all_output) {
	assert(SDL_ThreadID() == main_thread_id_get());
	assert(output_files == 0);

	output_files = SDL_TLSCreate();
	if (output_files == 0) {
		return false;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&sync_init, 0);

	return true;
}

bool log_filename_set(const char* const filename) {
	assert(filename != NULL);
	assert(strlen(filename) > 0u);

	SDL_AtomicGet(&sync_init);
	SDL_MemoryBarrierAcquire();

	char* full_filename = alloc_sprintf("%s%s", app_save_path_get(), filename);
	if (full_filename == NULL) {
		return false;
	}

	SDL_RWops* const file = SDL_RWFromFile(full_filename, "wb");
	if (file == NULL) {
		mem_free(full_filename);
		return false;
	}

	mem_free(full_filename);

	if (SDL_TLSSet(output_files, file, file_destroy) < 0) {
		SDL_RWclose(file);
		return false;
	}

	return true;
}

bool log_text(const char* const text) {
	assert(text != NULL);

	if (strlen(text) == 0u) {
		return true;
	}

	SDL_RWops* const file = file_get();
	return file == NULL || SDL_RWwrite(file, text, strlen(text), 1u) == 1u;
}

bool log_printf(const char* const format, ...) {
	assert(format != NULL);

	if (strlen(format) == 0u) {
		return true;
	}

	SDL_RWops* const file = file_get();
	if (file == NULL) {
		return true;
	}

	va_list args;
	va_start(args, format);
	char* const text = alloc_vsprintf(format, args);
	va_end(args);

	if (text == NULL) {
		return false;
	}

	const bool status = SDL_RWwrite(file, text, strlen(text), 1u) == 1u;
	mem_free(text);
	return status;
}
