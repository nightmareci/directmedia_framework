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

#include "util/private/log_private.h"
#include "main/private/app_private.h"
#include "util/private/conqueue.h"
#include "util/str.h"
#include "util/mem.h"
#include "main/main.h"
#include "SDL_thread.h"
#include "SDL_rwops.h"
#include <stdio.h>
#include <assert.h>

static SDL_atomic_t inited_flag = { 0 };
static bool print_to_all_output = true;

static bool print_all_output_to_stdout;
static SDL_RWops* all_output_file = NULL;
static conqueue_object* all_output_queue = NULL;

static SDL_TLSID output_files = 0;

static void SDLCALL file_destroy(void* data) {
	assert(data != NULL);
	if (data == NULL) {
		abort();
	}

	SDL_RWops* const file = data;
	SDL_RWclose(file);
}

static SDL_RWops* file_get() {
	assert(output_files != 0);
	if (output_files == 0) {
		abort();
	}

	SDL_RWops* const file = SDL_TLSGet(output_files);
	return file;
}

static bool inited() {
	const int current_inited = SDL_AtomicGet(&inited_flag);
	SDL_MemoryBarrierAcquire();
	return !!current_inited;
}

bool log_init(const char* const all_output) {
	assert(this_thread_is_main_thread());
	assert(output_files == 0);
	assert(print_to_all_output);

	if (inited()) {
		return false;
	}
	else if (all_output != NULL) {
		if (!strcmp(all_output, "stdout")) {
			print_all_output_to_stdout = true;
		}
		else {
			char* const full_filename = alloc_sprintf("%s%s", app_save_path_get(), all_output);
			assert(full_filename != NULL);
			if (full_filename == NULL) {
				return false;
			}

			all_output_file = SDL_RWFromFile(full_filename, "wb");
			assert(all_output_file != NULL);
			if (all_output_file == NULL) {
				mem_free(full_filename);
				return false;
			}

			mem_free(full_filename);

			print_all_output_to_stdout = false;
		}

		all_output_queue = conqueue_create();
		assert(all_output_queue != NULL);
		if (all_output_queue == NULL) {
			return false;
		}

		print_to_all_output = true;
	}
	else {
		output_files = SDL_TLSCreate();
		assert(output_files != 0);
		if (output_files == 0) {
			return false;
		}

		print_to_all_output = false;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&inited_flag, 1);

	return true;
}

bool log_all_output_deinit() {
	if (!inited()) {
		return true;
	}
	else if (!print_to_all_output) {
		return true;
	}
	else if (!log_all_output_dequeue(LOG_ALL_OUTPUT_DEQUEUE_EMPTY)) {
		return false;
	}

	conqueue_destroy(all_output_queue);

	if (print_all_output_to_stdout) {
		fflush(stdout);
	}
	else {
		SDL_RWclose(all_output_file);
		all_output_file = NULL;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&inited_flag, 0);

	return true;
}

bool log_all_output_dequeue(const uint64_t allotted_time) {
	const uint64_t start_time = nanotime_now();
	const uint64_t now_max = nanotime_now_max();

	if (!inited()) {
		return true;
	}

	assert(print_to_all_output);
	if (!print_to_all_output) {
		return true;
	}

	if (print_all_output_to_stdout) {
		const bool is_main_thread = this_thread_is_main_thread();
		assert(is_main_thread);
		if (!is_main_thread) {
			return false;
		}

		for (
			char* text = conqueue_dequeue(all_output_queue);
			(allotted_time == LOG_ALL_OUTPUT_DEQUEUE_EMPTY || nanotime_interval(start_time, nanotime_now(), now_max) < allotted_time) && text != NULL;
			text = conqueue_dequeue(all_output_queue)
		) {
			const bool put = fputs(text, stdout) != EOF;
			mem_free(text);
			assert(put);
			if (!put) {
				return false;
			}
		}
		const bool flushed = fflush(stdout) != EOF;
		assert(flushed);
		if (!flushed) {
			return false;
		}
	}
	else {
		for (
			char* text = conqueue_dequeue(all_output_queue);
			(allotted_time == LOG_ALL_OUTPUT_DEQUEUE_EMPTY || nanotime_interval(start_time, nanotime_now(), now_max) < allotted_time) && text != NULL;
			text = conqueue_dequeue(all_output_queue)
		) {
			const bool wrote = SDL_RWwrite(all_output_file, text, strlen(text), 1u) == 1u;
			mem_free(text);
			assert(wrote);
			if (!wrote) {
				return false;
			}
		}
	}

	return true;
}

bool log_filename_set(const char* const filename) {
	assert(filename != NULL);
	assert(strlen(filename) > 0u);
	if (!inited()) {
		return false;
	}

	char* const full_filename = alloc_sprintf("%s%s", app_save_path_get(), filename);
	assert(full_filename != NULL);
	if (full_filename == NULL) {
		return false;
	}

	SDL_RWops* const file = SDL_RWFromFile(full_filename, "wb");
	assert(file != NULL);
	if (file == NULL) {
		mem_free(full_filename);
		return false;
	}

	mem_free(full_filename);

	const bool tls_set = SDL_TLSSet(output_files, file, file_destroy) >= 0;
	assert(tls_set);
	if (!tls_set) {
		SDL_RWclose(file);
		return false;
	}

	return true;
}

static void uninited_put(const char* const text) {
	const bool is_main_thread = this_thread_is_main_thread();
	assert(is_main_thread);
	if (!is_main_thread) {
		abort();
	}
	const bool put = fputs(text, stdout) != EOF;
	assert(put);
	if (!put) {
		abort();
	}
	const bool flushed = fflush(stdout) != EOF;
	assert(flushed);
	if (!flushed) {
		abort();
	}
}

void log_text(const char* const text) {
	assert(text != NULL);

	if (strlen(text) == 0u) {
		return;
	}

	if (!inited()) {
		uninited_put(text);
		return;
	}
	else if (print_to_all_output) {
		const char* const thread_name = app_this_thread_name_get();
		char* enqueued_text;
		if (thread_name == NULL) {
			enqueued_text = alloc_sprintf("%s", text);
		}
		else {
			enqueued_text = alloc_sprintf("[%s thread] %s", thread_name, text);
		}
		assert(enqueued_text != NULL);
		if (enqueued_text == NULL) {
			abort();
		}
		const bool enqueued = conqueue_enqueue(all_output_queue, enqueued_text);
		assert(enqueued);
		if (!enqueued) {
			mem_free(enqueued_text);
			abort();
		}
		else {
			return;
		}
	}
	else {
		SDL_RWops* const file = file_get();
		if (file != NULL) {
			const bool wrote = SDL_RWwrite(file, text, strlen(text), 1u) != 1u;
			assert(wrote);
			if (!wrote) {
				abort();
			}
		}
		else {
			return;
		}
	}
}

static char* formatted_text_alloc(const char* const format, va_list args) {
	char* text;
	if (inited() && print_to_all_output) {
		char* const base_text = alloc_vsprintf(format, args);
		assert(base_text != NULL);
		if (base_text == NULL) {
			abort();
		}
		const char* const thread_name = app_this_thread_name_get();
		if (thread_name == NULL) {
			text = base_text;
		}
		else {
			text = alloc_sprintf("[%s thread] %s", thread_name, base_text);
			mem_free(base_text);
		}
	}
	else {
		text = alloc_vsprintf(format, args);
	}

	return text;
}

void log_printf(const char* const format, ...) {
	assert(format != NULL);

	if (strlen(format) == 0u) {
		return;
	}

	va_list args;
	va_start(args, format);
	char* text = formatted_text_alloc(format, args);
	va_end(args);

	assert(text != NULL);
	if (text == NULL) {
		abort();
	}

	if (!inited()) {
		uninited_put(text);
		mem_free(text);
		return;
	}
	else if (print_to_all_output) {
		const bool enqueued = conqueue_enqueue(all_output_queue, text);
		assert(enqueued);
		if (!enqueued) {
			mem_free(text);
			abort();
		}
		else {
			return;
		}
	}
	else {
		SDL_RWops* const file = file_get();
		if (file == NULL) {
			return;
		}

		const bool wrote = SDL_RWwrite(file, text, strlen(text), 1u) == 1u;
		assert(wrote);
		mem_free(text);
		if (!wrote) {
			abort();
		}
		else {
			return;
		}
	}
}

void log_vprintf(const char* const format, va_list args) {
	assert(format != NULL);

	if (strlen(format) == 0u) {
		return;
	}

	char* const text = formatted_text_alloc(format, args);

	assert(text != NULL);
	if (text == NULL) {
		abort();
	}

	if (!inited()) {
		uninited_put(text);
		mem_free(text);
		return;
	}
	else if (print_to_all_output) {
		const bool enqueued = conqueue_enqueue(all_output_queue, text);
		assert(enqueued);
		if (!enqueued) {
			mem_free(text);
			abort();
		}
		else {
			return;
		}
	}
	else {
		SDL_RWops* const file = file_get();
		if (file == NULL) {
			return;
		}

		const bool wrote = SDL_RWwrite(file, text, strlen(text), 1u) == 1u;
		assert(wrote);
		mem_free(text);
		if (!wrote) {
			abort();
		}
		else {
			return;
		}
	}
}
