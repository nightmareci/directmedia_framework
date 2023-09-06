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

#include "util/util.h"
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/*
 * Similar to sprintf, except it returns a string containing the output. The
 * string must be freed with mem_free when you're done with it.
 */
char* alloc_sprintf(const char* const fmt, ...);

/*
 * Similar to vsprintf, except it returns a string containing the output. The
 * string must be freed with mem_free when you're done with it.
 */
char* alloc_vsprintf(const char* const fmt, va_list args);

#ifndef _WIN32
/*
 * Compare strings case insensitively. The return value is the same as for
 * standard C strcmp.
 */
int strcmpi(const char* lhs, const char* rhs);
#endif

/*
 * Convert English alphabetic characters to uppercase.
 */
void strtoupper(char* const str, const size_t size);

/*
 * Convert English alphabetic characters to lowercase.
 */
void strtolower(char* const str, const size_t size);

/*
 * Get the Unicode codepoint corresponding to the first character in the UTF-8
 * encoded string str. bytes is set to the number of bytes that represent the
 * character, that can be added to the input str to advance to the next UTF-8
 * encoded character. In the case of the data at str not being valid UTF-8 data,
 * Unicode REPLACEMENT CHARACTER (U+FFFD) is returned and bytes is set to 1.
 */
uint32_t utf8_get(const char* const str, size_t* const bytes);

/*
 * Returns the number of UTF-8-encoded characters in the string.
 */
size_t utf8_strlen(const char* const str);
