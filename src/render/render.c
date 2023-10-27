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
#include "render/layers.h"
#include "render/print.h"
#include "main/app.h"
#include "data/data.h"
#include "opengl/opengl.h"
#include "util/text.h"
#include "util/mem.h"
#include <math.h>
#include <stdarg.h>
#include <assert.h>

static frames_object* render_frames;
static data_cache_object* data_cache;
#define NUM_LAYERS 2u
static layers_object* layers;
static sprites_object* sprites;

bool render_init(frames_object* const frames) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);

	data_cache = data_cache_create(app_resource_path_get(), app_save_path_get());
	if (data_cache == NULL) {
		return false;
	}

	layers = NULL;
	sprites = NULL;

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, frames);

	return true;
}

void render_deinit() {
	if (sprites != NULL) {
		sprites_destroy(sprites);
	}

	if (layers != NULL) {
		layers_destroy(layers);
	}

	if (data_cache != NULL) {
		data_cache_destroy(data_cache);
	}

	data_cache = NULL;
	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&render_frames, NULL);
}

static bool render_start_update_func(void* const state) {
	if (sprites == NULL) {
		sprites = sprites_create(0u);
		if (sprites == NULL) {
			return false;
		}
	}
	else {
		sprites_restart(sprites);
	}

	if (layers == NULL) {
		layers = layers_create(NUM_LAYERS);
		if (layers == NULL) {
			return false;
		}
	}
	else {
		layers_restart(layers);
	}

	return true;
}

bool render_start() {
	static const command_funcs funcs = {
		.update = render_start_update_func,
		.draw = NULL,
		.destroy = NULL
	};

	return frames_enqueue_command(render_frames, &funcs, NULL);
}

static bool render_end_draw_func(void* const state) {
	if (!sprites_draw(sprites)) return false;
	if (!layers_draw(layers)) return false;

	return true;
}

bool render_end() {
	static const command_funcs funcs = {
		.update = NULL,
		.draw = render_end_draw_func,
		.destroy = NULL
	};

	return frames_enqueue_command(render_frames, &funcs, NULL);
}

static bool render_clear_draw_func(void* const state) {
	const uintptr_t color = (uintptr_t)state;
	glClearColor(
		(uint8_t)(color >> 24) / 255.0f,
		(uint8_t)(color >> 16) / 255.0f,
		(uint8_t)(color >>  8) / 255.0f,
		(uint8_t)(color >>  0) / 255.0f
	);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

bool render_clear(const uint8_t red, const uint8_t green, const uint8_t blue, const uint8_t alpha) {
	static const command_funcs funcs = {
		.update = NULL,
		.draw = render_clear_draw_func,
		.destroy = NULL
	};

	static_assert(sizeof(void*) >= 4u, "sizeof(void*) < 4u");
	return frames_enqueue_command(render_frames, &funcs, (void*)(
		((uintptr_t)red   << 24) |
		((uintptr_t)green << 16) |
		((uintptr_t)blue  <<  8) |
		((uintptr_t)alpha <<  0)
	));
}

typedef struct render_sprites_object {
	const char* sheet_filename;
	size_t layer_index;
	size_t num_added;
	sprite_type* added_sprites;
} render_sprites_object;

static bool render_sprites_update_func(void* const state) {
	render_sprites_object* const s = state;

	const data_object* const data = data_cache_get(data_cache, DATA_TYPE_TEXTURE, DATA_PATH_RESOURCE, s->sheet_filename, NULL, false);
	if (data == NULL) {
		return false;
	}
	assert(s->layer_index < NUM_LAYERS);
	return layers_sprites_add(layers, data->texture, s->layer_index, s->num_added, s->added_sprites);
}

static void render_sprites_destroy_func(void* const state) {
	render_sprites_object* const s = state;
	mem_free(s->added_sprites);
	mem_free(s);
}

bool render_sprites(const char* const sheet_filename, const size_t layer_index, const size_t num_added, const sprite_type* const added_sprites) {
	assert(
		sheet_filename != NULL &&
		added_sprites != NULL
	);

	if (num_added == 0u) {
		return true;
	}

	render_sprites_object* const s = mem_malloc(sizeof(render_sprites_object));
	if (s == NULL) {
		return false;
	}

	s->sheet_filename = sheet_filename;
	s->layer_index = layer_index;
	s->num_added = num_added;
	s->added_sprites = mem_malloc(sizeof(sprite_type) * num_added);
	if (s->added_sprites == NULL) {
		mem_free(s);
		return false;
	}
	memcpy(s->added_sprites, added_sprites, sizeof(sprite_type) * num_added);

	static const command_funcs funcs = {
		.update = render_sprites_update_func,
		.draw = NULL,
		.destroy = render_sprites_destroy_func
	};

	if (!frames_enqueue_command(render_frames, &funcs, s)) {
		mem_free(s->added_sprites);
		mem_free(s);
		return false;
	}

	return true;
}

typedef struct render_print_object {
	const char* font_filename;
	size_t layer_index;
	float x;
	float y;
	char* text;
} render_print_object;

static bool render_print_update_func(void* const state) {
	render_print_object* const p = state;
	const data_object* const font = data_cache_get(data_cache, DATA_TYPE_FONT, DATA_PATH_RESOURCE, p->font_filename, NULL, false);
	if (font == NULL) {
		return false;
	}
	return print_layer_text(font->font, layers, p->layer_index, p->x, p->y, p->text);
}

static void render_print_destroy_func(void* const state) {
	render_print_object* const p = state;
	mem_free((char*)p->text);
	mem_free(p);
}

bool render_text(const char* const font_filename, const size_t layer_index, const float x, const float y, const char* const text) {
	render_print_object* const p = mem_malloc(sizeof(render_print_object));
	if (p == NULL) {
		return false;
	}

	p->font_filename = font_filename;
	p->layer_index = layer_index;
	p->x = x;
	p->y = y;
	const size_t size = strlen(text) + 1u;
	p->text = mem_malloc(size);
	if (p->text == NULL) {
		mem_free(p);
		return false;
	}
	memcpy(p->text, text, size);

	static const command_funcs funcs = {
		.update = render_print_update_func,
		.draw = NULL,
		.destroy = render_print_destroy_func
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		mem_free((char*)p->text);
		mem_free(p);
		return false;
	}

	return true;
}

bool render_printf(const char* const font_filename, const size_t layer_index, const float x, const float y, const char* const format, ...) {
	render_print_object* const p = mem_malloc(sizeof(render_print_object));
	if (p == NULL) {
		return false;
	}

	p->font_filename = font_filename;
	p->layer_index = layer_index;
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
		.update = render_print_update_func,
		.draw = NULL,
		.destroy = render_print_destroy_func
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		mem_free((char*)p->text);
		mem_free(p);
		return false;
	}

	return true;
}
