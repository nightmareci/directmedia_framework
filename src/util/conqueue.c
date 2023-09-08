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

#include "util/conqueue.h"
#include "util/util.h"
#include "SDL_atomic.h"
#include "SDL_thread.h"
#include <stdlib.h>
#include <assert.h>

typedef struct node_object node_object;
struct node_object {
	void* value;
	node_object* next;
};

typedef struct conqueue_object conqueue_object;
struct conqueue_object {
	node_object* enqueue;
	node_object* dequeue;

	#ifndef NDEBUG
	SDL_atomic_t consumer_set;
	SDL_threadID consumer;
	#endif
};

#ifndef NDEBUG
static void assert_current_thread_is_consumer(conqueue_object* const queue) {
	SDL_MemoryBarrierRelease();
	SDL_bool set = SDL_AtomicCAS(&queue->consumer_set, 0, 1);
	SDL_MemoryBarrierAcquire();
	if (set) {
		queue->consumer = SDL_ThreadID();

		SDL_MemoryBarrierRelease();
		SDL_AtomicSet(&queue->consumer_set, 2);
	}
	while (true) {
		const int set = SDL_AtomicGet(&queue->consumer_set);
		SDL_MemoryBarrierAcquire();
		if (set == 2) {
			break;
		}
	}
	assert(SDL_ThreadID() == queue->consumer);
}

#else
#define assert_current_thread_is_consumer(queue) ((void)0)

#endif

void* conqueue_dequeue(conqueue_object* const queue);

conqueue_object* conqueue_create() {
	conqueue_object* const queue = mem_malloc(sizeof(conqueue_object));
	if (queue == NULL) {
		return NULL;
	}

	node_object* const node = mem_malloc(sizeof(node_object));
	if (node == NULL) {
		mem_free(queue);
		return NULL;
	}
	node->value = NULL;

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&node->next, NULL);

	#ifndef NDEBUG
	SDL_MemoryBarrierRelease();
	SDL_AtomicSet(&queue->consumer_set, 0);
	#endif

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&queue->enqueue, node);

	queue->dequeue = node;

	return queue;
}

void conqueue_destroy(conqueue_object* const queue) {
	assert(queue != NULL);
	assert_current_thread_is_consumer(queue);

	while (conqueue_dequeue(queue) != NULL) continue;
	mem_free(queue->dequeue);
	mem_free(queue);
}

bool conqueue_enqueue(conqueue_object* const queue, void* const value) {
	assert(queue != NULL && value != NULL);

	node_object* node = mem_malloc(sizeof(node_object));
	if (node == NULL) {
		return false;
	}
	node->value = value;

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&node->next, NULL);

	node_object* enqueue;
	SDL_bool enqueue_next;

	do {
		enqueue = SDL_AtomicGetPtr((void**)&queue->enqueue);
		SDL_MemoryBarrierAcquire();

		node_object* const next = SDL_AtomicGetPtr((void**)&enqueue->next);
		SDL_MemoryBarrierAcquire();

		if (next != NULL) {
			continue;
		}

		SDL_MemoryBarrierRelease();
		enqueue_next = SDL_AtomicCASPtr((void**)&enqueue->next, next, node);
		SDL_MemoryBarrierAcquire();
	} while (!enqueue_next);

	SDL_MemoryBarrierRelease();
	SDL_AtomicCASPtr((void**)&queue->enqueue, enqueue, node);
	SDL_MemoryBarrierAcquire();
	return true;
}

void* conqueue_dequeue(conqueue_object* const queue) {
	assert(queue != NULL);
	assert_current_thread_is_consumer(queue);

	node_object* const dequeue = queue->dequeue;

	node_object* const next = SDL_AtomicGetPtr((void**)&dequeue->next);
	SDL_MemoryBarrierAcquire();

	if (next != NULL) {
		queue->dequeue = next;
		void* const value = next->value;
		mem_free(dequeue);
		return value;
	}
	else {
		return NULL;
	}
}
