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
 * Simple render API. Layer indices are ordered bottom-to-top, 0 on up.
 */

#include "render/render_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct render_settings_type {
	float width;
	float height;
} render_settings_type;

/*
 * Start a render frame using the passed-in settings. Other render API functions
 * may be called after calling this function, terminated by a single render_end
 * call.
 */
bool render_start(const render_settings_type* const settings);

/*
 * End a render frame. Must be called; it's fatally erroneous to not pair start
 * and end.
 */
bool render_end();

/*
 * Clear the rendered screen using the specified RGBA color. Colors are
 * specified in the range [0.0f, 1.0f], 0.0f as no-color (black) and 1.0f as
 * full-color (white). Only clears within the app render content rectangle, not
 * the entire contents of the window, in cases where the render content
 * mismatches the aspect ratio of the window content. The window content outside
 * the rendered rectangle is always pure black.
 */
bool render_clear(const float red, const float green, const float blue, const float alpha);

/*
 * Render the requested list of sprites. For best performance, batch up sprites
 * as largely as possible, into fewer render_sprites calls.
 */
bool render_sprites(const char* const sheet_filename, const size_t layer_index, const size_t num_added, const sprite_type* const added_sprites);

/*
 * Render the requested string directly, without formatting interpretation, using the indicated font.
 */
bool render_string(const char* const font_filename, const size_t layer_index, const float x, const float y, const char* const string);

/*
 * Render the requested formatted string, using the indicated font.
 */
bool render_printf(const char* const font_filename, const size_t layer_index, const float x, const float y, const char* const format, ...);
