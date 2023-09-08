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

#include "data/data.h"
#include "opengl/opengl.h"
#include "util/util.h"
#include "util/string_util.h"

static bool create(data_object* const data, SDL_RWops* const rwops);
static bool destroy(data_object* const data);

const data_type_manager data_type_manager_font = {
	.create = create,
	.destroy = destroy
};

static bool create(data_object* const data, SDL_RWops* const rwops) {
	const Sint64 size = SDL_RWsize(rwops);
	if (size <= 0) {
		return false;
	}

	void* bytes = mem_malloc(size);
	if (bytes == NULL) {
		return false;
	}

	const size_t read = SDL_RWread(rwops, bytes, 1u, size);
	if (read == 0u || read != size) {
		mem_free(bytes);
		return false;
	}
    
    data_font_object* const font = (data_font_object*)mem_malloc(sizeof(data_font_object));
	if (font == NULL) {
		mem_free(bytes);
		return false;
	}

	font->font = font_create(bytes, size);
	if (font->font == NULL) {
		mem_free(font);
		mem_free(bytes);
		return false;
	}

	mem_free(bytes);

    font->textures = (const data_object**)mem_malloc(font->font->num_pages * sizeof(data_object*));
	if (font->textures == NULL) {
		font_destroy(font->font);
		mem_free(font);
		return false;
	}

	char* const directory = data_directory_get(data);
	if (directory == NULL) {
		mem_free(font->textures);
		font_destroy(font->font);
		mem_free(font);
		return false;
	}

	for (size_t page = 0u; page < font->font->num_pages; page++) {
		char* const page_filename = alloc_sprintf("%s%s", directory, font->font->page_names[page]);
		if (page_filename == NULL) {
			for (size_t i = 0u; i < page; i++) {
				data_unload(data->font->textures[page]);
			}
			mem_free(directory);
			mem_free(font->textures);
			font_destroy(font->font);
			mem_free(font);
			return false;
		}

		const data_object* const page_data = data_load(data->cache, DATA_TYPE_TEXTURE, data->id.path, page_filename, NULL);
		mem_free(page_filename);

		if (page_data == NULL) {
			for (size_t i = 0u; i < page; i++) {
				data_unload(data->font->textures[page]);
			}
			mem_free(directory);
			mem_free(font->textures);
			font_destroy(font->font);
			mem_free(font);
			return false;
		}
		font->textures[page] = page_data;
	}
	mem_free(directory);

	data->font = font;
	return true;
}

static bool destroy(data_object* const data) {
	for (size_t page = 0u; page < data->font->font->num_pages; page++) {
		data_unload(data->font->textures[page]);
	}
	const bool status = font_destroy(data->font->font);
	mem_free((void*)data->font->textures);
	mem_free((void*)data->font);
	mem_free((void*)data->id.filename);
	mem_free((void*)data);

	return status;
}
