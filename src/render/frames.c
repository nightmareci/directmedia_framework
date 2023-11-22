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

#include "render/frames.h"
#include "util/conqueue.h"
#include "util/queue.h"
#include "util/mem.h"
#include "main/app.h"
#include "opengl/opengl.h"
#include "SDL.h"
#include <stdlib.h>
#include <assert.h>

typedef struct command_object {
	command_funcs funcs;
	void* state;
} command_object;

typedef struct frame_object {
	queue_object* commands;
	bool ended;
} frame_object;

typedef struct frames_object {
	conqueue_object* frame_queues;
	frame_object* next_latest_frame;
	frame_object* latest_frame;
} frames_object;

frames_object* frames_create() {
	frames_object* const frames = mem_calloc(1u, sizeof(frames_object));
	if (frames == NULL) {
		return NULL;
	}

	frames->frame_queues = conqueue_create();
	if (frames->frame_queues == NULL) {
		mem_free(frames);
		return NULL;
	}

	return frames;
}

bool frames_destroy(frames_object* const frames) {
	assert(frames != NULL);
	assert(frames->frame_queues != NULL);

	for (
		frame_object* frame = (frame_object*)conqueue_dequeue(frames->frame_queues);
		frame != NULL;
		queue_destroy(frame->commands), mem_free(frame), frame = (frame_object*)conqueue_dequeue(frames->frame_queues)
	) {
		for (
			command_object* command = (command_object*)queue_dequeue(frame->commands);
			command != NULL;
			mem_free(command), command = (command_object*)queue_dequeue(frame->commands)
		) {
			if (command->funcs.update != NULL && !command->funcs.update(command->state)) {
				return false;
			}

			if (command->funcs.destroy != NULL) {
				command->funcs.destroy(command->state);
			}
		}
		glFlush();
	}
	conqueue_destroy(frames->frame_queues);
	mem_free(frames);

	return true;
}

frames_status_type frames_start(frames_object* const frames) {
	assert(frames != NULL);
	assert(frames->frame_queues != NULL);

	frame_object* const next_frame = mem_malloc(sizeof(frame_object));
	if (next_frame == NULL) {
		return FRAMES_STATUS_ERROR;
	}

	next_frame->commands = queue_create();
	if (next_frame->commands == NULL) {
		mem_free(next_frame);
		return FRAMES_STATUS_ERROR;
	}

	next_frame->ended = false;

	frames->next_latest_frame = next_frame;

	return FRAMES_STATUS_SUCCESS;
}

bool frames_end(frames_object* const frames) {
	assert(frames->next_latest_frame != NULL);

	frames->next_latest_frame->ended = true;
	if (!conqueue_enqueue(frames->frame_queues, frames->next_latest_frame)) {
		queue_destroy(frames->next_latest_frame->commands);
		mem_free(frames->next_latest_frame);
		return false;
	}

	SDL_MemoryBarrierRelease();
	SDL_AtomicSetPtr((void**)&frames->latest_frame, frames->next_latest_frame);
	return true;
}

bool frames_enqueue_command(
	frames_object* const frames,
	const command_funcs* const funcs,
	void* const state
) {
	assert(frames != NULL);
	assert(frames->next_latest_frame != NULL);
	assert(frames->next_latest_frame->commands != NULL);
	assert(funcs != NULL);

	queue_object* const commands = frames->next_latest_frame->commands;

	command_object* const command = mem_malloc(sizeof(command_object));
	if (command == NULL) {
		return false;
	}
	
	command->funcs = *funcs;
	command->state = state;
	
	if (!queue_enqueue(commands, command)) {
		mem_free(command);
		return false;
	}

	return true;
}

frames_status_type frames_draw_latest(frames_object* const frames) {
	assert(frames != NULL);
	assert(frames->frame_queues != NULL);

	frame_object* const latest_frame = SDL_AtomicGetPtr((void**)&frames->latest_frame);
	SDL_MemoryBarrierAcquire();

	if (latest_frame == NULL) {
		return FRAMES_STATUS_NO_FRAMES;
	}

	bool ended = false;
	for (
		frame_object* frame = (frame_object*)conqueue_dequeue(frames->frame_queues);
		frame != NULL;
		queue_destroy(frame->commands), mem_free(frame), frame = (frame_object*)conqueue_dequeue(frames->frame_queues)
	) {
		const bool draw_now = frame == latest_frame;
		for (
			command_object* command = (command_object*)queue_dequeue(frame->commands);
			command != NULL;
			mem_free(command), command = (command_object*)queue_dequeue(frame->commands)
		) {
			if (command->funcs.update != NULL && !command->funcs.update(command->state)) {
				return FRAMES_STATUS_ERROR;
			}

			if (draw_now && command->funcs.draw != NULL && !command->funcs.draw(command->state)) {
				return FRAMES_STATUS_ERROR;
			}
			
			if (command->funcs.destroy != NULL) {
				command->funcs.destroy(command->state);
			}
		}

		// TODO: Profile with and without this here, to see what performance/memory usage looks like. But it might be the case that this should be here just to avoid too many operations queued in the GL at once.
		if (frame != latest_frame) {
			glFlush();
		}
		else {
			SDL_MemoryBarrierRelease();
			SDL_AtomicCASPtr((void**)&frames->latest_frame, latest_frame, NULL);
			SDL_MemoryBarrierAcquire();
			ended = frame->ended;
		}
	}

	if (ended) {
		SDL_Window* const window = app_window_get();
		assert(window != NULL);
		SDL_GL_SwapWindow(window);
		return FRAMES_STATUS_SUCCESS;
	}
	else {
		return FRAMES_STATUS_NO_END;
	}
}
