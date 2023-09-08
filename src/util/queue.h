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
 * Single-threaded-only queue. Only if you need to access the queue concurrently in
 * multiple threads, use the conqueue type (util/conqueue.h).
 */

#include <stdbool.h>

typedef struct queue_object queue_object;

/*
 * Create an empty queue object. If creation of the queue object failed, returns
 * NULL, indicating a fatal error.
 */
queue_object* queue_create();

/*
 * Destroy a queue object. Also empties the queue, if there are values already
 * queued.
 */
void queue_destroy(queue_object* const queue);

/*
 * Enqueue a value pointer onto the queue. Returns true if enqueueing was
 * successful, otherwise returns false in the case of fatal errors. Because
 * queue_dequeue returns NULL for an empty queue, it is invalid to attempt to
 * enqueue NULL.
 */
bool queue_enqueue(queue_object* const queue, void* const value);

/*
 * Dequeue a value pointer from the queue. Returns a non-NULL value pointer if
 * successful, otherwise returns NULL if the queue is now empty.
 */
void* queue_dequeue(queue_object* const queue);

/*
 * Empty out the internal queue node cache.
 */
void queue_empty_cache(queue_object* const queue);
