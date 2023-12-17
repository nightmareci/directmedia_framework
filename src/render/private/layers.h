#pragma once
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

/*
 * Simple layers-of-sprites graphics API.
 */

#include "render/render_types.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct layers_object layers_object;

layers_object* layers_create(const size_t num_layers);
void layers_destroy(layers_object* const layers);

#if 0
bool layers_shrink(layers_object* const layers); // TODO
bool layers_resize_num_layers(layers_object* const layers, const size_t num_layers); // TODO
bool layers_shrink_num_layers(layers_object* const layers); // TODO
bool layers_resize_layer(layers_object* const layers, const size_t layer_index, const size_t num_sprites); // TODO
bool layers_shrink_layer(layers_object* const layers, const size_t layer_index); // TODO
#endif

void layers_restart(layers_object* const layers);

void layers_screen_reset(layers_object* const layers);
void layers_screen_set(layers_object* const layers, const float width, const float height);

/*
 * Add sprites to a layer. Sprites added to a layer are drawn in submission
 * order. And layers are drawn from least-layer-index-to-greatest,
 * back-to-front.
 */
bool layers_sprites_add(layers_object* const layers, data_texture_object* const sheet, const size_t layer_index, const size_t num_added, sprite_type* const added_sprites);

bool layers_draw(layers_object* const layers);
