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

#include "SDL_rwops.h"
#include <stdbool.h>

typedef struct data_object data_object;

typedef struct data_type_manager {
	bool (* create)(data_object* const data, SDL_RWops* const rwops);
	bool (* destroy)(data_object* const data);
	bool (* destroy_by_dict)(void* const data);
} data_type_manager;

#define DATA_TYPE_MANAGER_DEFINITION(manager_name, create_func, destroy_func) \
static bool manager_name##_destroy_by_dict(void* const data) { \
	return destroy_func((data_object*)data); \
} \
\
const data_type_manager manager_name = { \
	.create = create_func, \
	.destroy = destroy_func, \
	.destroy_by_dict = manager_name##_destroy_by_dict \
};
