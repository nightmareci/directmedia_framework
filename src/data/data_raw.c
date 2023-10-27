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
#include "util/mem.h"
#include <stdlib.h>

static bool create(data_object* const data, SDL_RWops* const rwops) {
	const Sint64 size = SDL_RWsize(rwops);
	if (size <= 0) {
		return false;
	}

	const size_t base_size = offsetof(data_raw_object, bytes) + size;
	const size_t align = 16u - (base_size % 16u);
    data_raw_object* const raw = (data_raw_object*)mem_aligned_alloc(16u, base_size + (align % 16u));
	if (raw == NULL) {
		return false;
	}

	raw->size = size;

	const size_t read = SDL_RWread(rwops, raw->bytes, 1u, size);
	if (read == 0u || read != size) {
		mem_aligned_free(raw);
		return false;
	}

	data->raw = raw;
	return true;
}

static bool destroy(data_object* const data) {
	mem_aligned_free((void*)data->raw);
	mem_free((void*)data->id.filename);
	mem_free((void*)data);

	return true;
}

DATA_TYPE_MANAGER_DEFINITION(data_type_manager_raw, create, destroy);
