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
#include <stdlib.h>
#include <assert.h>

typedef struct node_struct node_struct;
struct node_struct {
    void* value;
    node_struct* next;
};

struct queue_struct {
    node_struct* dequeue;
	node_struct* enqueue;
    node_struct* cache;
};

queue_struct* queue_create() {
    queue_struct* const queue = malloc(sizeof(queue_struct));
    if (queue == NULL) {
        return NULL;
    }

    queue->dequeue = queue->enqueue = calloc(1u, sizeof(node_struct));
    if (queue->enqueue == NULL) {
        free(queue);
        return false;
    }
    queue->cache = NULL;

	return queue;
}

void queue_destroy(queue_struct* const queue) {
    assert(queue != NULL);

    while (queue_dequeue(queue) != NULL) continue;
    queue_empty_cache(queue);
    free(queue);
}

bool queue_enqueue(queue_struct* const queue, void* const value) {
    assert(queue != NULL && value != NULL);

    node_struct* node;
    if (queue->cache != NULL) {
        node = queue->cache;
        queue->cache = queue->cache->next;
    }
    else {
        node = malloc(sizeof(node_struct));
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

void* queue_dequeue(queue_struct* const queue) {
    assert(queue != NULL);

    if (queue->dequeue->next == NULL) {
        return NULL;
    }

    node_struct* const dequeue = queue->dequeue;
    void* const value = dequeue->next->value;
    queue->dequeue = dequeue->next;
    dequeue->next = queue->cache;
    queue->cache = dequeue;

    return value;
}

void queue_empty_cache(queue_struct* const queue) {
    assert(queue != NULL);

    for (node_struct* current = queue->cache; current != NULL;) {
        node_struct* const next = current->next;
        free(current);
        current = next;
    }
}
