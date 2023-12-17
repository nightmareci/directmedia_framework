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

#include "util/log.h"

/*
 * TODO: Implement some means for logging when the current thread's log filename
 * hasn't been set. Not having that feature means functions called before
 * logging has been initialized can't log errors.
 */

/*
 * Initialize logging support for the whole application. Must be called in the
 * main thread, before any of the other functions are called.
 *
 * If all_output isn't NULL, all logged text is written to the named file,
 * (hopefully) eventually; if all_output is NULL, output will only go to the
 * individual threads' log files, as specified above. The exact string "stdout"
 * is special cased to output to stdout, not into a file in the filesystem.
 * Because concurrency has a bit of "unpredictability", the ordering of output
 * might be out of order with respect to the real time the threads have
 * submitted logging operations, but at the very least each log operation will
 * fully complete before the next; log operations won't interrupt each other.
 */
bool log_init(const char* const all_output);

/*
 * Set the log filename used for the current thread. Returns true if setting was
 * successful, otherwise false in the case of errors.
 */
bool log_filename_set(const char* const filename);
