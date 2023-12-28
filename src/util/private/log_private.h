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
#include "util/nanotime.h"

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
 *
 * Returns true if initializing was successful, otherwise false in the case of
 * errors.
 */
bool log_init(const char* const all_output);

/*
 * When a filename/stdout is set for all log output, this completes all pending
 * log operations and deinitializes such support.
 *
 * Returns true if deinitializing was successful, otherwise false in the case of
 * errors.
 */
bool log_all_output_deinit();

#define LOG_ALL_OUTPUT_DEQUEUE_EMPTY UINT64_MAX

/*
 * This dequeues and outputs to the all_output filename/stdout all pending log
 * operations. If the amount of time spent dequeueing/outputting goes past the
 * allotted_time, this function immediately returns, possible leaving some log
 * operations still in the queue; it's the responsibility of the user of the
 * logging API to ensure continual dequeues with enough throughput to keep up
 * with submitted log operations, not allowing the queue to grow out of control.
 *
 * However, if it's desired to completely empty the queue,
 * LOG_ALL_OUTPUT_DEQUEUE_EMPTY can be passed as a parameter.
 * LOG_ALL_OUTPUT_DEQUEUE_EMPTY should *only* be used when it's guaranteed no
 * other threads are enqueueing log operations, as it's a race condition if
 * other threads are enqueueing at the same time this function is called.
 *
 * Returns true if dequeueing was successful, otherwise false in the case of
 * errors.
 */
bool log_all_output_dequeue(const uint64_t allotted_time);

/*
 * Set the log filename used for the current thread.
 *
 * Returns true if setting was successful, otherwise false in the case of
 * errors.
 */
bool log_filename_set(const char* const filename);
