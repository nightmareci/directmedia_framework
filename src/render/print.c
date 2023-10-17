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

#include "render/print.h"
#include "util/string_util.h"
#include "util/util.h"
#include <stdlib.h>
#include <stdarg.h>
#include <float.h>
#include <assert.h>

bool print_layer_text(data_font_object* const font, layers_object* const layers, const size_t layer_index, const float x, const float y, const char* const text) {
	assert(font != NULL);
	assert(layers != NULL);
	assert(x >= -FLT_MAX);
	assert(x <= FLT_MAX);
	assert(y >= -FLT_MAX);
	assert(y <= FLT_MAX);
	assert(text != NULL);

	/*
	 * TODO: Create a new font generator that sets the Unicode bit properly. It
	 * seems the AngelCode generator has a bug, where it doesn't set the
	 * Unicode bit for Unicode binary format fonts.
	 */
	//if (!(font->font->bits1 & FONT_BITS1_UNICODE)) {
	if (font->font->format != FONT_FORMAT_BINARY && !(font->font->bits1 & FONT_BITS1_UNICODE)) {
		fprintf(stderr, "Error: Font used for printing text is not a Unicode font\n");
		fflush(stderr);
		return false;
	}

	const size_t len = utf8_strlen(text);
	if (len == 0u) {
		return true;
	}

	float print_x = x, print_y = y;
	size_t bytes;
	uint32_t c = utf8_get(text, &bytes);
	if (c == UINT32_C(0xFFFD) && bytes == 1u) {
		return false;
	}

	const char* s = text + bytes;

	sprite_type* const sprites = mem_malloc(sizeof(sprite_type) * len);
	if (sprites == NULL) {
		return false;
	}

	size_t sequence_len = 0u;
	size_t sequence_page = font->font->num_pages;

	for (uint32_t next_c = 0u; c != 0u; c = next_c, s += bytes) {
		next_c = utf8_get(s, &bytes);
		if (next_c == UINT32_C(0xFFFD) && bytes == 1u) {
			return false;
		}
		if (c == '\n' || c == '\r') {
			print_x = x;
			print_y += font->font->line_h;
			if (
				(c == '\n' && next_c == '\r') ||
				(c == '\r' && next_c == '\n')
			) {
				s += bytes;
				next_c = utf8_get(s, &bytes);
				if (next_c == UINT32_C(0xFFFD) && bytes == 1u) {
					return false;
				}
			}
			continue;
		}

		const font_char_object* font_c;
		if (!font_char_get(font->font, c, &font_c)) {
			continue;
		}

		if (sequence_page == font->font->num_pages) {
			sequence_page = font_c->page;
		}
		if (sequence_page != font_c->page) {
			if (!layers_sprites_add(layers, font->textures[sequence_page]->texture, layer_index, sequence_len, sprites)) {
				mem_free(sprites);
				return false;
			}
			sequence_len = 0u;
			sequence_page = font_c->page;
		}

		sprites[sequence_len].src[0] = font_c->x;
		sprites[sequence_len].src[1] = font_c->y;
		sprites[sequence_len].src[2] = font_c->w;
		sprites[sequence_len].src[3] = font_c->h;

		sprites[sequence_len].dst[0] = print_x + font_c->x_offset;
		sprites[sequence_len].dst[1] = print_y + font_c->y_offset;
		sprites[sequence_len].dst[2] = font_c->w;
		sprites[sequence_len].dst[3] = font_c->h;

		sequence_len++;

		print_x += font_c->x_advance;
		ptrdiff_t amount;
		if (font_kerning_amount_get(font->font, c, next_c, &amount)) {
			print_x += amount;
		}
	}

	if (!layers_sprites_add(layers, font->textures[sequence_page]->texture, layer_index, sequence_len, sprites)) {
		mem_free(sprites);
		return false;
	}

	mem_free(sprites);

	return true;
}

bool print_layer_formatted(data_font_object* const font, layers_object* const layers, const size_t layer_index, const float x, const float y, const char* const format, ...) {
	assert(font != NULL);
	assert(layers != NULL);
	assert(x >= -FLT_MAX);
	assert(x <= FLT_MAX);
	assert(y >= -FLT_MAX);
	assert(y <= FLT_MAX);
	assert(format != NULL);

	va_list args;
	va_start(args, format);
	char* const text = alloc_vsprintf(format, args);
	va_end(args);
	if (text == NULL) {
		return false;
	}

	const bool success = print_layer_text(font, layers, layer_index, x, y, text);
	mem_free(text);
	return success;
}
