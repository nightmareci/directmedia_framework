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

#include "framework/file.h"

static bool create(void* const file_param, SDL_RWops* const rwops);
static bool destroy(void* const file_param);

const file_type_manager_struct file_type_manager_music = {
	.create = create,
	.destroy = destroy
};

static bool create(void* const file_param, SDL_RWops* const rwops) {
	file_struct* const file = (file_struct*)file_param;

	file->music = Mix_LoadMUS_RW(rwops, 0);

	return file->music != NULL;
}

static bool destroy(void* const file_param) {
	file_struct* const file = (file_struct*)file_param;

	Mix_FreeMusic(file->music);
	free((void*)file->description.filename);
	free((void*)file);

	return true;
}
