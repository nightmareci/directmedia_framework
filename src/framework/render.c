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

#include "framework/render.h"
#include "framework/app.h"
#include "framework/file.h"
#include "framework/print.h"
#include "framework/glad.h"
#include "framework/string_util.h"
#include "framework/defs.h"
#include <math.h>
#include <stdarg.h>
#include <assert.h>

static frames_struct* render_frames;
static file_cache_struct* file_cache;
static print_data_struct* print_data;

bool render_init(frames_struct* const frames) {
	render_frames = frames;

	if (!print_init()) {
		return false;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);

	file_cache = file_cache_create(app_asset_path_get(), app_save_path_get());
	if (file_cache == NULL) {
		print_deinit();
		return false;
	}

	print_data = print_data_create();
	if (print_data == NULL) {
		print_deinit();
		file_cache_destroy(file_cache);
		return false;
	}

	return true;
}

void render_deinit() {
	if (print_data != NULL) {
		print_data_destroy(print_data);
	}

	if (file_cache != NULL) {
		file_cache_destroy(file_cache);
	}

	print_deinit();

	print_data = NULL;
	file_cache = NULL;
	render_frames = NULL;
}

static bool clear_draw_func(void* const state) {
	const float shade = (uint8_t)(uintptr_t)state / 255.0f;
	glClearColor(shade, shade, shade, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

bool render_clear(const uint8_t shade) {
	static const command_funcs_struct funcs = {
		.update = NULL,
		.draw = clear_draw_func,
		.destroy = NULL
	};
	return frames_enqueue_command(render_frames, &funcs, (void*)(uintptr_t)shade);
}

bool render_sprites(const char* const image_filename, const render_sprite_struct* const sprites) {
	// TODO
	return true;
}

typedef struct print_struct {
	const char* font_filename;
	float x;
	float y;
	char* text;
} print_struct;

static bool print_draw_func(void* const state) {
	print_struct* const p = state;
	const file_struct* const file = file_cache_get(file_cache, FILE_TYPE_FONT, FILE_PATH_ASSET, p->font_filename, NULL, false);
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

static void print_destroy(void* const state) {
	print_struct* const p = state;
	free((char*)p->text);
	free(p);
}

bool render_print(const char* const font_filename, const float x, const float y, const char* const format, ...) {
	print_struct* const p = malloc(sizeof(print_struct));
	if (p == NULL) {
		return false;
	}

	p->font_filename = font_filename;
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
	
	static const command_funcs_struct funcs = {
		.update = NULL,
		.draw = print_draw_func,
		.destroy = print_destroy
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		free((char*)p->text);
		free(p);
		return false;
	}

	return true;
}
