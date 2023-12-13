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
 * Multiple producer threads, single consumer thread concurrent queue. When
 * compiled in debug mode, the dequeue and destroy functions verify the correct
 * thread is consuming; the first thread to dequeue or destroy becomes the
 * valid-consumer. Only use this queue type when you need to use it
 * concurrently; it has higher runtime overhead than the single-threaded-only
 * queue type (util/queue.h).
 */

// TODO: Change the implementation to support multiple consumer threads. But do
// provide an option to select between single-consumer/multiple-consumer, as
// multiple-consumer has higher overhead than single-consumer. And investigate
// if single-producer can have lower overhead than multiple-producer; if so,
// also provide that option.

#include <stdbool.h>

typedef struct conqueue_object conqueue_object;

/*
 * Create an empty queue object. If creation of the queue object failed, returns
 * NULL, indicating a fatal error.
 */
conqueue_object* conqueue_create();

/*
 * Destroy a queue object. Also empties the queue, if there are values already
 * enqueued. This function must only be called by the consumer thread.
 */
void conqueue_destroy(conqueue_object* const queue);

/*
 * Enqueue a value pointer onto the queue. Returns true if enqueueing was
 * successful, otherwise returns false in the case of fatal errors. Because
 * conqueue_dequeue returns NULL for an empty queue, it is invalid to attempt to
 * enqueue NULL.
 */
bool conqueue_enqueue(conqueue_object* const queue, void* const value);

/*
 * Dequeue a value pointer from the queue. Returns a non-NULL value pointer if
 * successful, otherwise returns NULL if the queue is now empty. This function
 * must only be called by the consumer thread.
 */
void* conqueue_dequeue(conqueue_object* const queue);
