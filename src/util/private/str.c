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

#include "util/str.h"
#include "util/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <assert.h>

char* alloc_sprintf(const char* const fmt, ...) {
	assert(fmt != NULL);

	va_list args_list1;
	va_start(args_list1, fmt);
	va_list args_list2;
	va_copy(args_list2, args_list1);
	const int sz = vsnprintf(NULL, 0, fmt, args_list1) + 1;
	va_end(args_list1);
	if (sz < 0) {
		va_end(args_list2);
		return NULL;
	}
	char* printout = mem_malloc(sz);
	if (!printout) {
		va_end(args_list2);
		return NULL;
	}
	if (vsnprintf(printout, sz, fmt, args_list2) < 0) {
		mem_free(printout);
		va_end(args_list2);
		return NULL;
	}
	va_end(args_list2);
	return printout;
}

char* alloc_vsprintf(const char* const fmt, va_list args) {
	assert(fmt != NULL);

	va_list args_copy;
	va_copy(args_copy, args);
	const int sz = vsnprintf(NULL, 0, fmt, args) + 1;
	if (sz < 0) {
		va_end(args_copy);
		return NULL;
	}
	char* printout = mem_malloc(sz);
	if (!printout) {
		va_end(args_copy);
		return NULL;
	}
	if (vsnprintf(printout, sz, fmt, args_copy) < 0) {
		mem_free(printout);
		va_end(args_copy);
		return NULL;
	}
	va_end(args_copy);
	return printout;
}

#ifndef _MSC_VER
int stricmp(const char* lhs, const char* rhs) {
	assert(lhs != NULL);
	assert(rhs != NULL);

	int lhs_lower;
	int rhs_lower;

	for (
		lhs_lower = tolower(*lhs++), rhs_lower = tolower(*rhs++);
		lhs_lower == rhs_lower && *lhs != '\0' && *rhs != '\0';
		lhs_lower = tolower(*lhs++), rhs_lower = tolower(*rhs++)
	);

	if (lhs_lower < rhs_lower) return -1;
	else if (lhs_lower > rhs_lower) return 1;
	else return 0;
}
#endif

void strntoupper(char* const str, const size_t size) {
	assert(str != NULL);

	for (size_t i = 0u; i < size; i++) {
		str[i] = toupper(str[i]);
	}
}

void strntolower(char* const str, const size_t size) {
	assert(str != NULL);

	for (size_t i = 0u; i < size; i++) {
		str[i] = tolower(str[i]);
	}
}

uint32_t utf8_get(const char* const str, size_t* const bytes) {
	assert(str != NULL);
	assert(bytes != NULL);

	uint32_t codepoint = 0u;
	const char* s = str;

	if ((uint32_t)*s <= 0x7Fu) {
		codepoint = (uint32_t)*s;
		*bytes = 1u;
	}
	else if (((uint32_t)*s & 0xE0u) == 0xC0u) {
		codepoint = (uint32_t)*s & 0x1Fu;
		*bytes = 2u;
	}
	else if (((uint32_t)*s & 0xF0u) == 0xE0u) {
		codepoint = (uint32_t)*s & 0x0Fu;
		*bytes = 3u;
	}
	else if (((uint32_t)*s & 0xF8u) == 0xF0u) {
		codepoint = (uint32_t)*s & 0x07u;
		*bytes = 4u;
	}
	else {
		codepoint = 0xFFFDu; // REPLACEMENT CHARACTER
		*bytes = 1u;
	}

	for (size_t i = 1u; i < *bytes; i++) {
		s++;
		codepoint <<= 6;
		codepoint |= ((uint32_t)*s & 0x3Fu);
	}

	return codepoint;
}

size_t utf8_strlen(const char* const str) {
	assert(str != NULL);

	size_t len = 0u;
	size_t bytes;
	for (
		const char* c = str;
		utf8_get(c, &bytes) != '\0';
		c += bytes, len++
	);
	return len;
}
