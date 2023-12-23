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

#include "render/private/render_private.h"
#include "render/private/sprites.h"
#include "render/private/layers.h"
#include "render/private/print.h"
#include "render/private/opengl.h"
#include "main/app.h"
#include "data/data.h"
#include "util/log.h"
#include "util/str.h"
#include "util/mem.h"
#include "util/maths.h"
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
	vecptr const screen_size = state;
	if (!(
		screen_size[0] >= 0.0f && screen_size[0] <= FLT_MAX &&
		screen_size[1] >= 0.0f && screen_size[1] <= FLT_MAX
	)) {
		mem_free(screen_size);
		return false;
	}

	if (sprites == NULL) {
		sprites = sprites_create(0u);
		if (sprites == NULL) {
			mem_free(screen_size);
			return false;
		}
	}
	else {
		sprites_restart(sprites);
	}

	size_t render_size[2];
	app_render_size_get(&render_size[0], &render_size[1]);

	if (layers == NULL) {
		layers = layers_create(NUM_LAYERS);
		if (layers == NULL) {
			mem_free(screen_size);
			return false;
		}
	}
	else {
		layers_restart(layers);
	}

	const float render_aspect = (float)render_size[0] / (float)render_size[1];
	const float screen_aspect = screen_size[0] / screen_size[1];
	vec2 set_size = { screen_size[0], screen_size[1] };
	if (render_aspect > 1.0f) {
		if (render_aspect > screen_aspect) {
			set_size[0] *= render_aspect / screen_aspect;
		}
		else {
			set_size[1] *= screen_aspect / render_aspect;
		}
	}
	else if (render_aspect < 1.0f) {
		if (render_aspect > screen_aspect) {
			set_size[0] *= render_aspect / screen_aspect;
		}
		else {
			set_size[1] *= screen_aspect / render_aspect;
		}
	}

	sprites_screen_set(sprites, set_size[0], set_size[1]);
	layers_screen_set(layers, set_size[0], set_size[1]);

	mem_free(screen_size);
	return true;
}

bool render_start(const float width, const float height) {
	static const command_funcs funcs = {
		.update = render_start_update_func,
		.draw = NULL,
		.destroy = NULL
	};

	vecptr const screen_size = mem_malloc(sizeof(vec2));
	if (screen_size == NULL) {
		return false;
	}
	screen_size[0] = width;
	screen_size[1] = height;
	return frames_enqueue_command(render_frames, &funcs, screen_size);
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
	const vecptr color = (vecptr)state;
	glClearColor(color[0], color[1], color[2], color[3]);
	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

static void render_clear_destroy_func(void* const state) {
	const vecptr color = state;
	mem_free(color);
}

bool render_clear(const float red, const float green, const float blue, const float alpha) {
	static const command_funcs funcs = {
		.update = NULL,
		.draw = render_clear_draw_func,
		.destroy = render_clear_destroy_func
	};

	vecptr color = mem_malloc(sizeof(vec4));
	if (color == NULL) {
		return false;
	}
	color[0] = red;
	color[1] = green;
	color[2] = blue;
	color[3] = alpha;

	return frames_enqueue_command(render_frames, &funcs, color);
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
	const bool success = layers_sprites_add(layers, data->texture, s->layer_index, s->num_added, s->added_sprites);
	mem_free(s->added_sprites);
	mem_free(s);
	return success;
}

bool render_sprites(const char* const sheet_filename, const size_t layer_index, const size_t num_added, const sprite_type* const added_sprites) {
	assert(sheet_filename != NULL);
	assert(added_sprites != NULL);

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
		.destroy = NULL
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
	char* string;
} render_print_object;

static bool render_print_update_func(void* const state) {
	render_print_object* const p = state;
	const data_object* const font = data_cache_get(data_cache, DATA_TYPE_FONT, DATA_PATH_RESOURCE, p->font_filename, NULL, false);
	if (font == NULL) {
		return false;
	}
	const bool success = print_layer_string(font->font, layers, p->layer_index, p->x, p->y, p->string);
	mem_free((char*)p->string);
	mem_free(p);
	return success;
}

bool render_string(const char* const font_filename, const size_t layer_index, const float x, const float y, const char* const string) {
	render_print_object* const p = mem_malloc(sizeof(render_print_object));
	if (p == NULL) {
		return false;
	}

	p->font_filename = font_filename;
	p->layer_index = layer_index;
	p->x = x;
	p->y = y;
	const size_t size = strlen(string) + 1u;
	p->string = mem_malloc(size);
	if (p->string == NULL) {
		mem_free(p);
		return false;
	}
	memcpy(p->string, string, size);

	static const command_funcs funcs = {
		.update = render_print_update_func,
		.draw = NULL,
		.destroy = NULL
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		mem_free((char*)p->string);
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
	p->string = alloc_vsprintf(format, args);
	va_end(args);
	if (p->string == NULL) {
		mem_free(p);
		return false;
	}

	static const command_funcs funcs = {
		.update = render_print_update_func,
		.draw = NULL,
		.destroy = NULL
	};
	if (!frames_enqueue_command(render_frames, &funcs, p)) {
		mem_free((char*)p->string);
		mem_free(p);
		return false;
	}

	return true;
}
