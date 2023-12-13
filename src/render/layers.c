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

// TODO: Refactor so this library manages its own rendering, not depending at
// all on the sprites library? Then it can use different techniques, like
// indexed rendering of an out-of-order sprites array.

#include "render/layers.h"
#include "render/sprites.h"
#include "util/mem.h"
#include "SDL_video.h"
#include <assert.h>

#define ALIGN_BOTH_BUMP(type_a, type_b) ( \
	_Alignof(type_a) > _Alignof(type_b) ? \
		_Alignof(type_a) - _Alignof(type_b) : \
		_Alignof(type_b) - _Alignof(type_a) \
)

#define SIZEOF_TAILED_ARRAY_STRUCT(struct_type, array_type, array_elements) \
	(sizeof(struct_type) + (array_elements) * sizeof(array_type) + ALIGN_BOTH_BUMP(struct_type, array_type))

typedef struct sprites_sequence {
	size_t length;
	size_t size;
	data_texture_object* sheet;
	sprite_type sprites[];
} sprites_sequence;

typedef struct sprites_layer {
	size_t length;
	size_t size;
	sprites_sequence* sequences[];
} sprites_layer;

struct layers_object {
	sprites_object* sprites;
	size_t size;
	sprites_layer** layers;
};

layers_object* layers_create(const size_t num_layers) {
	layers_object* const layers = mem_calloc(1u, sizeof(layers_object));
	if (layers == NULL) {
		return NULL;
	}

	layers->sprites = sprites_create(0u);
	if (layers->sprites == NULL) {
		mem_free(layers);
		return NULL;
	}

	if (num_layers == 0u) {
		return layers;
	}

	layers->layers = mem_calloc(num_layers, sizeof(sprites_layer*));
	if (layers->layers == NULL) {
		sprites_destroy(layers->sprites);
		mem_free(layers);
		return NULL;
	}

	layers->size = num_layers;

	return layers;
}

void layers_destroy(layers_object* const layers) {
	if (layers->layers != NULL) {
		for (size_t i = 0u; i < layers->size; i++) {
			sprites_layer* const layer = layers->layers[i];
			if (layer == NULL) {
				continue;
			}
			for (size_t j = 0u; j < layer->size; j++) {
				sprites_sequence* const sequence = layer->sequences[j];
				if (sequence == NULL) {
					continue;
				}
				mem_free(sequence);
			}
			mem_free(layer);
		}
		mem_free(layers->layers);
	}
	sprites_destroy(layers->sprites);
	mem_free(layers);
}

void layers_restart(layers_object* const layers) {
	for (size_t i = 0u; i < layers->size; i++) {
		sprites_layer* const layer = layers->layers[i];
		if (layer == NULL) {
			continue;
		}
		for (size_t j = 0u; j < layer->size; j++) {
			sprites_sequence* const sequence = layer->sequences[j];
			if (sequence != NULL) {
				sequence->length = 0u;
			}
		}
		layer->length = 0u;
	}
}

void layers_screen_reset(layers_object* const layers) {
	sprites_screen_reset(layers->sprites);
}

void layers_screen_set(layers_object* const layers, const float width, const float height) {
	sprites_screen_set(layers->sprites, width, height);
}

bool layers_sprites_add(layers_object* const layers, data_texture_object* const sheet, const size_t layer_index, const size_t num_added, sprite_type* const added_sprites) {
	assert(layers != NULL);
	assert(layer_index < layers->size);
	assert(sheet != NULL);
	assert(added_sprites != NULL);

	if (num_added == 0u) {
		return true;
	}

	sprites_layer* layer = layers->layers[layer_index];
	if (layer == NULL) {
		sprites_layer* const new_layer = mem_calloc(1u, SIZEOF_TAILED_ARRAY_STRUCT(sprites_layer, sprites_sequence*, 1u));
		if (new_layer == NULL) {
			return false;
		}
		new_layer->size = 1u;
		layers->layers[layer_index] = new_layer;
		layer = new_layer;
	}
	else if (layer->length + 1u > layer->size) {
		const size_t new_layer_length = layer->length + 1u;
		sprites_layer* const new_layer = mem_realloc(layer, SIZEOF_TAILED_ARRAY_STRUCT(sprites_layer, sprites_sequence*, new_layer_length * 2u));
		if (new_layer == NULL) {
			return false;
		}
		memset(new_layer->sequences + new_layer->size, 0, (new_layer_length * 2u - new_layer->size) * sizeof(sprites_sequence*));
		new_layer->size = new_layer_length * 2u;
		layers->layers[layer_index] = new_layer;
		layer = new_layer;
	}

	sprites_sequence* sequence = layer->sequences[layer->length];
	if (sequence == NULL) {
		sprites_sequence* const new_sequence = mem_malloc(SIZEOF_TAILED_ARRAY_STRUCT(sprites_sequence, sprite_type, num_added * 2u));
		if (new_sequence == NULL) {
			return false;
		}
		new_sequence->length = 0u;
		new_sequence->size = num_added * 2u;
		layer->sequences[layer->length] = new_sequence;
		sequence = new_sequence;
	}
	else if (num_added > sequence->size) {
		sprites_sequence* const new_sequence = mem_realloc(sequence, SIZEOF_TAILED_ARRAY_STRUCT(sprites_sequence, sprite_type, num_added + sequence->size));
		if (new_sequence == NULL) {
			return false;
		}
		new_sequence->size = num_added + sequence->size;
		layer->sequences[layer->length] = new_sequence;
		sequence = new_sequence;
	}

	sequence->sheet = sheet;
	sequence->length = num_added;

	memcpy(sequence->sprites, added_sprites, num_added * sizeof(sprite_type));
	layer->length++;

	return true;
}

bool layers_draw(layers_object* const layers) {
	assert(layers != NULL);

	sprites_restart(layers->sprites);

	for (size_t i = 0u; i < layers->size; i++) {
		sprites_layer* const layer = layers->layers[i];
		if (layer == NULL) {
			continue;
		}

		for (size_t j = 0u; j < layer->length; j++) {
			sprites_sequence* const sequence = layer->sequences[j];
			if (sequence == NULL) {
				continue;
			}

			if (!sprites_add(layers->sprites, sequence->sheet, sequence->length, sequence->sprites)) {
				return false;
			}
		}
	}

	return sprites_draw(layers->sprites);
}
