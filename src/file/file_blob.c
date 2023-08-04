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

#include "file/file.h"
#include <stdlib.h>

static bool create(void* const file_param, SDL_RWops* const rwops);
static bool destroy(void* const file_param);

const file_type_manager_struct file_type_manager_blob = {
	.create = create,
	.destroy = destroy
};

static bool create(void* const file_param, SDL_RWops* const rwops) {
	file_struct* const file = (file_struct*)file_param;

	const Sint64 size = SDL_RWsize(rwops);
	if (size <= 0) {
		return false;
	}

	const size_t base_size = offsetof(file_blob_struct, data) + size;
	const size_t align = 16u - (base_size % 16u);
    file_blob_struct* const blob = (file_blob_struct*)malloc(base_size + (align % 16u));
	if (blob == NULL) {
		return false;
	}

	blob->size = size;

	const size_t read = SDL_RWread(rwops, blob->data, 1u, size);
	if (read == 0u || read != size) {
		free(blob);
		return false;
	}

	file->blob = blob;
	return true;
}

static bool destroy(void* const file_param) {
	file_struct* const file = (file_struct*)file_param;

	free((void*)file->blob);
	free((void*)file->description.filename);
	free((void*)file);

	return true;
}
