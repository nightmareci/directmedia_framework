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
 * Simple sprite graphics API. Only sprites_draw actually submits draw calls,
 * and always on top of what was previously drawn, as it disables and makes no
 * use of the depth buffer; thus, this graphics API is suitable for drawing a 2D
 * HUD on top of a previously-drawn 3D scene. It's best to use a single
 * sprites_object for all sprite graphics if possible, as the internals of the
 * API can better utilize system resources, especially memory, with such usage.
 */

#include "render/render_types.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct sprites_object sprites_object;

sprites_object* sprites_create(const size_t initial_size);
void sprites_destroy(sprites_object* const sprites);
bool sprites_resize(sprites_object* const sprites, const size_t new_size);
bool sprites_shrink(sprites_object* const sprites);
void sprites_restart(sprites_object* const sprites);

void sprites_screen_reset(sprites_object* const sprites);
void sprites_screen_set(sprites_object* const sprites, const float width, const float height);

bool sprites_add(sprites_object* const sprites, const size_t num_added, sprite_type* const added_sprites);
bool sprites_draw(sprites_object* const sprites, data_texture_object* const sheet);
