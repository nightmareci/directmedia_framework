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

#include "util/queue.h"
#include "util/mem.h"
#include <stdlib.h>
#include <assert.h>

typedef struct node_object node_object;
struct node_object {
    void* value;
    node_object* next;
};

struct queue_object {
    node_object* dequeue;
	node_object* enqueue;
    node_object* cache;
};

queue_object* queue_create() {
    queue_object* const queue = mem_malloc(sizeof(queue_object));
    if (queue == NULL) {
        return NULL;
    }

    queue->dequeue = queue->enqueue = mem_calloc(1u, sizeof(node_object));
    if (queue->enqueue == NULL) {
        mem_free(queue);
        return false;
    }
    queue->cache = NULL;

	return queue;
}

void queue_destroy(queue_object* const queue) {
    assert(queue != NULL);

    while (queue_dequeue(queue) != NULL) continue;
    mem_free(queue->dequeue);
    queue_empty_cache(queue);
    mem_free(queue);
}

bool queue_enqueue(queue_object* const queue, void* const value) {
    assert(queue != NULL);
    assert(value != NULL);

    node_object* node;
    if (queue->cache != NULL) {
        node = queue->cache;
        queue->cache = queue->cache->next;
    }
    else {
        node = mem_malloc(sizeof(node_object));
    }
    if (node == NULL) {
        return false;
    }

    node->value = value;
    node->next = NULL;
    queue->enqueue->next = node;
    queue->enqueue = node;

    return true;
}

void* queue_dequeue(queue_object* const queue) {
    assert(queue != NULL);

    if (queue->dequeue->next == NULL) {
        return NULL;
    }

    node_object* const dequeue = queue->dequeue;
    void* const value = dequeue->next->value;
    queue->dequeue = dequeue->next;
    dequeue->next = queue->cache;
    queue->cache = dequeue;

    return value;
}

void queue_empty_cache(queue_object* const queue) {
    assert(queue != NULL);

    for (node_object* current = queue->cache; current != NULL;) {
        node_object* const next = current->next;
        mem_free(current);
        current = next;
    }
}
