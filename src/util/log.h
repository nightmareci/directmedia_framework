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

#include <stdbool.h>

/*
 * Thread-safe stdio-style logging facility. Thread safety of logging is
 * achieved via one-log-file-per-thread.
 *
 * You should probably stick to UTF-8 encoding for the strings.
 */

/*
 * Simply log a text string, verbatim. No formatting or interpretation of the
 * text is done. It is erroneous to request log operations before setting the
 * filename for the current thread.
 */
bool log_text(const char* const text);

/*
 * Log some text printf-style. Internally just uses sprintf, so read the sprintf
 * documentation for full usage. It is erroneous to request log operations
 * before setting the filename for the current thread.
 */
bool log_printf(const char* const format, ...);
