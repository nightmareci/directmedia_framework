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

#include "framework/frames.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct render_rect_struct {
	float x, y;
	float w, h;
} render_rect_struct;

typedef struct render_sprite_struct {
	render_rect_struct src;
	render_rect_struct dst;
} render_sprite_struct;

bool render_init(frames_struct* const frames);

bool render_deinit(frames_struct* const frames);

bool render_clear(frames_struct* const frames, const uint8_t shade);

bool render_sprites(frames_struct* const frames, const char* const image_filename, const render_sprite_struct* const sprites);

bool render_print(frames_struct* const frames, const char* const font, const float x, const float y, const char* const format, ...);
