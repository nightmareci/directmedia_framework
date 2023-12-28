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

/*
 * Thread-safe logging API. In the case of errors indicated by these functions'
 * return value, you should revert to some other means of communicating an
 * error, such as returning EXIT_FAILURE to the operating system, and/or
 * directly printing to stdout only in the main thread.
 */

#include <stdarg.h>
#include <stdbool.h>

/*
 * Simply log a text string, verbatim. No formatting or interpretation of the
 * text is done. It is erroneous to request log operations before setting the
 * filename for the current thread.
 */
void log_text(const char* const text);

/*
 * Log some text printf-style. Internally just uses sprintf, so read the sprintf
 * documentation for full usage. It is erroneous to request log operations
 * before setting the filename for the current thread.
 */
void log_printf(const char* const format, ...);

/*
 * Log some text vprintf-style. Internally just uses sprintf, so read the
 * sprintf documentation for full usage. It is erroneous to request log
 * operations before setting the filename for the current thread.
 */
void log_vprintf(const char* const format, va_list args);
