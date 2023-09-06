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

#include "render/render.h"
#include "render/sprites.h"
#include "main/app.h"
#include "data/data.h"
#include "render/print.h"
#include "opengl/opengl.h"
#include "util/string_util.h"
#include "util/util.h"
#include <math.h>
#include <stdarg.h>
#include <assert.h>

static frames_object* render_frames;
static data_cache_object* data_cache;
static print_data_object* print_data;
static sprites_object* sprites;

bool render_init(frames_object* const frames) {
	if (!print_init()) {
		return false;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);

	data_cache = data_cache_create(app_resource_path_get(), app_save_path_get());
	if (data_cache == NULL) {
		print_deinit();
		return false;
	}

	print_data = print_data_create();
	if (print_data == NULL) {
		print_deinit();
		data_cache_destroy(data_cache);
		return false;
	}

	sprites = NULL;

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, frames);

	return true;
}

void render_deinit() {
	if (sprites != NULL) {
		sprites_destroy(sprites);
	}

	if (print_data != NULL) {
		print_data_destroy(print_data);
	}

	if (data_cache != NULL) {
		data_cache_destroy(data_cache);
	}

	print_deinit();

	print_data = NULL;
	data_cache = NULL;
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, NULL);
}

static bool render_clear_draw_func(void* const state) {
	const float shade = (uint8_t)(uintptr_t)state / 255.0f;
	glClearColor(shade, shade, shade, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

bool render_clear(const uint8_t shade) {
	static const command_funcs funcs = {
		.update = NULL,
		.draw = render_clear_draw_func,
		.destroy = NULL
	};
	return frames_enqueue_command(render_frames, &funcs, (void*)(uintptr_t)shade);
}

typedef struct render_sprites_struct {
	const char* sheet_filename;
	size_t num_added;
	sprite_type* added_sprites;
} render_sprites_struct;

static bool render_sprites_update_func(void* const state) {
	render_sprites_struct* const s = state;
	if (sprites == NULL) {
		sprites = sprites_create(0u);
		if (sprites == NULL) {
			return false;
		}
	}
	else {
		sprites_restart(sprites);
	}
	return sprites_add(sprites, s->num_added, s->added_sprites);
}

static bool render_sprites_draw_func(void* const state) {
	render_sprites_struct* const s = state;
	const data_object* const data = data_cache_get(data_cache, DATA_TYPE_TEXTURE, DATA_PATH_RESOURCE, s->sheet_filename, NULL, false);
	if (data == NULL) {
		return false;
	}
	return sprites_draw(sprites, data->texture);
}

static void render_sprites_destroy_func(void* const state) {
	render_sprites_struct* const s = state;
	mem_free(s->added_sprites);
	mem_free(s);
}

bool render_sprites(const char* const sheet_filename, const size_t num_added, const sprite_type* const added_sprites) {
	assert(
		sheet_filename != NULL &&
		added_sprites != NULL
	);

	if (num_added == 0u) {
		return true;
	}

	render_sprites_struct* const s = mem_malloc(sizeof(render_sprites_struct));
	if (s == NULL) {
		return false;
	}

	s->sheet_filename = sheet_filename;
	s->num_added = num_added;
	s->added_sprites = mem_malloc(sizeof(sprite_type) * num_added);
	if (s->added_sprites == NULL) {
		mem_free(s);
		return false;
	}
	memcpy(s->added_sprites, added_sprites, sizeof(sprite_type) * num_added);

	static const command_funcs funcs = {
		.update = render_sprites_update_func,
		.draw = render_sprites_draw_func,
		.destroy = render_sprites_destroy_func
	};

	if (!frames_enqueue_command(render_frames, &funcs, s)) {
		mem_free(s->added_sprites);
		mem_free(s);
		return false;
	}

	return true;
}

typedef struct render_print_struct {
	const char* font_filename;
	float x;
	float y;
	char* text;
} render_print_struct;

static bool render_print_draw_func(void* const state) {
	render_print_struct* const p = state;
	const data_object* const data = data_cache_get(data_cache, DATA_TYPE_FONT, DATA_PATH_RESOURCE, p->font_filename, NULL, false);
	if (data == NULL) {
		return false;
	}
	if (!print_size_reset(print_data)) {
		return false;
	}
	if (!print_text(print_data, data->font, p->x, p->y, p->text)) {
		return false;
	}

	return print_draw(print_data, 640.0f, 480.0f);
}

static void render_print_destroy_func(void* const state) {
	render_print_struct* const p = state;
	mem_free((char*)p->text);
	mem_free(p);
}

bool render_print(const char* const font_filename, const float x, const float y, const char* const format, ...) {
	render_print_struct* const p = mem_malloc(sizeof(render_print_struct));
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
		mem_free(p);
		return false;
	}

	static const command_funcs funcs = {
		.update = NULL,
		.draw = render_print_draw_func,
		.destroy = render_print_destroy_func
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		mem_free((char*)p->text);
		mem_free(p);
		return false;
	}

	return true;
}
