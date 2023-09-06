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
#include "util/util.h"

static bool create(void* const data_param, SDL_RWops* const rwops);
static bool destroy(void* const data_param);

const data_type_manager data_type_manager_music = {
	.create = create,
	.destroy = destroy
};

static bool create(void* const data_param, SDL_RWops* const rwops) {
	data_object* const data = (data_object*)data_param;

	data->music = Mix_LoadMUS_RW(rwops, 0);

	return data->music != NULL;
}

static bool destroy(void* const data_param) {
	data_object* const data = (data_object*)data_param;

	Mix_FreeMusic(data->music);
	mem_free((void*)data->id.filename);
	mem_free((void*)data);

	return true;
}