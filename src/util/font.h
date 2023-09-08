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
 * Bitmap font loading library. This library only interprets the raw bitmap
 * font data, does no file nor image handling, and is pure in-memory, portable
 * code; file and image management must be done externally. This library is not
 * thread-safe.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum font_format_type {
	FONT_FORMAT_TEXT, // TODO: Add support for the text format.
	FONT_FORMAT_BINARY
} font_format_type;

typedef enum font_bits1_flag {
	FONT_BITS1_SMOOTH  = 1 << 0,
	FONT_BITS1_UNICODE = 1 << 1,
	FONT_BITS1_ITALIC  = 1 << 2,
	FONT_BITS1_BOLD    = 1 << 3,
	FONT_BITS1_FIXED   = 1 << 4
} font_bits1_flag;

typedef enum font_bits2_flag {
	FONT_BITS2_PACKED = 1 << 7
} font_bits2_flag;

typedef struct font_char_object {
	size_t x, y;
	size_t w, h;
	ptrdiff_t x_offset, y_offset;
	ptrdiff_t x_advance;
	size_t page;
	size_t channel;
} font_char_object;

typedef struct font_kerning_pair_object {
	size_t first, second;
	ptrdiff_t amount;
} font_kerning_pair_object;

typedef struct font_kerning_pairs_object font_kerning_pairs_object;
typedef struct font_chars_object font_chars_object;

typedef struct font_object {
	font_format_type format;

	// info
	ptrdiff_t font_size;
	font_bits1_flag bits1;
	size_t char_set;
	size_t stretch_h;
	bool antialiasing;
	size_t padding_up, padding_right, padding_down, padding_left;
	size_t spacing_horiz, spacing_vert;
	bool outline;
	char* font_name;

	// common
	size_t line_h;
	size_t base;
	size_t scale_w, scale_h;
	size_t num_pages;
	font_bits2_flag bits2;
	uint8_t alpha_channel, red_channel, green_channel, blue_channel;

	// page
	char** page_names;

	// char
	font_chars_object* chars;

	// kerning
	font_kerning_pairs_object* kerning_pairs;
} font_object;

font_object* font_create(const void* const data, const size_t size);
bool font_destroy(font_object* const font);

bool font_kerning_amount_get(font_object* const font, const size_t first, const size_t second, ptrdiff_t* const amount);
bool font_char_get(font_object* const font, const size_t id, const font_char_object** const font_char);
