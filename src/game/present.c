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

#include "game/present.h"
#include "game/game.h"
#include "framework/app.h"
#include "framework/file.h"
#include "framework/print.h"
#include "framework/glad.h"
#include "framework/string_util.h"
#include "framework/defs.h"
#include <math.h>
#include <stdarg.h>
#include <assert.h>

static file_cache_struct* present_cache;
static print_data_struct* print_data;

static bool clear_operation(void* const input) {
	const uintptr_t ticks = (uintptr_t)input;
	const float shade = sinf(M_PIf * fmodf(ticks / (float)TICK_RATE, 1.0f)) * 0.25f + 0.25f;
	glClearColor(shade, shade, shade, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

typedef struct print_struct {
	const char* font;
	float x;
	float y;
	char* text;
} print_struct;

static bool print_operation(void* const input) {
	print_struct* const p = input;
	const file_struct* const file = file_cache_get(present_cache, FILE_TYPE_FONT, FILE_PATH_ASSET, p->font, NULL, false);
	if (file == NULL) {
		return false;
	}
	if (!print_size_reset(print_data)) {
		return false;
	}
	if (!print_text(print_data, file->font, p->x, p->y, p->text)) {
		return false;
	}

	return print_draw(print_data, 640.0f, 480.0f);
}

static void print_destroy(void* const input) {
	print_struct* const p = input;
	free((char*)p->text);
	free(p);
}

bool present_clear(commands_struct* const commands, uint64_t ticks) {
	return commands_enqueue(commands, (void*)(uintptr_t)ticks, clear_operation, NULL);
}

bool present_print(commands_struct* const commands, const float x, const float y, const char* const font, const char* const format, ...) {
	print_struct* const p = malloc(sizeof(print_struct));
	if (p == NULL) {
		return false;
	}

	p->font = font;
	p->x = x;
	p->y = y;
	va_list args;
	va_start(args, format);
	p->text = alloc_vsprintf(format, args);
	va_end(args);
	if (p->text == NULL) {
		free(p);
		return false;
	}

	if (!commands_enqueue(commands, p, print_operation, print_destroy)) {
		free((char*)p->text);
		free(p);
		return false;
	}

	return true;
}

bool present_init() {
	if (!print_init()) {
		return false;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);

	present_cache = file_cache_create(app_asset_path_get(), app_save_path_get());
	if (present_cache == NULL) {
		return false;
	}

	print_data = print_data_create();
	if (print_data == NULL) {
		return false;
	}

	return true;
}

void present_deinit() {
	if (print_data != NULL) {
		print_data_destroy(print_data);
	}

	if (present_cache != NULL) {
		file_cache_destroy(present_cache);
	}

	print_deinit();

	print_data = NULL;
	present_cache = NULL;
}
