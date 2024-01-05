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
 * Simple sprite graphics API. First, you should probably call sprites_restart
 * before calling any other methods at the start of every frame. Then, call the
 * functions other than sprites_restart and sprites_draw to prepare what you
 * want drawn. Then, after the desired content to draw has been submitted, call
 * sprites_draw once at the end of a frame.
 *
 * sprites_resize and sprites_shrink can optionally be used for a degree of
 * management of the memory in use by a sprites_object. The library internally
 * expands the amount of memory used to accommodate what is required, while also
 * attempting reuse of memory between calls of sprites_restart and sprites_draw;
 * if you never call sprites_resize and sprites_shrink, the memory used will
 * only grow as required, and no larger. For example, after a context change in
 * the application, if you wanted to reset the amount of memory the
 * sprites_object is using to a minimum, call sprites_resize with num_sprites as
 * 0.
 */

#include "render/render_types.h"
#include "data/data_texture.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct sprites_object sprites_object;

sprites_object* sprites_create(const size_t initial_size);
void sprites_destroy(sprites_object* const sprites);

bool sprites_resize(sprites_object*const sprites, const size_t num_sprites);
bool sprites_shrink(sprites_object* const sprites);

void sprites_restart(sprites_object* const sprites);

void sprites_screen_reset(sprites_object* const sprites);
void sprites_screen_set(sprites_object* const sprites, const float width, const float height);

bool sprites_add(sprites_object* const sprites, data_texture_object* const sheet, const size_t num_added, sprite_type* const added_sprites);
bool sprites_draw(sprites_object* const sprites);
