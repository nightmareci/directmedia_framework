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

#include "framework/font.h"
#include "framework/dict.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define FONT_FORMAT_VERSION 3u

#define GET_UINT32(data) ( \
	((uint32_t)(data)[3] << 24) | \
	((uint32_t)(data)[2] << 16) | \
	((uint32_t)(data)[1] <<  8) | \
	((uint32_t)(data)[0] <<  0) \
)
#define GET_INT32(data) ((int32_t)GET_UINT32((data)))
#define GET_UINT16(data) ( \
	((uint16_t)(data)[1] << 8) | \
	((uint16_t)(data)[0] << 0) \
)
#define GET_INT16(data) ((int16_t)GET_UINT16((data)))

typedef enum font_block_enum {
	FONT_BLOCK_INFO = 1,
	FONT_BLOCK_COMMON,
	FONT_BLOCK_PAGES,
	FONT_BLOCK_CHARS,
	FONT_BLOCK_KERNING_PAIRS
} font_block_enum;

static bool destroy_kerning_pair(void* unused) {
	return true;
}

static bool destroy_font_char(void* data) {
	free(data);
	return true;
}

font_struct* font_create(const void* const data, const size_t size) {
	font_struct* const font = (font_struct*)malloc(sizeof(font_struct));
	if (font == NULL) {
		return NULL;
	}

	const uint8_t* const blob_data = (uint8_t*)data;
	const size_t blob_size = size;

	font->format = FONT_FORMAT_BINARY;

	size_t blob_offset = 0u;
	size_t block_size;
	const uint8_t* block_data;

	if (blob_size < blob_offset + 4u || blob_data[0] != 'B' || blob_data[1] != 'M' || blob_data[2] != 'F' || blob_data[3] != FONT_FORMAT_VERSION) {
		free(font);
		return NULL;
	}
	blob_offset += 4u;

	if (blob_size < blob_offset + 1u + 4u || blob_data[blob_offset + 0u] != FONT_BLOCK_INFO || (block_size = GET_UINT32(blob_data + blob_offset + 1u), block_size < 14u) || blob_size < blob_offset + 1u + 4u + block_size) {
		free(font);
		return NULL;
	}
	blob_offset += (size_t)(1u + 4u);

	block_data = blob_data + blob_offset;
	font->font_size = GET_INT16(block_data + 0u);
	font->bits1 = (font_bits1_enum)block_data[2];
	font->char_set = block_data[3];
	font->stretch_h = GET_UINT16(block_data + 4u);
	font->antialiasing = !!block_data[6];
	font->padding_up = block_data[7];
	font->padding_right = block_data[8];
	font->padding_down = block_data[9];
	font->padding_left = block_data[10];
	font->spacing_horiz = block_data[11];
	font->spacing_vert = block_data[12];
	font->outline = block_data[13];
	if (block_size > 14u) {
		font->font_name = (char*)malloc(block_size - 14u);
		if (font->font_name == NULL) {
			free(font);
			return NULL;
		}
		memcpy(font->font_name, block_data + 14u, block_size - 14u);

		bool valid = false;
		for (size_t i = 0u; i < block_size - 14u; i++) {
			if (font->font_name[i] == '\0') {
				valid = true;
				break;
			}
		}
		if (!valid) {
			free(font->font_name);
			free(font);
			return NULL;
		}
	}
	else {
		font->font_name = NULL;
	}
	blob_offset += block_size;

	if (blob_size < blob_offset + 1u + 4u || blob_data[blob_offset + 0u] != FONT_BLOCK_COMMON || (block_size = GET_UINT32(blob_data + blob_offset + 1u), block_size < 15u) || blob_size < blob_offset + 1u + 4u + block_size) {
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	blob_offset += (size_t)(1u + 4u);

	block_data = blob_data + blob_offset;
	font->line_h = GET_UINT16(block_data + 0u);
	font->base = GET_UINT16(block_data + 2u);
	font->scale_w = GET_UINT16(block_data + 4u);
	font->scale_h = GET_UINT16(block_data + 6u);
	font->num_pages = GET_UINT16(block_data + 8u);
	if (font->num_pages == 0u) {
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	font->bits2 = (font_bits2_enum)block_data[10];
	font->alpha_channel = block_data[11];
	font->red_channel = block_data[12];
	font->green_channel = block_data[13];
	font->blue_channel = block_data[14];
	blob_offset += block_size;

	if (blob_size < blob_offset + 1u + 4u || blob_data[blob_offset + 0u] != FONT_BLOCK_PAGES || (block_size = GET_UINT32(blob_data + blob_offset + 1u), block_size < 2u || (block_size / font->num_pages) * font->num_pages != block_size) || blob_size < blob_offset + 1u + 4u + block_size) {
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	blob_offset += (size_t)(1u + 4u);

	block_data = blob_data + blob_offset;
	font->page_names = (char**)malloc(font->num_pages * sizeof(char*));
	if (font->page_names == NULL) {
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	char* const page_names_data = (char*)malloc(block_size);
	if (page_names_data == NULL) {
		free(font->page_names);
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	memcpy(page_names_data, block_data, block_size);
	const size_t name_length = block_size / font->num_pages;
	for (size_t i = 0u; i < font->num_pages; i++) {
		font->page_names[i] = page_names_data + name_length * i;

		bool valid = false;
		for (size_t j = 0u; j < name_length; j++) {
			if (font->page_names[i][j] == '\0') {
				valid = j > 0u;
				break;
			}
		}
		if (!valid) {
			free(*font->page_names);
			free(font->page_names);
			if (font->font_name != NULL) {
				free(font->font_name);
			}
			free(font);
			return NULL;
		}
	}
	blob_offset += block_size;

	if (blob_size < blob_offset + 1u + 4u || blob_data[blob_offset + 0u] != FONT_BLOCK_CHARS || (block_size = GET_UINT32(blob_data + blob_offset + 1u), block_size < 20u || block_size % 20u != 0u) || blob_size < blob_offset + 1u + 4u + block_size) {
		free(*font->page_names);
		free(font->page_names);
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	blob_offset += (size_t)(1u + 4u);

	block_data = blob_data + blob_offset;
	const size_t num_chars = block_size / 20u;
	dict_struct* const font_chars = dict_create(num_chars);
	if (font_chars == NULL) {
		free(*font->page_names);
		free(font->page_names);
		if (font->font_name != NULL) {
			free(font->font_name);
		}
		free(font);
		return NULL;
	}
	for (size_t i = 0u; i < num_chars; i++) {
		font_char_struct* const font_char = (font_char_struct*)malloc(sizeof(font_char_struct));
		if (font_char == NULL) {
			dict_destroy(font_chars);
			free(*font->page_names);
			free(font->page_names);
			if (font->font_name != NULL) {
				free(font->font_name);
			}
			free(font);
			return NULL;
		}

		const uint8_t* const char_data = block_data + i * 20u;
		const size_t id = GET_UINT32(char_data + 0u);
		font_char->x = GET_UINT16(char_data + 4);
		font_char->y = GET_UINT16(char_data + 6);
		font_char->w = GET_UINT16(char_data + 8);
		font_char->h = GET_UINT16(char_data + 10);
		font_char->x_offset = GET_INT16(char_data + 12);
		font_char->y_offset = GET_INT16(char_data + 14);
		font_char->x_advance = GET_INT16(char_data + 16);
		font_char->page = char_data[18];
		font_char->channel = char_data[19];

		if (!dict_set(font_chars, &id, sizeof(id), font_char, sizeof(font_char), destroy_font_char, NULL)) {
			free(font_char);
			dict_destroy(font_chars);
			free(*font->page_names);
			free(font->page_names);
			if (font->font_name != NULL) {
				free(font->font_name);
			}
			free(font);
			return NULL;
		}
	}
	font->chars = (font_chars_struct*)font_chars;
	blob_offset += block_size;

	if (blob_size > blob_offset) {
		if (blob_size < blob_offset + 1u + 4u || blob_data[blob_offset + 0u] != FONT_BLOCK_KERNING_PAIRS || (block_size = GET_UINT32(blob_data + blob_offset + 1u), block_size % 10u != 0u) || blob_size < blob_offset + 1u + 4u + block_size) {
			dict_destroy(font_chars);
			free(*font->page_names);
			free(font->page_names);
			if (font->font_name != NULL) {
				free(font->font_name);
			}
			free(font);
			return NULL;
		}
		blob_offset += (size_t)(1u + 4u);

		block_data = blob_data + blob_offset;
		size_t num_kerning_pairs = block_size / 10u;
		dict_struct* const kerning_pairs = dict_create(num_kerning_pairs);
		if (kerning_pairs == NULL) {
			dict_destroy(font_chars);
			free(*font->page_names);
			free(font->page_names);
			if (font->font_name != NULL) {
				free(font->font_name);
			}
			free(font);
			return NULL;
		}
		for (size_t i = 0u; i < num_kerning_pairs; i++) {
			const uint8_t* const kerning_pair_data = block_data + i * 10u;
			const size_t pair[2] = {
				GET_UINT32(kerning_pair_data + 0u),
				GET_UINT32(kerning_pair_data + 4u)
			};
			const ptrdiff_t amount = GET_INT16(kerning_pair_data + 8u);
			if (!dict_set(kerning_pairs, (const void*)pair, sizeof(pair), (void*)(intptr_t)amount, sizeof(void*), destroy_kerning_pair, NULL)) {
				dict_destroy(kerning_pairs);
				dict_destroy(font_chars);
				free(*font->page_names);
				free(font->page_names);
				if (font->font_name != NULL) {
					free(font->font_name);
				}
				free(font);
				return NULL;
			}
		}
		font->kerning_pairs = (font_kerning_pairs_struct*)kerning_pairs;
	}
	else {
		font->kerning_pairs = NULL;
	}

	return font;
}

bool font_destroy(font_struct* const font) {
	dict_destroy((dict_struct*)font->kerning_pairs);
	dict_destroy((dict_struct*)font->chars);
	free(*font->page_names);
	free(font->page_names);
	if (font->font_name != NULL) {
		free(font->font_name);
	}
	free(font);
	return true;
}

bool font_kerning_amount_get(font_struct* const font, const size_t first, const size_t second, ptrdiff_t* const amount) {
	const size_t pair[2] = { first, second };
	void* amount_value;
	if (!dict_get((dict_struct*)font->kerning_pairs, (const void*)pair, sizeof(pair), &amount_value, NULL)) {
		return false;
	}

	*amount = (int16_t)(intptr_t)amount_value;
	return true;
}

bool font_char_get(font_struct* const font, const size_t id, const font_char_struct** const font_char) {
	dict_struct* const font_chars = (dict_struct*)font->chars;
	if (dict_get(font_chars, &id, sizeof(id), (void**)font_char, NULL)) {
		return true;
	}
	else {
		return false;
	}
}
